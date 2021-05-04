#pragma once

#include <cstdint>
#include <array>
#include <vector>
#include <csetjmp>
#include <functional>

#ifdef EMSCRIPTEN
#include "emscripten/fiber.h"
#endif

#ifdef __GNUC__
    #define _CALLSTACK_H_NOINLINE __attribute__((noinline))
#endif

#ifdef _MSC_VER
    #define _CALLSTACK_H_NOINLINE __declspec(noinline)
#endif

#ifndef _CALLSTACK_H_NOINLINE
    #define _CALLSTACK_H_NOINLINE
#endif

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

        return m_array.size() - std::min(m_array.size(), std::max(stack_depth_up, stack_depth_down));
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
    void begin(std::function<void()> fn);

    // this function blocks until internal state yields or terminates.
    // return false if execution has terminated
    // return true if execution yields
    bool resume();
    
    void yield();

private:
    // returns 0 if stack grows toward higher addresses, 1 if reversed.
    static bool stack_direction()
    {
        volatile int a;
        return stack_direction_helper(&a);
    }

    static bool stack_direction_helper(volatile int* a)
    {
        volatile int b;
        return (reinterpret_cast<uintptr_t>(&b) < reinterpret_cast<uintptr_t>(a));
    }

    // helper function for begin()
    // These attributes are likely not actually necessary.
    
    [[noreturn]]
    void _begin() noexcept;

    [[noreturn]]

    void __begin();
};

#else

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