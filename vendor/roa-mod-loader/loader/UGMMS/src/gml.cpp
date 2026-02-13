#include "GMLScriptEnv/gml.h"

#include "GMLScriptEnv/stdafx.h"
#include "GMLScriptEnv/search.h"

#include <vector>
#include <unordered_map>

CRoomInternal& CRoom::GetMembers()
{
	size_t bg_color_idx = get_builtin_variable_index("background_color");


	// This lookup will fail in newer runners where backgrounds were removed and return -1
	if (bg_color_idx != -1)
	{
		// Note: We have to craft the pointer manually here, since
		// bool alignment prevents us from just having a struct (it'd get aligned to sizeof(PVOID)).

		// Don't ask why it's from m_Color and not from m_ShowColor, it doesn't make sense
		// and I can't figure out why it works - it just does.
		return *reinterpret_cast<CRoomInternal*>(&this->m_Color);
	}

	return this->WithBackgrounds.Internals;
};

// Map of function name -> id
std::unordered_map<std::string, func_info>* functionIDMap = new std::unordered_map<std::string, func_info>();

// Function to call built-in GML functions
GMLVar* (*GMLLegacyCall)(GMLInstance* instSelf, GMLInstance* instOther, GMLVar& gmlResult, int argumentNumber, int functionID, GMLVar** arguments) = NULL;

//////////////////////////////////////////////////////////////////////////////
// SETUP /////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

// Sets up functionIDMap
std::string __find_function_map(void* ptr)
{
	// Observing a near empty compiled GM project, each function definition is stored as:
	// 64 uint8_ts: function name (remaining uint8_ts are 0'd out)
	// 4 uint8_ts: address of function?
	// 1 uint8_t: argument count?
	// 3 uint8_ts: padding (0)
	// 1 uint8_t: unknown
	// 3 uint8_ts: padding (0)
	// 4 uint8_ts of 0xFF
	// For a total of 80 uint8_ts each
	// However, the size is not consistent in some games
	// Function list ends when a zero length name is hit.
	uint8_t* funcArray = *(uint8_t**)ptr;

	// Automatically determine size of a function entry
	// Different between test project and Rivals of Aether, can't be hardcoded
	std::vector<uint8_t> entryTerminator{ 0xFF, 0xFF, 0xFF, 0xFF };
	void* ptrEntryTerminator = __impl_scan_local(entryTerminator, funcArray, 120);
	if (ptrEntryTerminator == NULL)
	{
		// Failed to find entry terminator
		return "Malformed function list";
	}
	int entrySize = (int)((uint8_t*)ptrEntryTerminator + 4 - funcArray);

	uint32_t index = 0; // Current index of function being read
	while (true)
	{
		uint8_t* entry = funcArray + entrySize * index;

		// Read the next name
		const char* name = (const char*)(entry);
		if (strlen(name) == 0)
		{
			// No name - End of function list
			break;
		}
		else
		{
			// Add the name to the function map
			functionIDMap->insert(
				{
					std::string(name),
					func_info
					{
						index,
						*(uint32_t**)(entry + 0x40)
					}
				});
			index += 1;
		}
	}

	return "";
}

