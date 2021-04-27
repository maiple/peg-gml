// defined in peggml

global._peggml_loaded = false

var parser = peggml_parser_create("
    # Grammar for Calculator...
    Additive    <- Multitive '+' Additive / Multitive
    Multitive   <- Primary '*' Multitive / Primary
    Primary     <- '(' Additive ')' / Number
    Number      <- < [0-9]+ >
    %whitespace <- [ \t]*
")

if (parser < 0)
{
    show_debug_message(peggml_error_str())
    exit
}

peggml_parser_destroy(parser)