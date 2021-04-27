#define peggml_init

gml_pragma("global", "global._peggml_loaded = false;")

if (!global._peggml_loaded)
{
    global._peggml_loaded = true
}
else
{
    // already initialized.
    exit
}

var dllName = "libpeggml.dll";
if (os_type == os_linux)
{
    dllName = "libpeggml.so"
}
var callType = dll_cdecl;
var minVersion = 1.0

global._peggml_abi_test = external_define(dllName, "peggml_abi_test", callType, ty_string,   0);

// test abi
if (global._peggml_abi_test < 0 || external_call(global._peggml_abi_test) != "gml-peglib")
{
    show_debug_message("peggml abi unable to be loaded")
    exit
}

global._peggml_version = external_define(dllName, "peggml_version", callType, ty_real,   0);
var version = external_call(global._peggml_version)
// check version min
if (version < minVersion)
{
    show_debug_message("peggml version mismatch; expected at least " + string(minVersion) + ", but shared library version is " + string(version))
    exit
}

// error declarations
global._peggml_error = external_define(dllName, "peggml_error", callType, ty_real, 0);
global._peggml_error_str = external_define(dllName, "peggml_error_str", callType, ty_string, 0);

// remaining declarations
global._peggml_parser_create = external_define(dllName, "peggml_parser_create", callType, ty_real, 1, ty_string);
global._peggml_parser_destroy = external_define(dllName, "peggml_parser_destroy", callType, ty_real, 1, ty_real);
global._peggml_parser_enable_packrat = external_define(dllName, "peggml_parser_enable_packrat", callType, ty_real, 0);

#define peggml_error
return external_call(global._peggml_error)

#define peggml_error_str
return external_call(global._peggml_error_str)

#define peggml_parser_create
peggml_init()
return external_call(global._peggml_parser_create, argument0)

#define peggml_parser_enable_packrat
return external_call(global._peggml_parser_enable_packrat)

#define peggml_parser_destroy
peggml_init()
return external_call(global._peggml_parser_destroy, argument0)