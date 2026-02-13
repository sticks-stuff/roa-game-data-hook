#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

#include <windows.h>

#include <loader/hook.h>
#include <loader/log.h>
#include <loader/memory.h>

#include <GMLScriptEnv/gml.h>

#include <cstdint>
#include <atomic>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#include "config.h"

using ScriptFn = PFUNC_YYGMLScript;

static ModConfig g_cfg;

static ScriptFn g_orig_set_player_profile = nullptr;
static ScriptFn g_get_player_name = nullptr;
static ScriptFn g_get_char_info = nullptr;
static ScriptFn g_orig_css_bg_init = nullptr;
static ScriptFn g_orig_update_char_color = nullptr;
static ScriptFn g_orig_local_done_char_select = nullptr;
static ScriptFn g_orig_take_damage = nullptr;
static ScriptFn g_orig_init_main_menu_text = nullptr;
static ScriptFn g_orig_get_stage_dest = nullptr;
static ScriptFn g_orig_init_stage_camera = nullptr;
static ScriptFn g_orig_death_update = nullptr;
static ScriptFn g_orig_game_end = nullptr;
static ScriptFn g_orig_draw_end_game_text = nullptr;
static ScriptFn g_get_player_stocks = nullptr;
static ScriptFn g_get_player_damage = nullptr;
static ScriptFn g_get_player_color = nullptr;
static ScriptFn g_is_player_active = nullptr;

static GMLVar characters[4] = {};

static GMLVar* get_char_ids()
{
	return characters;
}

static void (*char_select_orig)() = nullptr;
static void __declspec(naked) char_select_detour() // credit to Rai for this (https://discord.com/channels/819715327893045288/819715327893045292/1468815350438039562)
{
	__asm
	{
		// edx is port id (1-4)
		// eax is the pointer to port's character id as an RValue
		push edx
		push ebx
		sub esp, 16
		movdqu xmmword ptr [esp], xmm0

		// these shouldnt be true but just incase
		cmp edx, 0x4
		jnle char_select_detour_exit
		cmp edx, 0x0
		je char_select_detour_exit

		dec edx
		shl edx, 4
		lea ebx, characters
		add ebx, edx

		movdqu xmm0, xmmword ptr[esi]
		movsd [ebx] GMLVar.valueReal, xmm0
		mov edx, [esi + 0xc]
		mov [ebx] GMLVar.type, edx

	char_select_detour_exit:
		movdqu xmm0, xmmword ptr[esp]
		add esp, 16
		pop ebx
		pop edx
		jmp char_select_orig
	}
}

enum class NetState
{
	menu,
	stage_select,
	character_select,
	in_game,
	game_end,
};

struct NetPlayer
{
	std::string name;
	int stocks = -1;
	int damage = -1;
	int char_id = -1;
	int alt = -1;
	bool active = false;
};

static std::mutex g_net_mutex;
static NetState g_net_state = NetState::menu;
static bool g_net_is_results_screen = false;
static int g_net_stage = -1;
static NetPlayer g_net_players[4];

static std::atomic<bool> g_net_stop{false};
static std::atomic<bool> g_net_has_client{false};
static HANDLE g_net_thread = nullptr;

static int g_last_stage_id = -1;

static bool is_numeric_type(int type)
{
	return type == GML_TYPE_REAL || type == GML_TYPE_INT32 || type == GML_TYPE_INT64 || type == GML_TYPE_BOOL;
}

static const char* net_state_name(NetState s)
{
	switch (s)
	{
	case NetState::menu: return "menu";
	case NetState::stage_select: return "stage_select";
	case NetState::character_select: return "character_select";
	case NetState::in_game: return "in_game";
	case NetState::game_end: return "game_end";
	default: return "menu";
	}
}

