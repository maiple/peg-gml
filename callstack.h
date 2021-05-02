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
        CS_ACTIVE,
        CS_ERROR
    } m_state { CS_INACTIVE };
    std::vector<char> m_array; // size will be fixed
    volatile void* m_stack_start; // marks a 'start' point on the stack. (Might not be exactly the true base of the stack, but we need this to combat an unknown stack growth direction.)
    volatile void* m_stack_yield_at; // marks the current usage on the stack.

    void mark_stack_start() {
        char a;
        m_stack_start = &a;
    }

    void mark_stack_yield_at() {
        char a;
        m_stack_yield_at = &a;
    }

protected:
    callstack_base& operator=(const callstack_base& other)=delete;
    callstack_base& operator=(callstack_base&& other)=delete;

    std::string m_error_what;

    void verify_state_on_resume()
    {
        if (is_active()) {
            throw std::runtime_error("cannot resume stack -- already active.");
        }
        else if (is_inactive())
        {
            throw std::runtime_error("cannot resume stack -- inactive.");
        }
    }

    void verify_state_on_begin()
    {
        if (!is_inactive())
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
    callstack_base(size_t size = 8000000)
        : m_array(size) { }
    bool is_active()    const { return m_state == CS_ACTIVE; }
    bool is_suspended() const { return m_state == CS_SUSPENDED; }
    bool is_inactive()  const { return m_state == CS_INACTIVE || m_state == CS_ERROR; }
    bool is_error()  const { return m_state == CS_ERROR; }
    const char* error_what() const { return m_error_what.c_str(); }

    size_t get_stack_size() const
    { return m_array.size(); }

    size_t current_stack_depth() const
    {
        return std::abs(reinterpret_cast<intptr_t>(m_stack_start)
            - reinterpret_cast<intptr_t>(m_stack_yield_at));
    }

    // scans stack to estimate depth.
    size_t estimate_stack_depth() const
    {
        // estimate from both directions and take max.
        uintptr_t stack_depth_up = 0, stack_depth_down = 0;
        for (size_t i = 0; i < m_array.size(); ++i)
        {
            if (m_array[i] != 0)
            {
                stack_depth_up = i;
                break;
            }
        }
        for (size_t i = m_array.size(); i --> 0;)
        {
            if (m_array[i] != 0)
            {
                stack_depth_down = m_array.size() - i;
                break;
            }
        }

        return std::max(stack_depth_up, stack_depth_down);
    }
};

#ifndef EMSCRIPTEN


// default size is 8 mb
class callstack : public callstack_base
{
    static constexpr int CS_TERMINATE = 1;
    static constexpr int CS_YIELD = 2;
    static constexpr int CS_RESUME = 3;
    static constexpr int CS_CATCH = 4;
    
    jmp_buf m_env_external;
    jmp_buf m_env_internal;
    std::function<void()> m_main;
    volatile void* m_stack_base;
    
public:
    callstack(size_t size = 8000000)
        : callstack_base(size)
        , m_stack_base(stack_direction() ? (m_array.data() + m_array.size() - 1) : m_array.data())
    { }

    // start execution; pass a std::function in to execute.
    #if defined(__GNUC__)
        __attribute__((optimize("O0")))
        __attribute__((noinline))
    #endif
    #if defined(_MSC_VER)
        #pragma optimize( "", off )
        __declspec(noinline)
    #endif
    void begin(std::function<void()> fn)
    {
        m_main = fn;
        if (!m_main)
        {
            m_main = [](){};
        }

        m_state = CS_SUSPENDED;

        // run _begin, which will do all of the following:
        // - set stack pointers to the member stack.
        // - store jump buffer which will execute m_main when longjmp'd to
        // - longjmp back here and return.
        if(!setjmp(m_env_external))
        {
            _begin();
        }
    }
    #if defined(_MSC_VER)
    #pragma optimize( "", on )
    #endif

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
        case CS_CATCH:
            {
                m_state = CS_ERROR;
                return false;
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
        __attribute__((optimize("O0")))
    #elif defined(_MSC_VER)
        #pragma optimize( "", off )
        __declspec(noinline)
    #endif
    static bool stack_direction_helper(volatile int* a)
    {
        volatile int b;
        return (reinterpret_cast<uintptr_t>(&b) < reinterpret_cast<uintptr_t>(a));
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
        static callstack* volatile s_cs{ this };
        // step 1: set stack pointer registers to secondary stack ------------------------

        // macros: see https://sourceforge.net/p/predef/wiki/Architectures/
        #if defined(__GNUC__) || defined(__clang__)
            #if defined(__i386__)
                #define PEGGML_ARCH_DEFINED
                // x86
                asm volatile(
                    "movl %0, %%esp\n\t"
                    "movl %%esp, %%ebp"
                    : /* No outputs. */
                    : "rm" (m_stack_base)
                );
            #elif defined(__x86_64__)
                #define PEGGML_ARCH_DEFINED
                // x86_64
                asm volatile(
                    "movq %0, %%rsp\n\t"
                    "movq %%rsp, %%rbp"
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

            // step 2: begin function execution on secondary stack ------------------------
            s_cs->__begin();
        #endif
    }

    [[noreturn]]
     #ifdef __GNUC__
        __attribute__((noinline))
    #elif defined(_MSC_VER)
        __declspec(noinline)
    #endif
    void __begin()
    {
        if (setjmp(m_env_internal) == 0)
        {
            longjmp(m_env_external, 1);
        }
        else
        {
            bool error = false;
            try
            {
                m_main();
            }
            catch (const std::exception& e)
            {
                error = true;
                m_error_what = e.what();
            }
            catch (...)
            {
                error = true;
                m_error_what = "(unknown exception type)";
            }
            longjmp(m_env_external, error ? CS_CATCH : CS_TERMINATE);
        }
    }
    

    #if defined(_MSC_VER)
    #pragma optimize( "", on )
    #endif
};

#else

#include "emscripten/fiber.h"

class callstack : public callstack_base
{
    emscripten_fiber_t m_fiber;
    emscripten_fiber_t m_fiber_main;
    std::function<void()> m_main;

public:
    callstack(size_t size = 8000000)
        : callstack_base(size)
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
    [[noreturn]]
    void _begin()
    {
        while (true)
        {
            try
            {
                m_main();
                m_state = CS_INACTIVE;
            }
            catch (const std::exception& e)
            {
                m_error_what = e.what();
                m_state = CS_ERROR;
            }
            catch (...)
            {
                m_error_what = "(unknown exception type)";
                m_state = CS_ERROR;
            }
            m_main = [](){};
            emscripten_fiber_swap(&m_fiber, &m_fiber_main);
        }
    }
};

#endif