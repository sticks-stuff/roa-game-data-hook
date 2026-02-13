#pragma once

#include <cstdint>
#include <string>
#include <windows.h>

struct ModConfig
{
	bool debug = true;
	uint16_t port_tcp = 4242;
	uint16_t port_udp = 6500;
	uint32_t set_player_profile = 0x0093E690;
	uint32_t get_player_name = 0x0274A620;
	uint32_t get_char_info = 0x030BD860;
	uint32_t css_bg_init = 0x00B3D3D0;
	uint32_t update_char_color = 0x01D82D00;
	uint32_t local_done_char_select = 0x02F919A0;
	uint32_t take_damage = 0x01D1A5B0;
	uint32_t init_main_menu_text = 0x009DD180;
	uint32_t get_stage_dest = 0x01E3DB40;
	uint32_t init_stage_camera = 0x01E4EF50;
	uint32_t death_update = 0x007454B0;
	uint32_t game_end = 0x030D7C00;
	uint32_t draw_end_game_text = 0x01E8D710;
	uint32_t get_player_stocks = 0x00886320;
	uint32_t get_player_damage = 0x00885EB0;
	uint32_t get_player_color = 0x01E316A0;
	uint32_t is_player_active = 0x02748D00;
};

ModConfig load_config_from_module_dir(HMODULE selfModule);
