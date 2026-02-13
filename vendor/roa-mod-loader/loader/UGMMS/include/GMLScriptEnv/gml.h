#pragma once
#include <string>

#include "gml_types.h"
#include <string>

typedef GMLVar& (*GMLScriptPtr)(GMLInstance* self, GMLInstance* other, GMLVar& out, int argCount, GMLVar** args);

struct func_info
{
	uint32_t id;
	void* ptr;
};

std::string __gml_setup();

func_info get_func_info(std::string name);
func_info get_func_info(const char* name);

static RVariableRoutine* builtin_array = nullptr;
static int32_t* builtin_count = nullptr;

// Call a built-in GM function using its numerical ID
// Arguments are passed by array
GMLVar* gml_call_func(int functionID, int argCount, GMLVar** args);
size_t get_builtin_variable_index(const char* name);
//GMLScriptPtr getScriptPtr(int scriptID);

