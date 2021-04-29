// include this file after defining ERROR_PREFIX
// to expose error reading functions to the abi
// example: #define ERROR_PREFIX pegggml

#include "common.h"
#include "util.h"

#define ERROR_PREFIX peggml
#ifndef ERROR_PREFIX
    #error "ERROR_PREFIX must be defined"
#endif

class glue(ERROR_PREFIX, ErrorManager)
{
public:
    std::string m_error;
    bool m_error_occurred = false;

    void _error(const std::string& message)
    {
        m_error = message;
        m_error_occurred = true;
    }

    void _clear_error()
    {
        m_error = "";
        m_error_occurred = false;
    }
};

namespace
{
    glue(ERROR_PREFIX, ErrorManager) gError;
}

#define set_error(e) gError._error(e)
#define clear_error(e) gError._clear_error(e)
#define error(v, ...) [&](){gError._error(strprintf(__VA_ARGS__)); return v;}()

// returns 1 if an error occurred, 0 otherwise.
external ty_real glue(ERROR_PREFIX, _error)()
{
    return gError.m_error_occurred ? 0.0 : 1.0;
}

// returns error string for most recent error
external ty_string glue(ERROR_PREFIX, _error_str)()
{
    return gError.m_error.c_str();
}

// sets error string
external ty_real glue(ERROR_PREFIX, _set_error)(ty_string s)
{
    gError._error(s);
    return 0;
}

// resets error
external ty_real glue(ERROR_PREFIX, _clear_error)()
{
    gError._clear_error();
    return 0;
}