#pragma once

#include <cstdint>
#include <array>
#include <vector>
#include <csetjmp>
#include <functional>

class callstack_base {
protected:
    volatile enum {
        CS_INACTIVE,
        CS_SUSPENDED,
        CS_ACTIVE
    } m_state { CS_INACTIVE };

    callstack_base& operator=(const callstack_base& other)=delete;
    callstack_base& operator=(callstack_base&& other)=delete;

    void verify_state_on_resume()
    {
        if (m_state == CS_ACTIVE) {
            throw std::runtime_error("cannot resume stack -- already active.");
        }
        else if (m_state == CS_INACTIVE)
        {
            throw std::runtime_error("cannot resume stack -- inactive.");
        }
    }

    void verify_state_on_begin()
    {
        if (m_state != CS_INACTIVE)
        {
            throw std::runtime_error("cannot begin stackframe -- already active or suspended.");
        }
    }

    void verify_state_on_yield()
    {
        if (m_state != CS_ACTIVE)
        {
            throw std::runtime_error("cannot yield -- callstack not active.");
        }
    }

public:
    bool active()    const { return m_state == CS_ACTIVE; }
    bool suspended() const { return m_state == CS_SUSPENDED; }
    bool inactive()  const { return m_state == CS_INACTIVE; }
};

#ifndef EMSCRIPTEN

// default size is 8 mb
class callstack : callstack_base
{
    static constexpr int CS_TERMINATE = 1;
    static constexpr int CS_YIELD = 2;
    static constexpr int CS_RESUME = 3;
    
    jmp_buf m_env_external;
    jmp_buf m_env_internal;
    std::function<void()> m_main;
    std::vector<char> m_array; // size will be fixed
    volatile void* m_stack_base;

public:
    callstack(size_t size = 8000000)
        : m_array(size)
        , m_stack_base(stack_direction() ? (m_array.data() + m_array.size() - 1) : m_array.data())
    { }

    // start execution; pass a lambda in to execute.
    void begin(std::function<void()> fn)
    {
        

        m_main = fn;
        if (!m_main)
        {
            m_main = [](){};
        }

        // run _begin, which will do all of the following:
        // - set stack pointers to the member stack.
        // - store jump buffer which will execute m_main when longjmp'd to
        // - longjmp back here.
        if(setjmp(m_env_external))
        {
            // m_main will be executed when resume() is invoked.
            m_state = CS_SUSPENDED;
        }
        else
        {
            _begin();
        }
    }

    // this function blocks until internal state yields or terminates.
    // return false if execution has terminated
    // return true if execution yields
    bool resume()
    {
        verify_state_on_resume();

        m_state = CS_ACTIVE;
        switch (setjmp(m_env_external))
        {
        case CS_TERMINATE:
            {
                m_state = CS_INACTIVE;
                return false;
            }
        case CS_YIELD:
            {
                m_state = CS_SUSPENDED;
                return true;
            }
        default:
            {
                // current context is saved in setjmp,
                // so switch to its context.
                longjmp(m_env_internal, CS_RESUME);
            }
        }
    }

    void yield()
    {
        verify_state_on_yield();

        if (setjmp(m_env_internal) == 0)
        {
            // return to external
            longjmp(m_env_external, CS_YIELD);
        }
    }

private:
    // returns 0 if stack grows toward higher addresses, 1 if reversed.
    static bool stack_direction()
    {
        volatile int a;
        return stack_direction_helper(&a);
    }

    #ifdef __GNUC__
        __attribute__((noinline)) 
    #elif defined(_MSC_VER)
        __declspec(noinline)
    #endif
    static bool stack_direction_helper(volatile int* a)
    {
        volatile int b;
        return (reinterpret_cast<uintptr_t>(&b) > reinterpret_cast<uintptr_t>(a));
    }

    // helper function for begin()
    // These attributes are likely not actually necessary.
    [[noreturn]]
    #ifdef __GNUC__
        __attribute__((noinline))
        __attribute__((naked))
    #elif defined(_MSC_VER)
        __declspec(noinline)
    #endif
    void _begin() noexcept
    {
        // step 1: set stack pointer registers to secondary stack ------------------------

        // macros: see https://sourceforge.net/p/predef/wiki/Architectures/
        #if defined(__GNUC__)
            #if defined(__i386__)
                #define PEGGML_ARCH_DEFINED
                // x86
                asm volatile(
                    "movl %0, esp\n\t"
                    "movl ebp, esp"
                    : /* No outputs. */
                    : "rm" (m_stack_base)
                );
            #elif defined(__x86_64__)
                #define PEGGML_ARCH_DEFINED
                // x86_64
                asm volatile(
                    "movq %0, %%rsp\n\t"
                    "movq %%rbp, %%rsp"
                    : /* No outputs. */
                    : "rm" (m_stack_base)
                );
            #endif
        #elif defined(_MSC_VER)
            #if defined(_M_IX86)
            #define PEGGML_ARCH_DEFINED
            // MSVC doesn't support X64 inline asm
            __asm{
                mov m_stack_base, esp
                mov esp, ebp
            };
            #endif
        #endif

        #ifndef PEGGML_ARCH_DEFINED
            #error "compiler or architecture not supported. Consider adding support -- it's just two lines of asm."
        #else
            #undef PEGGML_ARCH_DEFINED
        
            // step 2: set stack pointer registers to secondary stack ------------------------
            if (setjmp(m_env_internal) == 0)
            {
                longjmp(m_env_external, 1);
            }
            else
            {
                m_main();
                longjmp(m_env_external, CS_TERMINATE);
            }
        #endif
    }
};

#else

#include "emscripten/fiber.h"

class callstack : callstack_base
{
    std::vector<char> m_array;
    emscripten_fiber_t m_fiber;
    emscripten_fiber_t m_fiber_main;
    std::function<void()> m_main;

public:
    callstack(size_t size = 8000000)
        : m_array(size)
    {
        // partition the array.
        size_t boundaries[] = {0, m_array.size() / 3, 2 * m_array.size() / 3, m_array.size()};

        // set up secondary fiber
        emscripten_fiber_init(
            &m_fiber,
            // entry function and argument
            [](void* v){ static_cast<callstack*>(v)->_begin(); }, this,
            // stack and size
            m_array.data() + boundaries[0],
            boundaries[1] - boundaries[0],
            // asyncify stack size
            m_array.data() + boundaries[1],
            boundaries[2] - boundaries[1]
        );

        // we can switch back to main fiber context from the secondary fiber's contexet.
        emscripten_fiber_init_from_current_context(
            &m_fiber_main,
            m_array.data() + boundaries[2],
            boundaries[3] - boundaries[2]
        );
    }

    // start execution; pass a lambda in to execute.
    void begin(std::function<void()> fn)
    {
        verify_state_on_begin();
        m_state = CS_SUSPENDED;

        m_main = fn;
    }

    bool resume()
    {
        verify_state_on_resume();
        m_state = CS_ACTIVE;

        emscripten_fiber_swap(&m_fiber_main, &m_fiber);
        return (m_state == CS_SUSPENDED);
    }

    void yield()
    {
        verify_state_on_yield();
        m_state = CS_SUSPENDED;

        emscripten_fiber_swap(&m_fiber, &m_fiber_main);
    }

private:
    void _begin()
    {
        m_main();
        m_state = CS_INACTIVE;
    }
};

#endif