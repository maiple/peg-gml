#include <mutex>

std::mutex m;

#include "peglib.h"
#include "util.h"
#include "callstack.h"

#include <memory>
#include <vector>
#include <map>
#include <cassert>

#include "peggml.h"
#define ERROR_PREFIX peggml
#include "error.h"

namespace
{
	std::string g_str_return;
}

#define STORE_STRING(s) (g_str_return = s).c_str()

// ABI test -- should return "gml-peglib"
ty_string
peggml_abi_test()
{
	return "gml-peglib";
}

// Version number
ty_real
peggml_version()
{
	return 1.1;
}

using namespace peg;

namespace
{
	std::vector<std::unique_ptr<parser>> g_parsers;

	parser* _get_parser(ty_real _handle)
	{
		size_t handle = _handle;
		if (g_parsers.size() <= handle || !g_parsers[handle])
		{
			return error(nullptr, "invalid handle %d", _handle);
		}

		return g_parsers[handle].get();
	}

	#define get_parser(lvar, handle, errval) parser* lvar = _get_parser(handle); if (!lvar) return error(errval, "invalid handle idx: %d", handle)
}

handle_t
peggml_parser_create(ty_string grammar)
{
	// find index for new parser
	size_t index = g_parsers.size();
	for (size_t i = 0; i < g_parsers.size(); ++i)
	{
		if (!g_parsers[i])
		{
			index = i;
			break;
		}
	}
	if (index == g_parsers.size())
	{
		g_parsers.emplace_back();
	}
	std::unique_ptr<parser> p(new parser());

	std::stringstream errlog;

	p->log = [&errlog](size_t line, size_t col, const std::string& msg) {
		errlog << line << ":" << col << ": " << msg << "\n";
	};

	auto ok = p->load_grammar(grammar);

	if (!ok || !*p)
	{
		std::string errstr = errlog.str();
		if (errstr.length() > 0)
		{
			return error(-1, "%s", errstr.c_str());
		}
		else
		{
			return error(-2, "grammar syntax invalid");
		}
	}
	else
	{
		g_parsers[index] = std::move(p);
		return index;
	}
}

ty_real
peggml_parser_enable_packrat(handle_t handle)
{
	get_parser(p, handle, 1);

	p->enable_packrat_parsing();

	return 0;
}

// Destroy grammar syntax
// (returns 0 on success)
ty_real
peggml_parser_destroy(handle_t _handle)
{
	size_t handle = handle;
	if (g_parsers.size() <= handle || !g_parsers[handle])
	{
		return error(1, "invalid handle %d", _handle);
	}

	g_parsers[handle].reset();

	return 0;
}

namespace
{
	bool g_parse_in_progress = false;
	std::string g_parse_text;
	std::unique_ptr<callstack> _g_parse_cs_ptr;

	// return callstack, allocating one if none exists.
	callstack& get_parse_cs()
	{
		if (!_g_parse_cs_ptr)
		{
			_g_parse_cs_ptr.reset(new callstack());
		}

		return *_g_parse_cs_ptr.get();
	}

	#define g_parse_cs get_parse_cs()

	// suspended parse context
	const SemanticValues* g_sv;
	symbol_id_t g_symbol_id;
	uint32_t g_uuid = 0;
	uuid_t g_root_uuid = -1;
}

// sets secondary callstack/fiber size
ty_real
peggml_set_stack_size(ty_real size)
{
	if (g_parse_in_progress) return error(1, "cannot set peggml stack size -- parse in progress.");
	if (size <= 0) return error(2, "peggml stack size must be positive");
	try
	{
		_g_parse_cs_ptr.reset(new callstack(static_cast<size_t>(size)));
		if (!_g_parse_cs_ptr)
		{
			return error(3, "error allocating stack");
		}
	}
	catch (...)
	{
		return error(4, "error allocating stack");
	}
	return 0;
}

ty_real
peggml_parser_set_symbol_id(handle_t handle, ty_string symbol, symbol_id_t symbol_id)
{
	if (symbol_id == 0)
	{
		return error(2, "cannot set symbol id to 0.");
	}

	if (symbol == nullptr)
	{
		return error(3, "argument string is nullptr");
	}

	get_parser(p, handle, 1);

	(*p)[symbol] = [symbol_id](const SemanticValues& sv) -> uuid_t {
		g_sv = &sv;
		// we could store 'symbol', but lazy...
		g_symbol_id = symbol_id;
		g_parse_cs.yield();
		return g_uuid++;
	};

	return 0;
}