static size_t readable_pointer_capacity(void* p, size_t elemSize, size_t maxElems)
{
	if (!p || elemSize == 0)
		return 0;

	MEMORY_BASIC_INFORMATION mbi{};
	if (VirtualQuery(p, &mbi, sizeof(mbi)) == 0)
		return 0;
	if (mbi.State != MEM_COMMIT)
		return 0;

	DWORD protect = mbi.Protect & 0xff;
	bool readable =
		protect == PAGE_READONLY ||
		protect == PAGE_READWRITE ||
		protect == PAGE_WRITECOPY ||
		protect == PAGE_EXECUTE_READ ||
		protect == PAGE_EXECUTE_READWRITE ||
		protect == PAGE_EXECUTE_WRITECOPY;
	if (!readable)
		return 0;

	uintptr_t regionBase = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
	uintptr_t addr = reinterpret_cast<uintptr_t>(p);
	if (addr < regionBase)
		return 0;
	uintptr_t offset = addr - regionBase;
	if (offset >= mbi.RegionSize)
		return 0;

	size_t bytes = static_cast<size_t>(mbi.RegionSize - offset);
	size_t cap = bytes / elemSize;
	if (cap > maxElems)
		cap = maxElems;
	return cap;
}

static std::string try_read_gml_string(GMLVar& v)
{
	if (v.type != GML_TYPE_STRING)
		return {};

	GMLStringRef* sref = v.valueString;
	if (readable_pointer_capacity(sref, sizeof(GMLStringRef), 1) == 0)
		return {};

	const char* cstr = sref->string;
	size_t ccap = readable_pointer_capacity((void*)cstr, sizeof(char), 256);
	if (ccap == 0)
		return {};

	char buf[256] = {0};
	size_t n = ccap;
	if (n > 255) n = 255;
	memcpy(buf, cstr, n);
	buf[255] = 0;
	return std::string(buf);
}

static void net_init_defaults_locked()
{
	g_net_state = NetState::menu;
	g_net_is_results_screen = false;
	g_net_stage = -1;
	for (int i = 0; i < 4; ++i)
	{
		g_net_players[i].name = "P" + std::to_string(i + 1);
		g_net_players[i].stocks = -1;
		g_net_players[i].damage = -1;
		g_net_players[i].char_id = -1;
		g_net_players[i].alt = -1;
		g_net_players[i].active = false;
	}
}

static int call_get_player_color(CInstance* self, CInstance* other, int playerId)
{
	if (!g_get_player_color)
		return -1;

	GMLVar out;
	GMLVar arg0;
	arg0.setReal(static_cast<double>(playerId));
	GMLVar* args[1] = {&arg0};
	g_get_player_color(self, other, out, 1, args);
	if (is_numeric_type(out.type))
		return static_cast<int>(out.getReal());
	return -1;
}

static std::string call_get_player_name(CInstance* self, CInstance* other, int playerId)
{
	if (!g_get_player_name)
		return {};
	GMLVar out;
	GMLVar arg0;
	arg0.setReal(static_cast<double>(playerId));
	GMLVar* args[1] = {&arg0};
	g_get_player_name(self, other, out, 1, args);
	return try_read_gml_string(out);
}

static int call_get_player_stocks(CInstance* self, CInstance* other, int playerId)
{
	if (!g_get_player_stocks)
		return -1;
	GMLVar out;
	GMLVar arg0;
	arg0.setReal(static_cast<double>(playerId));
	GMLVar* args[1] = {&arg0};
	g_get_player_stocks(self, other, out, 1, args);
	if (is_numeric_type(out.type))
		return static_cast<int>(out.getReal());
	return -1;
}

static int call_get_player_damage(CInstance* self, CInstance* other, int playerId)
{
	if (!g_get_player_damage)
		return -1;
	GMLVar out;
	GMLVar arg0;
	arg0.setReal(static_cast<double>(playerId));
	GMLVar* args[1] = {&arg0};
	g_get_player_damage(self, other, out, 1, args);
	if (is_numeric_type(out.type))
		return static_cast<int>(out.getReal());
	return -1;
}

static bool call_is_player_active(CInstance* self, CInstance* other, int playerId, bool* outKnown)
{
	if (outKnown)
		*outKnown = false;

	// is_player_active is 0 indexed
	if (playerId <= 0)
		return false;
	int playerIndex0 = playerId - 1;

	if (!g_is_player_active)
		return false;

	GMLVar out;
	GMLVar arg0;
	arg0.setReal(static_cast<double>(playerIndex0));
	GMLVar* args[1] = {&arg0};

	g_is_player_active(self, other, out, 1, args);

	if (!is_numeric_type(out.type))
		return false;

	if (outKnown)
		*outKnown = true;
	return out.getReal() != 0.0;
}

