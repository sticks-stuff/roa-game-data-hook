#pragma once

namespace sprite 
{
	void __setup();
	// Loads sprite replacements from a "sprites" subdirectory
	void spriteLoaderMod();
	// Returns the number of built-in sprites the game has
	int getSpriteCount();
}