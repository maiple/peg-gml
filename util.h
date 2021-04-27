# pragma once

#include <string>
#include <cstdio>
#include <stdarg.h>

// defer
// https://www.gingerbill.org/article/2015/08/19/defer-in-cpp/
template <typename F>
struct privDefer {
	F f;
	privDefer(F f) : f(f) {}
	~privDefer() { f(); }
};

template <typename F>
privDefer<F> defer_func(F f) {
	return privDefer<F>(f);
}

#define DEFER_1(x, y) x##y
#define DEFER_2(x, y) DEFER_1(x, y)
#define DEFER_3(x)    DEFER_2(x, __COUNTER__)
#define defer(code)   auto DEFER_3(_defer_) = defer_func([&](){code;})

// prints to std::string.
inline std::string strprintf(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    const size_t n = vsnprintf(nullptr, 0, fmt, args);
    va_end(args);

    #ifdef __GNUC__
        // note: this line requires GCC or C99.
        char buff[n];
    #else
        char* buff = new char[n];
        defer(delete[] buff);
    #endif

        va_start(args, fmt);
        vsprintf(buff, fmt, args);
        va_end(args);

        return buff;
}