std::string __gml_setup()
{
	// Returns a string if loading failed
	if (GMLLegacyCall != NULL) return "";

	////// Find address of callLegacyFunction
	// Contains the ASM of the function with inconsistent bits replaced with '?'
	std::vector<uint8_t> binLegacyCallFirst
	{
				0x55, 0x8B, 0xEC, 0x6A, 0xFF, '?', '?', '?', '?', '?', 0x64, 0xA1, 0x00, 0x00, 0x00, 0x00,
				0x50, 0x83, 0xEC, 0x14, 0xA1, '?', '?', '?', '?', 0x33, 0xC5, 0x89, 0x45, 0xF0, 0x53, 0x56,
				0x57, 0x50, 0x8D, 0x45, 0xF4, 0x64, 0xA3, 0x00, 0x00, 0x00, 0x00, '?', 0x45, 0x18
	};

	// Search for the function
	void* legacyCallAddr = __impl_scan(binLegacyCallFirst);
	if (legacyCallAddr == NULL)
	{
		// Failed to find it
		return "Failed to find CallLegacyFunction";
	}
	GMLLegacyCall = (GMLVar * (*)(GMLInstance*, GMLInstance*, GMLVar&, int, int, GMLVar**))legacyCallAddr;

	////// Search for second ASM chunk containing pointer to pointer to function array
	// '?'d out bit here is the pointer we need
	std::vector<uint8_t> binLegacyCallSecond
	{
		0x03, 0x05, '?', '?', '?', '?', 0x89, 0x45,
		0xE4, 0x8B, 0xC7, 0xC1, 0xE0, 0x04, 0xE8
	};

	// Search for it starting from the end of the first chunk
	void* legacyCallChunkAddr = __impl_scan_local(binLegacyCallSecond, (uint8_t*)legacyCallAddr + binLegacyCallFirst.size(), 64);
	if (GMLLegacyCall == nullptr) {
		// Failed to find second halfcallGMLFunction
		GMLLegacyCall = NULL;
		return "Malformed CallLegacyFunction";
	}

	// Pull the address from the ASM
	void* funcArrayAddrBase = __impl_read_ptr((uint8_t*)legacyCallChunkAddr + 2);
	// Initialize function array
	std::string result = __find_function_map(funcArrayAddrBase);
	if (result != "")
	{
		// Failed to load function list
		GMLLegacyCall = NULL;
		return result;
	}

	// set up builtin vars array
	std::vector<uint8_t> jnz_target_pattern =
	{
		0xa1, '?', '?', '?', '?',     // eax, [memory]
		0x3d, 0xf4, 0x01, 0x00, 0x00, // cmp eax, 0x1f4
		0x75, 0x0e,                   // jnz short 0x????
	};

	void* ptr = __impl_scan(jnz_target_pattern);

	if (!ptr)
	{
		return "failed to find builtin variable array";
	}

	// relative address operand of the closest jnz instruction
	uint8_t offset = *((int8_t*)ptr + 11);
	uint16_t* jump_target = (uint16_t*)ptr + offset;

	std::vector<uint8_t> lea_pattern =
	{
		0x8d, 0xb8, '?', '?', '?', '?', // lea edi, [memory]
		0x8b, '?'                      // mov eax, s8
	};

	std::vector<uint8_t> mov_pattern =
	{
		0xa1, '?', '?', '?', '?', // mov eax, [memory]
		0x83, 0xc4, 0x04          // add esp, 0x4
	};

	uint8_t* nearest_lea = ((uint8_t*)__impl_scan_local(lea_pattern, jump_target, 0x40)) + 2;
	uint8_t* nearest_mov = ((uint8_t*)__impl_scan_local(mov_pattern, jump_target, 0x40)) + 1;

	builtin_array = reinterpret_cast<RVariableRoutine*>(*(uint32_t*)nearest_lea);
	builtin_count = reinterpret_cast<int32_t*>(*(uint32_t*)nearest_mov);

	return "";
}

//////////////////////////////////////////////////////////////////////////////
// METHODS ///////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

func_info get_func_info(std::string name)
{
	auto t = functionIDMap->find(name);
	if (t == functionIDMap->end()) return {};
	return t->second;
}

func_info get_func_info(const char* name)
{
	return get_func_info(std::string(name));
}

GMLVar* GMLRetDummy = new GMLVar();
GMLVar* gml_call_func(int functionID, int argCount, GMLVar** args)
{
	if (GMLLegacyCall == NULL) return NULL;

	GMLVar* out = new GMLVar();

	GMLLegacyCall(NULL, NULL, *out, argCount, functionID, args);

	return out;
}

size_t get_builtin_variable_index(const char* name)
{
	for (int i = 0; i < *builtin_count; i++)
	{
		if (!strcmp(name, builtin_array[i].m_Name))
		{
			return i;
		}
	}
	return -1;
}
