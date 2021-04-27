#include "common.h"

typedef ty_real handle_t;

// ABI test -- should return "gml-peglib"
external ty_string
peggml_abi_test();

// Version number
external ty_real
peggml_version();

// Create new parser for the given grammar syntax
// see [https://github.com/yhirose/cpp-peglib#cpp-peglib] for syntax
// returns its handle, or -1 on failure
external handle_t
peggml_parser_create(ty_string);

// enable packrat parsing
external ty_real
peggml_parser_enable_packrat(handle_t);

// Destroy grammar syntax
// (returns 0 on success)
external ty_real
peggml_parser_destroy(handle_t);

// starts parsing the given string
external ty_real
peggml_parse_begin(handle_t, ty_string);

// returns 1 if a new element is being parsed, 0 if parsing has completed.
external ty_real
peggml_parse_next(handle_t, ty_string);

external ty_real
peggml_parse_elt_get_uuid(handle_t, ty_string);

external 