ty_real
peggml_parse_begin(handle_t handle, ty_string _text)
{
	if (g_parse_in_progress)
	{
		return error(-1, "parse already in progress.");
	}

	// copy to static location for permanent access even after setjmp/longjmp
	g_parse_text = _text;

	get_parser(p, handle, -2);

	g_parse_cs.begin([p, text=g_parse_text.c_str()](){
		p->parse(text, g_root_uuid, nullptr);
		g_parse_in_progress = false;
	});
	
	return 0;
}

ty_real
peggml_parse_next()
{
	if (g_parse_cs.resume())
	{
		return g_symbol_id;
	}
	else
	{
		return 0;
	}
}

ty_real
peggml_parse_elt_get_uuid()
{
	return static_cast<uuid_t>(g_uuid);
}

ty_string
peggml_parse_elt_get_string()
{
	return STORE_STRING(g_sv->sv());
}

ty_real
peggml_parse_elt_get_string_offset()
{
	return g_sv->sv().data() - g_sv->ss;
}

ty_real
peggml_parse_elt_get_string_line()
{
	return g_sv->line_info().first;
}

ty_real
peggml_parse_elt_get_string_column()
{
	return g_sv->line_info().second;
}

ty_real
peggml_parse_elt_get_choice()
{
	return g_sv->choice();
}

#define RANGE_CHECK(i, container, rvalue) \
	if (i < 0 || i >= (container).size()) \
		{ return error(rvalue, "index out of bounds"); }

index_t
peggml_parse_elt_get_child_count()
{
	return g_sv->size();
}

ty_real
peggml_parse_elt_get_child_uuid(index_t _i)
{
	size_t i = _i;
	RANGE_CHECK(i, *g_sv, -1);
	return std::any_cast<uuid_t>(g_sv->at(i));
}

index_t
peggml_parse_elt_get_token_count()
{
	return g_sv->tokens.size();
}

ty_real
peggml_parse_elt_get_token_offset(index_t _i)
{
	size_t i = _i;
	RANGE_CHECK(i, g_sv->tokens, 0);
	return g_sv->token(i).data() - g_sv->ss;
}

ty_string
peggml_parse_elt_get_token_string(index_t _i)
{
	size_t i = _i;
	RANGE_CHECK(i, g_sv->tokens, "");
	return STORE_STRING(g_sv->token(i));
}

ty_real
peggml_parse_elt_get_token_number()
{
	try
	{
		return g_sv->token_to_number<ty_real>();
	}
	catch(...)
	{
		return error(0, "error occurred parsing number from token");
	}
}

external uuid_t
peggml_get_root_uuid()
{
	return g_root_uuid;
}

#ifndef PEGGML_IS_DLL

int main(int argc, char** argv)
{
	std::cout << "hello world\n";
	int handle = peggml_parser_create(R"(
		# Grammar for Calculator...
		Additive    <- Multitive '+' Additive / Multitive
		Multitive   <- Primary '*' Multitive / Primary
		Primary     <- '(' Additive ')' / Number
		Number      <- < [0-9]+ >
		%whitespace <- [ \t]*
	)");
	peggml_parser_set_symbol_id(handle, "Additive", 1);
	peggml_parser_set_symbol_id(handle, "Multitive", 2);
	peggml_parser_set_symbol_id(handle, "Number", 4);
	if (peggml_parse_begin(handle, "5 + (3 * 7) + 2"))
	{
		std::cerr << peggml_error_str() << std::endl;
		return 1;
	}
	std::map<uuid_t, int> values;
	int value;
	while (true)
	{
		switch(static_cast<int32_t>(peggml_parse_next()))
		{
		case 1:
			value = 0;
			for (size_t i = 0; i < peggml_parse_elt_get_child_count(); ++i)
			{
				uuid_t child = peggml_parse_elt_get_child_uuid(i);
				value += values[child];
			}
			break;
		case 2:
			value = 1;
			for (size_t i = 0; i < peggml_parse_elt_get_child_count(); ++i)
			{
				uuid_t child = peggml_parse_elt_get_child_uuid(i);
				value *= values[child];
			}
			break;
		case 4:
			value = peggml_parse_elt_get_token_number();
			break;
		default:
			// no more to parse.
			goto end_loop;
		}

		values[peggml_parse_elt_get_uuid()] = value;
	}
end_loop:
	std::cout << "final value is " << value << std::endl;
	return 0;
}

#endif