static int read_char_id_for_player_index0(int playerIndex0)
{
	if (playerIndex0 < 0 || playerIndex0 >= 4)
		return -1;

	GMLVar tmp = characters[playerIndex0];
	if (!is_numeric_type(tmp.type))
		return -1;
	return static_cast<int>(tmp.getReal());
}

static void net_set_state(NetState s, bool isResults)
{
	std::lock_guard<std::mutex> lock(g_net_mutex);
	g_net_state = s;
	g_net_is_results_screen = isResults;
}

static void net_set_stage(int stage)
{
	std::lock_guard<std::mutex> lock(g_net_mutex);
	g_net_stage = stage;
}

static void net_update_player_from_game(CInstance* self, CInstance* other, int playerId)
{
	if (playerId < 1 || playerId > 4)
		return;

	NetPlayer p;
	{
		std::lock_guard<std::mutex> lock(g_net_mutex);
		p = g_net_players[playerId - 1];
	}

	bool known = false;
	bool active = call_is_player_active(self, other, playerId, &known);
	std::string name = call_get_player_name(self, other, playerId);
	int stocks = call_get_player_stocks(self, other, playerId);
	int damage = call_get_player_damage(self, other, playerId);

	if (!name.empty())
		p.name = std::move(name);
	if (stocks >= 0)
		p.stocks = stocks;
	if (damage >= 0)
		p.damage = damage;
	p.active = known && active;

	{
		std::lock_guard<std::mutex> lock(g_net_mutex);
		g_net_players[playerId - 1] = std::move(p);
	}
}

static std::string json_escape(const std::string& s)
{
	std::string out;
	out.reserve(s.size() + 8);
	for (unsigned char ch : s)
	{
		switch (ch)
		{
		case '\\': out += "\\\\"; break;
		case '"': out += "\\\""; break;
		case '\b': out += "\\b"; break;
		case '\f': out += "\\f"; break;
		case '\n': out += "\\n"; break;
		case '\r': out += "\\r"; break;
		case '\t': out += "\\t"; break;
		default:
			if (ch < 0x20)
			{
				char buf[7];
				snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)ch);
				out += buf;
			}
			else
			{
				out.push_back(static_cast<char>(ch));
			}
			break;
		}
	}
	return out;
}

static std::string net_snapshot_to_json()
{
	NetState state;
	bool isResults;
	int stage;
	NetPlayer players[4];
	{
		std::lock_guard<std::mutex> lock(g_net_mutex);
		state = g_net_state;
		isResults = g_net_is_results_screen;
		stage = g_net_stage;
		for (int i = 0; i < 4; ++i)
			players[i] = g_net_players[i];
	}
	for (int i = 0; i < 4; ++i)
		players[i].char_id = read_char_id_for_player_index0(i);

	std::ostringstream oss;
	oss << '{';
	oss << "\"state\":\"" << net_state_name(state) << "\",";
	oss << "\"is_results_screen\":" << (isResults ? "true" : "false") << ',';
	oss << "\"stage\":" << stage << ',';
	oss << "\"players\":[";
	for (int i = 0; i < 4; ++i)
	{
		if (i) oss << ',';
		oss << '{';
		oss << "\"name\":\"" << json_escape(players[i].name) << "\",";
		oss << "\"stocks\":" << players[i].stocks << ',';
		oss << "\"damage\":" << players[i].damage << ',';
		oss << "\"char_id\":" << players[i].char_id << ',';
		oss << "\"alt\":" << players[i].alt << ',';
		oss << "\"active\":" << (players[i].active ? "true" : "false");
		oss << '}';
	}
	oss << "]";
	oss << '}';
	return oss.str();
}

static std::string get_computer_name()
{
	char buf[MAX_COMPUTERNAME_LENGTH + 1] = {0};
	DWORD sz = MAX_COMPUTERNAME_LENGTH + 1;
	if (!GetComputerNameA(buf, &sz))
		return "unknown";
	return std::string(buf);
}

