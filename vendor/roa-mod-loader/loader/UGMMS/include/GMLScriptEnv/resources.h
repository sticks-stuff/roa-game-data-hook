#pragma once

namespace resources 
{
	// Used to calculate the built-in number of a resource
	// Takes a GML function id used to check whether an ID exists (like object_exists)
	int count(int checkFunc);
}