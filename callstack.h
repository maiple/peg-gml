#pragma once

#include <cstdint>
#include <array>
#include <vector>
#include <csetjmp>
#include <functional>

// default size is 4 mb
template<int Size=4000000>
class callstack
{
    volatile enum {
        CS_INACTIVE,
        CS_SUSPENDED,
        CS_ACTIVE
    } m_state { CS_INACTIVE };
    std::vector<char> m_transfer_in;
    std::vector<char> m_transfer_out;
    jmp_buf m_env_external;
    jmp_buf m_env_internal;
    std::function<void()> m_main;
    volatile void* m_stack_base;
    std::array<char, Size> m_array;

    static constexpr int CS_TERMINATE = 1;
    static constexpr int CS_YIELD = 2;
    static constexpr int CS_RESUME = 3;

public:
    callstack()
        : m_stack_base(stack_direction() ? (m_array.data() + Size) : m_array.data())
    {}

    callstack& operator=(const callstack& other)=delete;
    callstack& operator=(callstack&& other)=delete;

    bool active()    const { return m_state == CS_ACTIVE; }
    bool suspended() const { return m_state == CS_SUSPENDED; }
    bool inactive()  const { return m_state == CS_INACTIVE; }

    // start execution; pass a lambda in to execute.
    void begin(std::function<void()> fn)
    {
        if (m_state != CS_INACTIVE)
        {
            throw std::runtime_error("cannot begin stackframe -- already active or suspended.");
        }

        m_main = fn;
        if (!m_main)
        {
            m_main = [](){};
        }

        if (setjmp(m_env_external))
        {
            m_state = CS_SUSPENDED;
            return;
        }
        else
        {
            // we have to jump to a function in progress on the internal stack.
            m_main();
        }
    }

    // this function blocks until internal state yields or terminates.
    // return false if execution has terminated
    // return true if execution yields
    // sets provided object to yielded value *if yielded*
    // internal state receives object y
    template<typename T, typename Y>
    bool resume(T& out_t, Y&& y)
    {
        if (m_state == CS_ACTIVE) {
            throw std::runtime_error("cannot resume stack -- already active.");
        }
        else if (m_state == CS_INACTIVE)
        {
            throw std::runtime_error("cannot resume stack -- inactive.");
        }
        else
        {
            // set resuming value
            m_transfer_in.resize(sizeof(Y));
            new (m_transfer_in.data()) Y{ std::move(y) };

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
                    if (m_transfer_out.size() != sizeof(T))
                    {
                        throw std::runtime_error("yielded type is different size than expected");
                    }
                    out_t = std::move(*static_cast<T>(static_cast<void*>(m_transfer_out.data())));
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
    }

    template<typename Y, typename T>
    Y yield(T&& t)
    {
        if (m_state != CS_ACTIVE)
        {
            throw std::runtime_error("cannot yield -- callstack not active.");
        }

        // transfer t out
        this->m_transfer_out.resize(sizeof(T));
        new (this->m_transfer_out.data()) T{ std::move(t) };

        if (setjmp(m_env_internal) == 0)
        {
            // return to external
            longjmp(m_env_external, CS_YIELD);
        }

        // receive y
        if (m_transfer_out.size() != sizeof(Y))
        {
            throw std::runtime_error("resumed type is different size than expected");
        }
        return std::move(*static_cast<Y>(static_cast<void*>(m_transfer_out.data())));
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

    #ifdef __GNUC__
        __attribute__((noinline))
        __attribute__((naked))
    #elif defined(_MSC_VER)
        __declspec(noinline)
        __declspec(naked)
    #endif
    [[noreturn]] void _begin() noexcept
    {
        asm("movl %%esp, %0" : "=r"(m_stack_base) :);
        asm volatile(
            "movl %ebp, %esp"
        );

        if (setjmp(m_env_internal) == 0)
        {
            longjmp(m_env_external, 1);
        }
        else
        {
            m_main();
            longjmp(m_env_external, CS_TERMINATE);
        }
    }
};