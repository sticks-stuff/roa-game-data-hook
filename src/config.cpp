#include "config.h"

#include <filesystem>
#include <optional>

#include <yaml-cpp/yaml.h>

static std::optional<uint32_t> parse_u32(const YAML::Node& n)
{
	if (!n || !n.IsScalar())
		return std::nullopt;

	try
	{
		return n.as<uint32_t>();
	}
	catch (...)
	{
	}

	try
	{
		auto s = n.as<std::string>();
		s.erase(0, s.find_first_not_of(" \t\r\n"));
		s.erase(s.find_last_not_of(" \t\r\n") + 1);
		if (s.empty())
			return std::nullopt;
		return static_cast<uint32_t>(std::stoul(s, nullptr, 0));
	}
	catch (...)
	{
		return std::nullopt;
	}
}

static std::optional<uint16_t> parse_port_u16(const YAML::Node& n)
{
	auto v = parse_u32(n);
	if (!v)
		return std::nullopt;
	if (*v == 0 || *v > 65535)
		return std::nullopt;
	return static_cast<uint16_t>(*v);
}

static std::filesystem::path module_path(HMODULE selfModule)
{
	char pathBuf[MAX_PATH] = {0};
	DWORD len = GetModuleFileNameA(selfModule, pathBuf, MAX_PATH);
	if (len == 0)
		return std::filesystem::current_path();
	return std::filesystem::path(pathBuf);
}

ModConfig load_config_from_module_dir(HMODULE selfModule)
{
	ModConfig cfg;
	try
	{
		auto modPath = module_path(selfModule);
		auto modDir = modPath.parent_path();
		auto modStem = modPath.stem().string();

		auto cfgPath = modDir / modStem / "config.yaml";
		if (!std::filesystem::exists(cfgPath))
			cfgPath = modDir / (modStem + ".yaml");
		if (!std::filesystem::exists(cfgPath))
			cfgPath = modDir / "config.yaml";

		if (!std::filesystem::exists(cfgPath))
			return cfg;

		YAML::Node root = YAML::LoadFile(cfgPath.string());
		if (root["debug"]) cfg.debug = root["debug"].as<bool>();
		if (auto v = parse_port_u16(root["port_tcp"])) cfg.port_tcp = *v;
		if (auto v = parse_port_u16(root["port_udp"])) cfg.port_udp = *v;
		if (auto v = parse_u32(root["set_player_profile"])) cfg.set_player_profile = *v;
		if (auto v = parse_u32(root["get_player_name"])) cfg.get_player_name = *v;
		if (auto v = parse_u32(root["get_char_info"])) cfg.get_char_info = *v;
		if (auto v = parse_u32(root["css_bg_init"])) cfg.css_bg_init = *v;
		if (auto v = parse_u32(root["update_char_color"])) cfg.update_char_color = *v;
		if (auto v = parse_u32(root["local_done_char_select"])) cfg.local_done_char_select = *v;
		if (auto v = parse_u32(root["take_damage"])) cfg.take_damage = *v;
		if (auto v = parse_u32(root["init_main_menu_text"])) cfg.init_main_menu_text = *v;
		if (auto v = parse_u32(root["get_stage_dest"])) cfg.get_stage_dest = *v;
		if (auto v = parse_u32(root["init_stage_camera"])) cfg.init_stage_camera = *v;
		if (auto v = parse_u32(root["death_update"])) cfg.death_update = *v;
		if (auto v = parse_u32(root["game_end"])) cfg.game_end = *v;
		if (auto v = parse_u32(root["draw_end_game_text"])) cfg.draw_end_game_text = *v;
		if (auto v = parse_u32(root["get_player_stocks"])) cfg.get_player_stocks = *v;
		if (auto v = parse_u32(root["get_player_damage"])) cfg.get_player_damage = *v;
		if (auto v = parse_u32(root["get_player_color"])) cfg.get_player_color = *v;
		if (auto v = parse_u32(root["is_player_active"])) cfg.is_player_active = *v;
	}
	catch (...)
	{
	}
	return cfg;
}
