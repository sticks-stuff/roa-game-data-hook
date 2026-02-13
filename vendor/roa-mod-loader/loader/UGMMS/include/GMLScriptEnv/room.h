#pragma once

#include "gml.h"

namespace room 
{
	void __setup();

	int __impl_get_room_count();
	void* __impl_find_room_data();
	CRoom* __impl_get_room_by_index(uint32_t index);
}
