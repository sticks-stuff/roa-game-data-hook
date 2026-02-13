#include "GMLScriptEnv/stdafx.h"
#include "GMLScriptEnv/gml.h"
#include "GMLScriptEnv/room.h"
#include "GMLScriptEnv/resources.h"
#include "GMLScriptEnv/search.h"

#include <vector>

namespace room 
{
	using func_roomdata = CRoom*(*)(int index);

	func_roomdata room_data = nullptr;

	int roomCount = -1;

	void __setup() 
	{
		__impl_find_room_data();

		if (roomCount != -1) return;
		func_info room_exists = get_func_info("room_exists");
		roomCount = resources::count(room_exists.id);
	}

	int __impl_get_room_count() 
	{
		return roomCount;
	}

	void* __impl_find_room_data()
	{
		func_info room_instance_clear = get_func_info("room_instance_clear");

		std::vector<uint8_t> pattern =
		{
			0xe8, '?', '?', '?', '?',	// call <room_data>
			0x83, 0xc4, 0x0c,			// add esp, 0xc
			0x85, 0xc0,					// test eax, eax
		};

		void* ptr = __impl_scan_local(pattern, room_instance_clear.ptr, 0x80);
		
		if (ptr)
		{
			// found the call instruction, now get the absolute address of room_data
			room_data = (func_roomdata) __impl_absolute_address(ptr, *((uint32_t**)((uint8_t*)ptr + 0x01)), 0x05);
		}

		return room_data;
	}

	CRoom* __impl_get_room_by_index(uint32_t index)
	{
		return room_data(index);
	}
}
