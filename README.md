# PEG-GML

*Parsing Expression Grammars for GML (port of [cpp-peglib](https://github.com/yhirose/cpp-peglib))*

Instead of writing your own parser in GML for whatever it is you want to parse, you can use PEG-GML to create a parser for you. Simply write a [PEG](https://en.wikipedia.org/wiki/Parsing_expression_grammar) describing the syntax of whatever it is you want to parse, then write handler functions for each symbol to describe how to convert it into an actual value.

## Example 
Parsing arithmetic statements containing +, \*, and parentheses:

```gml
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
// "result is: 28"

// clean up
peggml_parser_destroy(parser)

// handlers ----------------------------------------
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
// interpret token as number (provided as convenience)
return peggml_parse_elt_get_token_number()
// (could also do real(peggml_parse_elt_get_token_string()))
```

## Installation

Simply add the script [peggml.gml](Scripts/peggml.gml) to your projects's scripts, and all the [datafiles](datafiles/) to your datafiles.