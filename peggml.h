#include "common.h"

typedef ty_real handle_t;
typedef ty_real symbol_id_t;
typedef ty_real index_t;
typedef ty_real uuid_t;


// ABI test -- should return "gml-peglib"
external ty_string
peggml_abi_test();

// Version number (can use this to compare header to compiled binary)
external ty_real
peggml_version();
#define PEGGML_VERSION 1.2

/// sets parsing stack size in bytes. (optional. Defaults to 8 mb.)
external ty_real
peggml_set_stack_size(ty_real size);

external ty_real
peggml_get_stack_size();

// how much of the stack depth is currently in use (in bytes)
external ty_real
peggml_stack_current_depth();

// estimates the maximum depth reached so far in the parsing stack (in bytes)
// (this can take a while, as it scans the memory.)
external ty_real
peggml_estimate_stack_usage();

// Create new parser for the given grammar syntax
// see [https://github.com/yhirose/cpp-peglib#cpp-peglib] for syntax
// returns its handle, or -1 on failure
external handle_t
peggml_parser_create(ty_string);

// Destroy grammar syntax
// (returns 0 on success)
external ty_real
peggml_parser_destroy(handle_t);

// enable packrat parsing
external ty_real
peggml_parser_enable_packrat(handle_t);

// define a nonzero symbol id for a symbol
// this will be returned from peggml_parse_next().
// if this is not invoked, the symbol will not be handlable.
external ty_real
peggml_parser_set_symbol_id(handle_t, ty_string, symbol_id_t);

// starts parsing the given string
external ty_real
peggml_parse_begin(handle_t, ty_string);

// returns symbol id if a new element is being parsed, 0 if parsing has completed.
external ty_real
peggml_parse_next();

external uuid_t
peggml_parse_elt_get_uuid();

external ty_string
peggml_parse_elt_get_string();

external ty_real
peggml_parse_elt_get_string_offset();

external ty_real
peggml_parse_elt_get_string_line();

external ty_real
peggml_parse_elt_get_string_column();

external ty_real
peggml_parse_elt_get_choice();

external index_t
peggml_parse_elt_get_child_count();

external uuid_t
peggml_parse_elt_get_child_uuid(index_t);

external index_t
peggml_parse_elt_get_token_count();

external ty_real
peggml_parse_elt_get_token_offset(index_t);

external ty_string
peggml_parse_elt_get_token_string(index_t);

// retrieves token 0 as a number
external ty_real
peggml_parse_elt_get_token_number();

// retrieves root uuid of most recent parse
external uuid_t
peggml_get_root_uuid();