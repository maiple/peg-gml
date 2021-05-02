/// this function runs if run via opengml.

global._peggml_loaded = false

var parser = peggml_parser_create("
    # Grammar for Calculator...
    Additive    <- Multitive '+' Additive / Multitive
    Multitive   <- Primary '*' Multitive / Primary
    Primary     <- '(' Additive ')' / Number
    Number      <- < [0-9]+ >
    %whitespace <- [ \t]*
")

peggml_parser_set_handler(parser, "Additive", handle_additive)
peggml_parser_set_handler(parser, "Multitive", handle_multitive)
peggml_parser_set_handler(parser, "Number", handle_number)

var result = peggml_parse(parser, "5 + (3 *7)   + 2")

show_debug_message("result is: " + string(result))

peggml_parser_destroy(parser)

#define handle_additive
// add all operands together
var value = 0;
for (var i = 0; i < peggml_parse_elt_get_child_count(); ++i)
{
    value += peggml_parse_elt_get_child_value(i);
}

return value

#define handle_multitive
// multiply all operands together
var value = 1;
for (var i = 0; i < peggml_parse_elt_get_child_count(); ++i)
{
    value *= peggml_parse_elt_get_child_value(i);
}

return value

#define handle_number
// interpret token as number
return peggml_parse_elt_get_token_number()