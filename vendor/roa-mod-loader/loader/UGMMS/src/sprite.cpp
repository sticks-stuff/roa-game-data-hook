#include "GMLScriptEnv/gml.h"
#include "GMLScriptEnv/resources.h"
#include "GMLScriptEnv/stdafx.h"
#include "GMLScriptEnv/sprite.h"

#include "loader/log.h"

namespace sprite 
{
	int spriteCount = -1;

	void __initial_setup() 
	{
		if (spriteCount != -1) return;
		func_info sprite_exists = get_func_info("sprite_exists");
		spriteCount = resources::count(sprite_exists.id);
	}

	int getSpriteCount() 
	{
		return spriteCount;
	}

	/*
	void spriteLoaderMod() 
	{
		// TODO - clean up any memory leaks here
		// Fetch function IDs
		int sprite_get_name = GMLInternals::getFunctionID("sprite_get_name");
		int sprite_add = GMLInternals::getFunctionID("sprite_add");
		int sprite_assign = GMLInternals::getFunctionID("sprite_assign");
		int sprite_get_number = GMLInternals::getFunctionID("sprite_get_number");
		int sprite_get_xoffset = GMLInternals::getFunctionID("sprite_get_xoffset");
		int sprite_get_yoffset = GMLInternals::getFunctionID("sprite_get_yoffset");
		int sprite_get_speed = GMLInternals::getFunctionID("sprite_get_speed");
		int sprite_get_speed_type = GMLInternals::getFunctionID("sprite_get_speed_type");
		int sprite_set_speed = GMLInternals::getFunctionID("sprite_set_speed");
		int file_exists = GMLInternals::getFunctionID("file_exists");
		int ini_open = GMLInternals::getFunctionID("ini_open");
		int ini_read_real = GMLInternals::getFunctionID("ini_read_real");
		int ini_close = GMLInternals::getFunctionID("ini_close");
		// Sprite speed isn't a thing in GMS1 so we need to account for that
		bool supportSpeed = (sprite_set_speed != -1);
		// Loop over every sprite ID
		for (GMLVar i = 0; i.valueInt32 < SpriteHelper::getSpriteCount(); i.valueInt32++) {
			// Get the name of the sprite
			GMLVar* argsGet[] = { &i };
			GMLVar* name = nullptr;
			GMLInternals::gml_call_func(sprite_get_name, 1, argsGet, name);
			// See if the file exists
			GMLVar fileName = GMLVar(("sprites/" + name->getString() + ".png").c_str());
			GMLVar* argsExists[] = { &fileName };
			GMLVar* exists = nullptr;
			GMLInternals::gml_call_func(file_exists, 1, argsExists, exists);

			// If the file exists...
			if (exists->truthy()) 
			{
				// Fetch sprite properties
				GMLVar* speed = NULL;
				GMLVar* speedType = NULL;
				GMLVar* xorigin = nullptr;
				GMLVar* yorigin = nullptr;
				GMLVar* number = nullptr;
				GMLVar* iniExists = nullptr;

				GMLInternals::gml_call_func(sprite_get_xoffset, 1, argsGet, xorigin);
				GMLInternals::gml_call_func(sprite_get_yoffset, 1, argsGet, yorigin);

				if (supportSpeed) 
				{
					GMLInternals::gml_call_func(sprite_get_speed, 1, argsGet, speed);
					GMLInternals::gml_call_func(sprite_get_speed_type, 1, argsGet, speedType);
				}

				GMLInternals::gml_call_func(sprite_get_number, 1, argsGet, number);

				// Look for ini
				GMLVar iniFile = GMLVar(("sprites/" + name->getString() + ".ini").c_str());
				GMLVar* argsIni[] = { &iniFile };

				GMLInternals::gml_call_func(file_exists, 1, argsIni, iniExists);

				// If the ini exists...
				if (iniExists->truthy()) 
				{
					// Open it
					GMLInternals::gml_call_func(ini_open, 1, argsIni);
					// Store original properties to delete
					GMLVar* speedOld = NULL;
					GMLVar* speedTypeOld = NULL;
					GMLVar* xoriginOld = xorigin;
					GMLVar* yoriginOld = yorigin;

					if (supportSpeed) {
						speedOld = speed;
						speedTypeOld = speedType;
					}

					GMLVar* numberOld = number;
					// Read from the ini
					GMLVar sprite_str("string");
					GMLVar xoffset_str("xoffset");
					GMLVar yoffset_str("yoffset");
					GMLVar speedType_str("speedType");
					GMLVar speed_str("speed");
					GMLVar frames_str("frames");
					GMLVar* argsIniXoffset[] = { &sprite_str, &xoffset_str, xoriginOld };
					GMLInternals::gml_call_func(ini_read_real, 3, argsIniXoffset, xorigin);
					GMLVar* argsIniYoffset[] = { &sprite_str, &yoffset_str, yoriginOld };
					GMLInternals::gml_call_func(ini_read_real, 3, argsIniYoffset, yorigin);
					if (supportSpeed) 
					{
						GMLVar* argsIniSpeed[] = { &sprite_str, &speed_str, speedOld };
						GMLInternals::gml_call_func(ini_read_real, 3, argsIniSpeed, speed);
						GMLVar* argsIniSpeedType[] = { &sprite_str, &speedType_str, speedTypeOld };
						GMLInternals::gml_call_func(ini_read_real, 3, argsIniSpeedType, speedType);
					}
					GMLVar* argsIniFrames[] = { &sprite_str, &frames_str, numberOld };
					GMLInternals::gml_call_func(ini_read_real, 3, argsIniFrames, number);
					// Close the ini
					GMLInternals::gml_call_func(ini_close, 0, NULL);
					// Clean old properties
					delete xoriginOld;
					delete yoriginOld;
					if (supportSpeed) 
					{
						delete speedOld;
						delete speedTypeOld;
					}
					delete numberOld;
				}
				// Clean up ini stuff
				iniFile.freeValue();
				delete iniExists;

				// Load new sprite
				GMLVar zero(0);
				GMLVar* argsSpriteAdd[] = { &fileName, number, &zero, &zero, xorigin, yorigin };
				GMLVar* newSprite = nullptr;
				GMLInternals::gml_call_func(sprite_add, 6, argsSpriteAdd, newSprite);
				if (newSprite->getReal() > 0) 
				{
					// Set final properties
					if (supportSpeed) 
					{
						GMLVar* argsSetSpeed[] = { newSprite, speed, speedType };
						GMLInternals::gml_call_func(sprite_set_speed, 3, argsSetSpeed);
					}
					// Replace the old sprite!
					GMLVar* argsSpriteAssign[] = { &i, newSprite };
					GMLInternals::gml_call_func(sprite_assign, 2, argsSpriteAssign);
				}
				else 
				{
					// Failed to load :(
					loader_log_error("Failed to load the sprite {}", name->getString());
				}

				// Clean up
				delete number;
				delete xorigin;
				delete yorigin;
				if (supportSpeed) {
					delete speed;
					delete speedType;
				}
				delete newSprite;
			}

			// Clean up
			name->valueString->refCountGML -= 1;
			fileName.freeValue();
			delete name;
			delete exists;
		}
	}
	*/
}
