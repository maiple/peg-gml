#ifndef EMSCRIPTEN

#include "callstack.h"

#ifdef __GNUC__
    #pragma GCC optimize ("O0")
#endif

#ifdef _MSC_VER
    #pragma optimize( "", off )
#endif

void callstack::begin(std::function<void()> fn)
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

bool callstack::resume()
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

void callstack::yield()
{
    verify_state_on_yield();

    if (setjmp(m_env_internal) == 0)
    {
        // return to external
        longjmp(m_env_external, CS_YIELD);
    }
}

_CALLSTACK_H_NOINLINE
void callstack::_begin() noexcept
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

_CALLSTACK_H_NOINLINE
void callstack::__begin()
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
#endif