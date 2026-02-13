#include "GMLScriptEnv/stdafx.h"
#include "GMLScriptEnv/resources.h"
#include "GMLScriptEnv/gml.h"

namespace resources 
{
	int count(int checkFunc) 
	{
		int val = -1;
		// Call sprite_exists for every id until it returns false
		for (int i = 0; true; i++) 
		{
			GMLVar nextID = GMLVar(i);
			GMLVar* args[] = { &nextID };
			GMLVar* exists = gml_call_func(checkFunc, 1, args);
			

			if (!exists) 
			{
				break;
			}

			if (!exists->truthy()) 
			{
				val = i;
				delete exists;
				break;
			}
			delete exists;
		}

		return val;
	}
}