static std::string get_local_ipv4()
{
	SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s == INVALID_SOCKET)
		return "0.0.0.0";

	sockaddr_in dst{};
	dst.sin_family = AF_INET;
	dst.sin_port = htons(53);
	inet_pton(AF_INET, "8.8.8.8", &dst.sin_addr);
	connect(s, (sockaddr*)&dst, sizeof(dst));

	sockaddr_in name{};
	int namelen = sizeof(name);
	if (getsockname(s, (sockaddr*)&name, &namelen) == SOCKET_ERROR)
	{
		closesocket(s);
		return "0.0.0.0";
	}

	char ipbuf[INET_ADDRSTRLEN] = {0};
	inet_ntop(AF_INET, &name.sin_addr, ipbuf, sizeof(ipbuf));
	closesocket(s);
	return std::string(ipbuf);
}

static DWORD WINAPI net_thread_main(LPVOID)
{
	const uint16_t tcpPort = (g_cfg.port_tcp != 0) ? g_cfg.port_tcp : 4242;
	const uint16_t udpPort = (g_cfg.port_udp != 0) ? g_cfg.port_udp : 6500;

	WSADATA wsa{};
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		loader_log_warn("[game-data-hook] WSAStartup failed");
		return 0;
	}

	SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (listenSock == INVALID_SOCKET)
	{
		loader_log_warn("[game-data-hook] TCP socket() failed");
		WSACleanup();
		return 0;
	}

	BOOL reuse = TRUE;
	setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));
	u_long nonblock = 1;
	ioctlsocket(listenSock, FIONBIO, &nonblock);

	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(tcpPort);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	if (bind(listenSock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
	{
		loader_log_warn("[game-data-hook] TCP bind failed (port={})", (int)tcpPort);
		closesocket(listenSock);
		WSACleanup();
		return 0;
	}
	if (listen(listenSock, 4) == SOCKET_ERROR)
	{
		loader_log_warn("[game-data-hook] TCP listen failed");
		closesocket(listenSock);
		WSACleanup();
		return 0;
	}
	loader_log_info("[game-data-hook] TCP server listening on 0.0.0.0:{} (udp_beacon_port={})", (int)tcpPort, (int)udpPort);

	SOCKET udpSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (udpSock != INVALID_SOCKET)
	{
		BOOL bcast = TRUE;
		setsockopt(udpSock, SOL_SOCKET, SO_BROADCAST, (const char*)&bcast, sizeof(bcast));
	}

	std::vector<SOCKET> clients;
	DWORD lastSendTick = 0;
	DWORD lastBeaconTick = 0;

	while (!g_net_stop.load())
	{
		for (;;)
		{
			sockaddr_in caddr{};
			int clen = sizeof(caddr);
			SOCKET cs = accept(listenSock, (sockaddr*)&caddr, &clen);
			if (cs == INVALID_SOCKET)
			{
				int err = WSAGetLastError();
				if (err == WSAEWOULDBLOCK)
					break;
				break;
			}
			clients.push_back(cs);
			g_net_has_client.store(true);
			loader_log_info("[game-data-hook] TCP client connected (clients={})", (int)clients.size());
		}

		DWORD now = GetTickCount();

		if (now - lastSendTick >= 50)
		{
			lastSendTick = now;
			if (!clients.empty())
			{
				std::string json = net_snapshot_to_json();
				json.push_back('\n');
				for (size_t i = 0; i < clients.size();)
				{
					int sent = send(clients[i], json.data(), (int)json.size(), 0);
					if (sent == SOCKET_ERROR)
					{
						closesocket(clients[i]);
						clients.erase(clients.begin() + i);
						continue;
					}
					++i;
				}
				g_net_has_client.store(!clients.empty());
			}
		}

		if (udpSock != INVALID_SOCKET && now - lastBeaconTick >= 1000)
		{
			lastBeaconTick = now;
			std::string pc = get_computer_name();
			std::string ip = get_local_ipv4();
			std::string msg = "roa_game_data_hook:" + pc + ":" + ip;

			sockaddr_in baddr{};
			baddr.sin_family = AF_INET;
			baddr.sin_port = htons(udpPort);
			baddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
			sendto(udpSock, msg.data(), (int)msg.size(), 0, (sockaddr*)&baddr, sizeof(baddr));
		}

		Sleep(10);
	}

	for (SOCKET cs : clients)
		closesocket(cs);
	if (udpSock != INVALID_SOCKET)
		closesocket(udpSock);
	closesocket(listenSock);
	WSACleanup();
	return 0;
}

static void hook_script_by_rva(const char* label, uint32_t rva, void* detour, void** original)
{
	if (rva == 0)
		return;

	HMODULE exe = GetModuleHandleA(nullptr);
	if (!exe)
	{
		loader_log_warn("[game-data-hook] could not get exe module handle for '{}'", label);
		return;
	}

	uint8_t* base = reinterpret_cast<uint8_t*>(exe);
	void* target = base + rva;

	int32_t st = loader_hook_create(target, detour, original);
	if (st != 0)
	{
		loader_log_warn("[game-data-hook] hook_create '{}' (rva=0x{:x}) failed (status={})", label, rva, st);
		return;
	}

	st = loader_hook_enable(target);
	if (st != 0)
	{
		loader_log_warn("[game-data-hook] hook_enable '{}' (rva=0x{:x}) failed (status={})", label, rva, st);
		return;
	}
}

static ScriptFn resolve_script_by_rva(const char* label, uint32_t rva)
{
	if (rva == 0)
		return nullptr;

	HMODULE exe = GetModuleHandleA(nullptr);
	if (!exe)
	{
		loader_log_warn("[game-data-hook] could not get exe module handle for '{}' rva=0x{:x}", label, rva);
		return nullptr;
	}

	uint8_t* base = reinterpret_cast<uint8_t*>(exe);
	void* target = base + rva;
	return reinterpret_cast<ScriptFn>(target);
}

static GMLVar& set_player_profile_detour(CInstance* self, CInstance* other, GMLVar& out, int argCount, GMLVar* args[])
{
	int playerId = -1;
	if (args && argCount >= 2 && args[1] && is_numeric_type(args[1]->type))
		playerId = static_cast<int>(args[1]->getReal());

	if (!g_orig_set_player_profile)
	{
		loader_log_warn("[game-data-hook] original set_player_profile is null; skipping call");
		return out;
	}

	GMLVar& ret = g_orig_set_player_profile(self, other, out, argCount, args);
	if (playerId > 0)
		net_update_player_from_game(self, other, playerId);

	return ret;
}

static GMLVar& css_bg_init_detour(CInstance* self, CInstance* other, GMLVar& out, int argCount, GMLVar* args[])
{
	net_set_state(NetState::character_select, false);
	for (int i = 1; i <= 4; ++i)
		net_update_player_from_game(self, other, i);

	if (!g_orig_css_bg_init)
	{
		loader_log_warn("[game-data-hook] original css_bg_init is null; skipping call");
		return out;
	}
	return g_orig_css_bg_init(self, other, out, argCount, args);
}

static GMLVar& update_char_color_detour(CInstance* self, CInstance* other, GMLVar& out, int argCount, GMLVar* args[])
{
	if (!g_orig_update_char_color)
	{
		loader_log_warn("[game-data-hook] original update_char_color is null; skipping call");
		return out;
	}
	GMLVar& ret = g_orig_update_char_color(self, other, out, argCount, args);

	for (int playerId = 1; playerId <= 4; ++playerId)
	{
		int alt = call_get_player_color(self, other, playerId);
		std::lock_guard<std::mutex> lock(g_net_mutex);
		g_net_players[playerId - 1].alt = alt;
	}

	return ret;
}

static GMLVar& LocalDoneCharSelect_detour(CInstance* self, CInstance* other, GMLVar& out, int argCount, GMLVar* args[])
{
	net_set_state(NetState::stage_select, false);
	for (int i = 1; i <= 4; ++i)
		net_update_player_from_game(self, other, i);

	if (!g_orig_local_done_char_select)
	{
		loader_log_warn("[game-data-hook] original LocalDoneCharSelect is null; skipping call");
		return out;
	}
	return g_orig_local_done_char_select(self, other, out, argCount, args);
}

static GMLVar& init_main_menu_text_detour(CInstance* self, CInstance* other, GMLVar& out, int argCount, GMLVar* args[])
{
	net_set_state(NetState::menu, false);
	net_set_stage(-1);

	if (!g_orig_init_main_menu_text)
	{
		loader_log_warn("[game-data-hook] original init_main_menu_text is null; skipping call");
		return out;
	}
	return g_orig_init_main_menu_text(self, other, out, argCount, args);
}

static GMLVar& take_damage_detour(CInstance* self, CInstance* other, GMLVar& out, int argCount, GMLVar* args[])
{
	int victimId = -1;
	if (args && argCount >= 1 && args[0] && is_numeric_type(args[0]->type))
		victimId = static_cast<int>(args[0]->getReal());

	if (!g_orig_take_damage)
	{
		loader_log_warn("[game-data-hook] original take_damage is null; skipping call");
		return out;
	}

	GMLVar& ret = g_orig_take_damage(self, other, out, argCount, args);

	if (victimId > 0)
	{
		int current = call_get_player_damage(self, other, victimId);

		if (victimId >= 1 && victimId <= 4)
		{
			std::lock_guard<std::mutex> lock(g_net_mutex);
			g_net_players[victimId - 1].damage = current;
		}
		net_set_state(NetState::in_game, false);
	}

	return ret;
}

static GMLVar& get_stage_dest_detour(CInstance* self, CInstance* other, GMLVar& out, int argCount, GMLVar* args[])
{
	if (!g_orig_get_stage_dest)
	{
		loader_log_warn("[game-data-hook] original get_stage_dest is null; skipping call");
		return out;
	}

	GMLVar& ret = g_orig_get_stage_dest(self, other, out, argCount, args);

	// this is kinda bad as we just sorta "cache" the last value of get_stage_dest, but it seems to work
	if (is_numeric_type(out.type))
	{
		g_last_stage_id = static_cast<int>(out.getReal());
	}

	net_set_state(NetState::stage_select, false);
	if (is_numeric_type(out.type))
		net_set_stage(g_last_stage_id);

	return ret;
}

static GMLVar& init_stage_camera_detour(CInstance* self, CInstance* other, GMLVar& out, int argCount, GMLVar* args[])
{
	// in_game
	net_set_state(NetState::in_game, false);
	if (g_last_stage_id >= 0)
		net_set_stage(g_last_stage_id);
	for (int playerId = 1; playerId <= 4; ++playerId)
		net_update_player_from_game(self, other, playerId);

	if (!g_orig_init_stage_camera)
	{
		loader_log_warn("[game-data-hook] original init_stage_camera is null; skipping call");
		return out;
	}

	return g_orig_init_stage_camera(self, other, out, argCount, args);
}

static GMLVar& death_update_detour(CInstance* self, CInstance* other, GMLVar& out, int argCount, GMLVar* args[])
{
	if (!g_orig_death_update)
	{
		loader_log_warn("[game-data-hook] original death_update is null; skipping call");
		return out;
	}

	GMLVar& ret = g_orig_death_update(self, other, out, argCount, args);

	net_set_state(NetState::in_game, false);
	for (int playerId = 1; playerId <= 4; ++playerId)
	{
		int stocks = call_get_player_stocks(self, other, playerId);
		int damage = call_get_player_damage(self, other, playerId);
		bool known = false;
		bool active = call_is_player_active(self, other, playerId, &known);
		{
			std::lock_guard<std::mutex> lock(g_net_mutex);
			g_net_players[playerId - 1].active = known && active;
			if (stocks >= 0)
				g_net_players[playerId - 1].stocks = stocks;
			if (damage >= 0)
				g_net_players[playerId - 1].damage = damage;
		}
	}

	return ret;
}

static GMLVar& game_end_detour(CInstance* self, CInstance* other, GMLVar& out, int argCount, GMLVar* args[])
{
	// we use this to know if GAME is on the screen before the results screen
	net_set_state(NetState::game_end, true);
	if (g_last_stage_id >= 0)
		net_set_stage(g_last_stage_id);
	for (int i = 1; i <= 4; ++i)
		net_update_player_from_game(self, other, i);

	if (!g_orig_game_end)
	{
		loader_log_warn("[game-data-hook] original GAME_END is null; skipping call");
		return out;
	}
	return g_orig_game_end(self, other, out, argCount, args);
}

static GMLVar& draw_end_game_text_detour(CInstance* self, CInstance* other, GMLVar& out, int argCount, GMLVar* args[])
{
	net_set_state(NetState::game_end, false);
	if (g_last_stage_id >= 0)
		net_set_stage(g_last_stage_id);
	for (int i = 1; i <= 4; ++i)
		net_update_player_from_game(self, other, i);

	if (!g_orig_draw_end_game_text)
	{
		loader_log_warn("[game-data-hook] original draw_end_game_text is null; skipping call");
		return out;
	}

	return g_orig_draw_end_game_text(self, other, out, argCount, args);
}

DWORD WINAPI entry(LPVOID module)
{
	HMODULE selfModule = (HMODULE)module;
	g_cfg = load_config_from_module_dir(selfModule);

	{
		std::lock_guard<std::mutex> lock(g_net_mutex);
		net_init_defaults_locked();
	}
	// menu on init.
	net_set_state(NetState::menu, false);

	g_get_player_name = resolve_script_by_rva("get_player_name", g_cfg.get_player_name);
	if (!g_get_player_name)
		loader_log_warn("[game-data-hook] could not resolve get_player_name (rva=0x{:x})", g_cfg.get_player_name);

	g_get_char_info = resolve_script_by_rva("get_char_info", g_cfg.get_char_info);
	if (!g_get_char_info)
		loader_log_warn("[game-data-hook] could not resolve get_char_info (rva=0x{:x})", g_cfg.get_char_info);

	g_get_player_stocks = resolve_script_by_rva("get_player_stocks", g_cfg.get_player_stocks);
	if (!g_get_player_stocks)
		loader_log_warn("[game-data-hook] could not resolve get_player_stocks (rva=0x{:x})", g_cfg.get_player_stocks);

	g_get_player_damage = resolve_script_by_rva("get_player_damage", g_cfg.get_player_damage);
	if (!g_get_player_damage)
		loader_log_warn("[game-data-hook] could not resolve get_player_damage (rva=0x{:x})", g_cfg.get_player_damage);

	g_get_player_color = resolve_script_by_rva("get_player_color", g_cfg.get_player_color);
	if (!g_get_player_color)
		loader_log_warn("[game-data-hook] could not resolve get_player_color (rva=0x{:x})", g_cfg.get_player_color);

	g_is_player_active = resolve_script_by_rva("is_player_active", g_cfg.is_player_active);
	if (!g_is_player_active)
		loader_log_warn("[game-data-hook] could not resolve is_player_active (rva=0x{:x})", g_cfg.is_player_active);

	hook_script_by_rva("set_player_profile", g_cfg.set_player_profile, (void*)set_player_profile_detour, (void**)&g_orig_set_player_profile);
	hook_script_by_rva("css_bg_init", g_cfg.css_bg_init, (void*)css_bg_init_detour, (void**)&g_orig_css_bg_init);
	hook_script_by_rva("update_char_color", g_cfg.update_char_color, (void*)update_char_color_detour, (void**)&g_orig_update_char_color);
	hook_script_by_rva("LocalDoneCharSelect", g_cfg.local_done_char_select, (void*)LocalDoneCharSelect_detour, (void**)&g_orig_local_done_char_select);
	hook_script_by_rva("take_damage", g_cfg.take_damage, (void*)take_damage_detour, (void**)&g_orig_take_damage);
	hook_script_by_rva("init_main_menu_text", g_cfg.init_main_menu_text, (void*)init_main_menu_text_detour, (void**)&g_orig_init_main_menu_text);
	hook_script_by_rva("get_stage_dest", g_cfg.get_stage_dest, (void*)get_stage_dest_detour, (void**)&g_orig_get_stage_dest);
	hook_script_by_rva("init_stage_camera", g_cfg.init_stage_camera, (void*)init_stage_camera_detour, (void**)&g_orig_init_stage_camera);
	hook_script_by_rva("death_update", g_cfg.death_update, (void*)death_update_detour, (void**)&g_orig_death_update);
	hook_script_by_rva("GAME_END", g_cfg.game_end, (void*)game_end_detour, (void**)&g_orig_game_end);
	hook_script_by_rva("draw_end_game_text", g_cfg.draw_end_game_text, (void*)draw_end_game_text_detour, (void**)&g_orig_draw_end_game_text);

	{
		// @ 0x042630ba
		std::vector<uint8_t> pattern =
		{
			0x89, 0xf0,								// mov eax, esi
			0x50,									// push eax
			0xe8, '?', '?', '?', '?',				// call func_00401870
			0x66, 0x0f, 0x57, 0xc0,					// xorpd xmm0, xmm0
			0xc7, 0x46, 0x38, 0x41, 0x02, 0x00, 0x00	// mov dword ptr [esi + 0x38], 0x241
		};

		uint32_t char_select = (uint32_t)loader_search_memory(pattern);
		if (char_select)
		{
			loader_hook_create(reinterpret_cast<void*>(char_select), &char_select_detour, reinterpret_cast<void**>(&char_select_orig));
			loader_hook_enable(reinterpret_cast<void*>(char_select));
		}
		else
		{
			loader_log_warn("[game-data-hook] char_select pattern not found; char_id will stay -1");
		}
	}
	if (!g_orig_set_player_profile)
		loader_log_warn("[game-data-hook] set_player_profile was NOT hooked (rva=0x{:x})", g_cfg.set_player_profile);
	if (!g_orig_css_bg_init)
		loader_log_warn("[game-data-hook] css_bg_init was NOT hooked (rva=0x{:x})", g_cfg.css_bg_init);
	if (!g_orig_update_char_color)
		loader_log_warn("[game-data-hook] update_char_color was NOT hooked (rva=0x{:x})", g_cfg.update_char_color);
	if (!g_orig_local_done_char_select)
		loader_log_warn("[game-data-hook] LocalDoneCharSelect was NOT hooked (rva=0x{:x})", g_cfg.local_done_char_select);
	if (!g_orig_take_damage)
		loader_log_warn("[game-data-hook] take_damage was NOT hooked (rva=0x{:x})", g_cfg.take_damage);
	if (!g_orig_init_main_menu_text)
		loader_log_warn("[game-data-hook] init_main_menu_text was NOT hooked (rva=0x{:x})", g_cfg.init_main_menu_text);
	if (!g_orig_get_stage_dest)
		loader_log_warn("[game-data-hook] get_stage_dest was NOT hooked (rva=0x{:x})", g_cfg.get_stage_dest);
	if (!g_orig_init_stage_camera)
		loader_log_warn("[game-data-hook] init_stage_camera was NOT hooked (rva=0x{:x})", g_cfg.init_stage_camera);
	if (!g_orig_death_update)
		loader_log_warn("[game-data-hook] death_update was NOT hooked (rva=0x{:x})", g_cfg.death_update);
	if (!g_orig_game_end)
		loader_log_warn("[game-data-hook] GAME_END was NOT hooked (rva=0x{:x})", g_cfg.game_end);
	if (!g_orig_draw_end_game_text)
		loader_log_warn("[game-data-hook] draw_end_game_text was NOT hooked (rva=0x{:x})", g_cfg.draw_end_game_text);

	if (!g_net_thread)
	{
		g_net_stop.store(false);
		g_net_thread = CreateThread(NULL, 0, net_thread_main, NULL, 0, NULL);
		if (g_net_thread)
			CloseHandle(g_net_thread);
	}

	return TRUE;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(hModule);
		HANDLE hHandle = CreateThread(NULL, 0, entry, hModule, 0, NULL);
		if (hHandle != NULL) CloseHandle(hHandle);
	}
	else if (reason == DLL_PROCESS_DETACH && !reserved)
	{
		g_net_stop.store(true);
		FreeLibraryAndExitThread(hModule, 0);
	}
	return TRUE;
}
