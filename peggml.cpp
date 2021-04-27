#include "peglib.h"
#include "util.h"
#include "callstack.h"

#include <memory>
#include <vector>

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
		size_t handle = handle;
		if (g_parsers.size() <= handle || !g_parsers[handle])
		{
			return error(nullptr, "invalid handle %d", _handle);
		}

		return g_parsers[handle].get();
	}

	#define get_parser(lvar, handle, errval) parser* lvar = _get_parser(handle); if (!lvar) return error(errval, "invalid handle idx: %d", handle)
}

handle_t
peggml_parser_create(ty_string syntax)
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
	std::unique_ptr<parser> p(new parser(syntax));

	if (!*p)
	{
		return error(-1, "grammar syntax invalid");
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
	callstack g_parse_cs;
}

ty_real
peggml_parse_begin(handle_t handle, ty_string _text)
{
	if (g_parse_in_progress)
	{
		return error(-1, "parse already in progress.");
	}

	get_parser(p, handle, -2);

	// copy to static location for permanent access even after setjmp/longjmp
	g_parse_text = _text;

	g_parse_cs.begin([p](){
		p->parse()
	})
	
	return 0;
}