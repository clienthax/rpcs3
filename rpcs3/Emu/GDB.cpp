#include "stdafx.h"

#include "GDB.h"
#include "util/logs.hpp"
#include "Utilities/StrUtil.h"
#include "Emu/Memory/vm.h"
#include "Emu/System.h"
#include "Emu/system_config.h"
#include "Emu/IdManager.h"
#include "Emu/CPU/CPUThread.h"
#include "Emu/Cell/PPUThread.h"
#include "Emu/Cell/SPUThread.h"
#include "Emu/Cell/lv2/sys_prx.h"

#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <afunix.h> // sockaddr_un
#else
#ifdef __clang__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/un.h> // sockaddr_un
#ifdef __clang__
#pragma GCC diagnostic pop
#endif
#endif

#include "Emu/Cell/timers.hpp"

#include <charconv>
#include <regex>
#include <string_view>

extern bool ppu_breakpoint(u32 addr, bool is_adding);
extern std::string ppu_get_function_name(const std::string& _module, u32 fnid);
extern const std::unordered_map<u32, std::string>& ppu_get_stub_code_names();

LOG_CHANNEL(GDB);

#ifndef _WIN32
int closesocket(int s)
{
	return close(s);
}

void set_nonblocking(int s)
{
	fcntl(s, F_SETFL, fcntl(s, F_GETFL) | O_NONBLOCK);
}

#define sscanf_s sscanf
#else

void set_nonblocking(int s)
{
	u_long mode = 1;
	ioctlsocket(s, FIONBIO, &mode);
}

#endif

struct gdb_cmd
{
	std::string cmd{};
	std::string data{};
	u8 checksum{};
};

bool check_errno_again()
{
#ifdef _WIN32
	int err = GetLastError();
	return (err == WSAEWOULDBLOCK);
#else
	int err = errno;
	return (err == EAGAIN) || (err == EWOULDBLOCK);
#endif
}

std::string u32_to_hex(u32 i)
{
	return fmt::format("%x", i);
}

std::string u64_to_padded_hex(u64 value)
{
	return fmt::format("%.16x", value);
}

std::string u32_to_padded_hex(u32 value)
{
	return fmt::format("%.8x", value);
}

template <typename T>
T hex_to(std::string_view val)
{
	T result;
	auto [ptr, err] = std::from_chars(val.data(), val.data() + val.size(), result, 16);
	if (err != std::errc())
	{
		throw std::runtime_error(fmt::format("Failed to read hex string: %s", std::make_error_code(err).message()));
	}

	return result;
}

constexpr auto& hex_to_u8 = hex_to<u8>;
constexpr auto& hex_to_u32 = hex_to<u32>;
constexpr auto& hex_to_u64 = hex_to<u64>;

void gdb_thread::start_server()
{
	// IPv4 address:port in format 127.0.0.1:2345
	static const std::regex ipv4_regex("^([0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3})\\:([0-9]{1,5})$");

	auto sname = g_cfg.misc.gdb_server.to_string();

	if (sname[0] == '\0')
	{
		// Empty string or starts with null: GDB server disabled
		GDB.notice("GDB Server is disabled.");
		return;
	}

	// Try to detect socket type
	std::smatch match;

	if (std::regex_match(sname, match, ipv4_regex))
	{
		struct addrinfo hints{};
		struct addrinfo* info;
		hints.ai_flags    = AI_PASSIVE;
		hints.ai_socktype = SOCK_STREAM;

		std::string bind_addr = match[1].str();
		std::string bind_port = match[2].str();

		if (getaddrinfo(bind_addr.c_str(), bind_port.c_str(), &hints, &info) == 0)
		{
			server_socket = static_cast<int>(socket(info->ai_family, info->ai_socktype, info->ai_protocol));

			if (server_socket == -1)
			{
				GDB.error("Error creating IP socket for '%s'.", sname);
				freeaddrinfo(info);
				return;
			}

			set_nonblocking(server_socket);

			if (bind(server_socket, info->ai_addr, static_cast<int>(info->ai_addrlen)) != 0)
			{
				GDB.error("Failed to bind socket on '%s'.", sname);
				freeaddrinfo(info);
				return;
			}

			freeaddrinfo(info);

			if (listen(server_socket, 1) != 0)
			{
				GDB.error("Failed to listen on '%s'.", sname);
				return;
			}

			GDB.notice("Started listening on '%s'.", sname);
			return;
		}
	}

	// Fallback to UNIX socket
	server_socket = static_cast<int>(socket(AF_UNIX, SOCK_STREAM, 0));

	if (server_socket == -1)
	{
		GDB.error("Failed to create Unix socket. Possibly unsupported.");
		return;
	}

	// Delete existing socket (TODO?)
	fs::remove_file(sname);

	set_nonblocking(server_socket);

	sockaddr_un unix_saddr;
	unix_saddr.sun_family = AF_UNIX;
	strcpy_trunc(unix_saddr.sun_path, sname);

	if (bind(server_socket, reinterpret_cast<struct sockaddr*>(&unix_saddr), sizeof(unix_saddr)) != 0)
	{
		GDB.error("Failed to bind Unix socket '%s'.", sname);
		return;
	}

	if (listen(server_socket, 1) != 0)
	{
		GDB.error("Failed to listen on Unix socket '%s'.", sname);
		return;
	}

	GDB.notice("Started listening on Unix socket '%s'.", sname);
}

int gdb_thread::read(void* buf, int cnt) const
{
	while (thread_ctrl::state() != thread_state::aborting)
	{
		const int result = recv(client_socket, static_cast<char*>(buf), cnt, 0);

		if (result == -1)
		{
			if (check_errno_again())
			{
				thread_ctrl::wait_for(5000);
				continue;
			}

			GDB.error("Error during socket read.");
			throw std::runtime_error("GDB connection closed");
		}
		return result;
	}
	return 0;
}

char gdb_thread::read_char()
{
	char result;
	int cnt = read(&result, 1);
	if (!cnt)
	{
		throw std::runtime_error("GDB connection closed");
	}
	return result;
}

u8 gdb_thread::read_hexbyte()
{
	std::string s;
	s += read_char();
	s += read_char();
	return hex_to_u8(s);
}

bool gdb_thread::try_read_cmd(gdb_cmd& out_cmd)
{
	char c = read_char();
	//interrupt
	if (c == 0x03) [[unlikely]]
	{
		out_cmd.cmd = '\x03';
		out_cmd.data = "";
		out_cmd.checksum = 0;
		return true;
	}
	if (c != '$') [[unlikely]]
	{
		// '+' = ACK, '-' = NACK (retransmit request) — skip and read next char
		if (c == '+' || c == '-')
		{
			c = read_char();
		}
		if (c != '$')
		{
			throw std::runtime_error(fmt::format("Expected '$', got '%c'", c));
		}
	}
	//clear packet data
	out_cmd.cmd = "";
	out_cmd.data = "";
	out_cmd.checksum = 0;
	bool cmd_part = true;
	u8 checksum = 0;
	while (true)
	{
		c = read_char();
		if (c == '#')
		{
			break;
		}
		checksum = (checksum + reinterpret_cast<u8&>(c)) % 256;
		//escaped char
		if (c == '}')
		{
			c = read_char() ^ 0x20;
			checksum = (checksum + reinterpret_cast<u8&>(c)) % 256;
		}
		//cmd-data splitters
		if (cmd_part && ((c == ':') || (c == '.') || (c == ';')))
		{
			cmd_part = false;
		}
		if (cmd_part)
		{
			out_cmd.cmd += c;
			//only q and v commands can have multi-char command
			if ((out_cmd.cmd.length() == 1) && (c != 'q') && (c != 'v'))
			{
				cmd_part = false;
			}
		}
		else
		{
			out_cmd.data += c;
		}
	}
	out_cmd.checksum = read_hexbyte();
	return out_cmd.checksum == checksum;
}

bool gdb_thread::read_cmd(gdb_cmd& out_cmd)
{
	while (true)
	{
		try
		{
			if (try_read_cmd(out_cmd))
			{
				ack(true);
				return true;
			}
			ack(false);
		}
		catch (const std::exception&)
		{
			return false;
		}
	}
}

void gdb_thread::send(const char* buf, int cnt) const
{
	GDB.trace("Sending %s (%d bytes).", buf, cnt);

	while (cnt > 0 && thread_ctrl::state() != thread_state::aborting)
	{
		int res = ::send(client_socket, buf, cnt, 0);
		if (res == -1)
		{
			if (check_errno_again())
			{
				thread_ctrl::wait_for(5000);
				continue;
			}
			GDB.error("Failed sending %d bytes.", cnt);
			return;
		}
		buf += res;
		cnt -= res;
	}
}

void gdb_thread::send_char(char c)
{
	send(&c, 1);
}

void gdb_thread::ack(bool accepted)
{
	send_char(accepted ? '+' : '-');
}

void gdb_thread::send_cmd(const std::string& cmd)
{
	u8 checksum = 0;
	std::string buf;
	buf.reserve(cmd.length() + 4);
	buf += "$";
	for (usz i = 0; i < cmd.length(); ++i)
	{
		checksum = (checksum + append_encoded_char(cmd[i], buf)) % 256;
	}
	buf += "#";
	buf += to_hexbyte(checksum);
	send(buf.c_str(), static_cast<int>(buf.length()));
}

bool gdb_thread::send_cmd_ack(const std::string& cmd)
{
	while (true)
	{
		send_cmd(cmd);
		char c = read_char();
		if (c == '+') [[likely]]
			return true;
		if (c != '-') [[unlikely]]
		{
			GDB.error("Wrong acknowledge character received: '%c'.", c);
			return false;
		}
		GDB.warning("Client rejected our cmd.");
	}
}

u8 gdb_thread::append_encoded_char(char c, std::string& str)
{
	u8 checksum = 0;
	if ((c == '#') || (c == '$') || (c == '}') || (c == '*')) [[unlikely]]
	{
		str += '}';
		c ^= 0x20;
		checksum = '}';
	}
	checksum = (checksum + reinterpret_cast<u8&>(c)) % 256;
	str += c;
	return checksum;
}

std::string gdb_thread::to_hexbyte(u8 i)
{
	std::string result = "00";
	u8 i1 = i & 0xF;
	u8 i2 = i >> 4;
	result[0] = i2 > 9 ? 'a' + i2 - 10 : '0' + i2;
	result[1] = i1 > 9 ? 'a' + i1 - 10 : '0' + i1;
	return result;
}

bool gdb_thread::select_thread(u64 id)
{
	//in case we have none at all
	selected_thread.reset();
	const auto on_select = [&](u32, cpu_thread& cpu)
	{
		return (id == ALL_THREADS) || (id == ANY_THREAD) || (cpu.id == id);
	};
	if (auto ppu = idm::select<named_thread<ppu_thread>>(on_select))
	{
		selected_thread = ppu.ptr;
		return true;
	}
	if (auto spu = idm::select<named_thread<spu_thread>>(on_select))
	{
		selected_thread = spu.ptr;
		return true;
	}
	GDB.warning("Unable to select thread! Is the emulator running?");
	return false;
}

std::string gdb_thread::get_reg(ppu_thread* thread, u32 rid)
{
	//ids from gdb/features/rs6000/powerpc-64.c
	//pc
	switch (rid)
	{
	case 64:
		return u64_to_padded_hex(thread->cia);
	//msr
	case 65:
		// MSR not tracked; return a static user-mode 64-bit PS3 value
		return u64_to_padded_hex(0x800000000000F032ULL);
	case 66:
		return u32_to_padded_hex(thread->cr.pack());
	case 67:
		return u64_to_padded_hex(thread->lr);
	case 68:
		return u64_to_padded_hex(thread->ctr);
	case 69:
	{
		u32 xer_val = 0;
		if (thread->xer.so) xer_val |= (1u << 31);
		if (thread->xer.ov) xer_val |= (1u << 30);
		if (thread->xer.ca) xer_val |= (1u << 29);
		xer_val |= (thread->xer.cnt & 0x7Fu);
		return u32_to_padded_hex(xer_val);
	}
	//fpscr
	case 70:
		return u32_to_padded_hex(thread->fpscr.bits.pack());
	default:
		if (rid > 70) return "";
		return (rid > 31)
			? u64_to_padded_hex(std::bit_cast<u64>(thread->fpr[rid - 32])) //fpr
			: u64_to_padded_hex(thread->gpr[rid]); //gpr
	}
}

bool gdb_thread::set_reg(ppu_thread* thread, u32 rid, const std::string& value)
{
	switch (rid)
	{
	case 64:
		thread->cia = static_cast<u32>(hex_to_u64(value));
		return true;
		//msr?
	case 65:
		return true;
	case 66:
		thread->cr.unpack(hex_to_u32(value));
		return true;
	case 67:
		thread->lr = hex_to_u64(value);
		return true;
	case 68:
		thread->ctr = hex_to_u64(value);
		return true;
	case 69:
	{
		u32 xer_val = hex_to_u32(value);
		thread->xer.so = !!(xer_val & (1u << 31));
		thread->xer.ov = !!(xer_val & (1u << 30));
		thread->xer.ca = !!(xer_val & (1u << 29));
		thread->xer.cnt = static_cast<u8>(xer_val & 0x7Fu);
		return true;
	}
		//fpscr
	case 70:
		thread->fpscr.bits.unpack(hex_to_u32(value));
		return true;
	default:
		if (rid > 70) return false;
		if (rid > 31)
		{
			const u64 val = hex_to_u64(value);
			thread->fpr[rid - 32] = std::bit_cast<f64>(val);
		}
		else
		{
			thread->gpr[rid] = hex_to_u64(value);
		}
		return true;
	}
}

u32 gdb_thread::get_reg_size(ppu_thread*, u32 rid)
{
	switch (rid)
	{
	case 66:
	case 69:
	case 70:
		return 4;
	default:
		if (rid > 70)
		{
			return 0;
		}
		return 8;
	}
}

std::string gdb_thread::get_spu_reg(spu_thread* thread, u32 rid)
{
	if (rid < 128)
	{
		// 128-bit GPR: output as 32 hex chars (big-endian words)
		const v128& reg = thread->gpr[rid];
		return fmt::format("%.8x%.8x%.8x%.8x",
			reg._u32[3], reg._u32[2], reg._u32[1], reg._u32[0]);
	}
	if (rid == 128)
	{
		return u32_to_padded_hex(thread->pc);
	}
	return "";
}

bool gdb_thread::set_spu_reg(spu_thread* thread, u32 rid, const std::string& value)
{
	if (rid < 128)
	{
		if (value.size() < 32) return false;
		v128& reg = thread->gpr[rid];
		reg._u32[3] = hex_to_u32(value.substr(0, 8));
		reg._u32[2] = hex_to_u32(value.substr(8, 8));
		reg._u32[1] = hex_to_u32(value.substr(16, 8));
		reg._u32[0] = hex_to_u32(value.substr(24, 8));
		return true;
	}
	if (rid == 128)
	{
		thread->pc = static_cast<u32>(hex_to_u32(value));
		return true;
	}
	return false;
}

u32 gdb_thread::get_spu_reg_size(u32 rid)
{
	if (rid < 128) return 16;
	if (rid == 128) return 4;
	return 0;
}

bool gdb_thread::send_reason()
{
	// Ensure a thread is selected — at initial connect pausedBy may be 0
	if (!selected_thread || selected_thread->state & cpu_flag::exit)
	{
		select_thread(ANY_THREAD);
	}

	if (selected_thread && !(selected_thread->state & cpu_flag::exit))
	{
		std::string reply = fmt::format("T05thread:%.16x;", selected_thread->id);

		// Include the PC inline so GDB/Ghidra can update it without an extra round-trip
		if (auto ppu = selected_thread->try_get<named_thread<ppu_thread>>())
		{
			// GDB register 64 (0x40) = PC
			reply += fmt::format("40:%s;", u64_to_padded_hex(ppu->cia));
		}

		// Signal that shared libraries may have changed — forces GDB to re-query
		// qXfer:libraries:read and refresh Ghidra's module list on every stop
		reply += "library:0;";

		return send_cmd_ack(reply);
	}

	return send_cmd_ack("S05");
}

void gdb_thread::wait_with_interrupts()
{
	char c;
	while (!paused)
	{
		int result = recv(client_socket, &c, 1, 0);

		if (result == -1)
		{
			if (check_errno_again())
			{
				thread_ctrl::wait_for(5000);
				continue;
			}

			GDB.error("Error during socket read in wait_with_interrupts.");
			paused = true;
			break;
		}
		else if (c == 0x03)
		{
			paused = true;
		}
	}
}

bool gdb_thread::cmd_extended_mode(gdb_cmd&)
{
	return send_cmd_ack("OK");
}

bool gdb_thread::cmd_reason(gdb_cmd&)
{
	return send_reason();
}

bool gdb_thread::cmd_supported(gdb_cmd&)
{
	return send_cmd_ack("PacketSize=10000;swbreak+;qXfer:features:read+;qXfer:exec-file:read+;qXfer:threads:read+;qXfer:libraries:read+;qXfer:memory-map:read+;qXfer:osdata:read+;vFile:setfs+;vFile:open+;vFile:pread+;vFile:close+;vFile:fstat+");
}

bool gdb_thread::cmd_thread_info(gdb_cmd&)
{
	// Build full list of thread IDs
	thread_info_buf.clear();
	const auto on_select = [&](u32, cpu_thread& cpu)
	{
		if (!thread_info_buf.empty())
		{
			thread_info_buf += ',';
		}
		thread_info_buf += u64_to_padded_hex(static_cast<u64>(cpu.id));
	};
	idm::select<named_thread<ppu_thread>>(on_select);
	idm::select<named_thread<spu_thread>>(on_select);

	// Send first chunk: "m<ids>" — up to 1190 chars of IDs
	constexpr usz max_chunk = 1190;
	if (thread_info_buf.size() <= max_chunk)
	{
		std::string result = "m" + thread_info_buf + "l";
		thread_info_buf.clear();
		return send_cmd_ack(result);
	}

	// Find a safe split point (last ',' within limit)
	usz split = thread_info_buf.rfind(',', max_chunk);
	if (split == umax) split = max_chunk;

	std::string chunk = "m" + thread_info_buf.substr(0, split);
	thread_info_buf = thread_info_buf.substr(split + 1); // skip the comma
	return send_cmd_ack(chunk);
}

bool gdb_thread::cmd_thread_info_continued(gdb_cmd&)
{
	if (thread_info_buf.empty())
	{
		return send_cmd_ack("l");
	}

	constexpr usz max_chunk = 1190;
	if (thread_info_buf.size() <= max_chunk)
	{
		std::string result = "m" + thread_info_buf + "l";
		thread_info_buf.clear();
		return send_cmd_ack(result);
	}

	usz split = thread_info_buf.rfind(',', max_chunk);
	if (split == umax) split = max_chunk;

	std::string chunk = "m" + thread_info_buf.substr(0, split);
	thread_info_buf = thread_info_buf.substr(split + 1);
	return send_cmd_ack(chunk);
}

bool gdb_thread::cmd_current_thread(gdb_cmd&)
{
	return send_cmd_ack(selected_thread && selected_thread->state.none_of(cpu_flag::exit) ? ("QC" + u64_to_padded_hex(selected_thread->id)) : "");
}

bool gdb_thread::cmd_read_register(gdb_cmd& cmd)
{
	if (!select_thread(general_ops_thread_id))
	{
		return send_cmd_ack("E02");
	}

	if (!selected_thread || selected_thread->state & cpu_flag::exit)
	{
		return send_cmd_ack("");
	}

	if (auto ppu = selected_thread->try_get<named_thread<ppu_thread>>())
	{
		u32 rid = hex_to_u32(cmd.data);
		std::string result = get_reg(ppu, rid);
		if (result.empty())
		{
			GDB.warning("Wrong register id %d.", rid);
			return send_cmd_ack("E01");
		}
		return send_cmd_ack(result);
	}

	if (selected_thread->try_get<named_thread<spu_thread>>())
	{
		// GDB expects PPU register sizes from the target XML — report SPU registers as unavailable
		u32 rid = hex_to_u32(cmd.data);
		const u32 size = get_reg_size(nullptr, rid);
		if (size == 0)
		{
			return send_cmd_ack("E01");
		}
		return send_cmd_ack(std::string(size * 2, 'x'));
	}

	GDB.warning("Unimplemented thread type %d.", selected_thread->id_type());
	return send_cmd_ack("");
}

bool gdb_thread::cmd_write_register(gdb_cmd& cmd)
{
	if (!select_thread(general_ops_thread_id))
	{
		return send_cmd_ack("E02");
	}

	if (!selected_thread || selected_thread->state & cpu_flag::exit)
	{
		return send_cmd_ack("");
	}

	if (auto ppu = selected_thread->try_get<named_thread<ppu_thread>>())
	{
		usz eq_pos = cmd.data.find('=');
		if (eq_pos == umax)
		{
			GDB.warning("Wrong write_register cmd data '%s'.", cmd.data);
			return send_cmd_ack("E02");
		}
		u32 rid = hex_to_u32(cmd.data.substr(0, eq_pos));
		std::string value = cmd.data.substr(eq_pos + 1);
		if (!set_reg(ppu, rid, value))
		{
			GDB.warning("Wrong register id %d.", rid);
			return send_cmd_ack("E01");
		}
		return send_cmd_ack("OK");
	}

	if (selected_thread->try_get<named_thread<spu_thread>>())
	{
		// Can't apply PPU register format to SPU — silently accept
		return send_cmd_ack("OK");
	}

	GDB.warning("Unimplemented thread type %d.", selected_thread->id_type());
	return send_cmd_ack("");
}

bool gdb_thread::cmd_read_memory(gdb_cmd& cmd)
{
	usz comma = cmd.data.find(',');
	u32 addr = hex_to_u32(cmd.data.substr(0, comma));
	u32 len = hex_to_u32(cmd.data.substr(comma + 1));
	std::string result;
	result.reserve(len * 2);
	for (u32 i = 0; i < len; ++i)
	{
		if (vm::check_addr(addr))
		{
			result += to_hexbyte(vm::read8(addr + i));
		}
		else
		{
			//break;
			result += "xx";
		}
	}
	if (len && result.empty())
	{
		//nothing read
		return send_cmd_ack("E01");
	}
	return send_cmd_ack(result);
}

bool gdb_thread::cmd_write_memory(gdb_cmd& cmd)
{
	usz s = cmd.data.find(',');
	usz s2 = cmd.data.find(':');
	if ((s == umax) || (s2 == umax))
	{
		GDB.warning("Malformed write memory request received: '%s'.", cmd.data);
		return send_cmd_ack("E01");
	}
	u32 addr = hex_to_u32(cmd.data.substr(0, s));
	u32 len = hex_to_u32(cmd.data.substr(s + 1, s2 - s - 1));
	const char* data_ptr = (cmd.data.c_str()) + s2 + 1;
	for (u32 i = 0; i < len; ++i)
	{
		if (vm::check_addr(addr + i, vm::page_writable))
		{
			u8 val;
			int res = sscanf_s(data_ptr, "%02hhX", &val);
			if (!res)
			{
				GDB.warning("Couldn't read u8 from string '%s'.", data_ptr);
				return send_cmd_ack("E02");
			}
			data_ptr += 2;
			vm::write8(addr + i, val);
		}
		else
		{
			return send_cmd_ack("E03");
		}
	}
	return send_cmd_ack("OK");
}

bool gdb_thread::cmd_read_all_registers(gdb_cmd&)
{
	std::string result;
	select_thread(general_ops_thread_id);

	if (!selected_thread || selected_thread->state & cpu_flag::exit)
	{
		return send_cmd_ack("");
	}

	if (auto ppu = selected_thread->try_get<named_thread<ppu_thread>>())
	{
		//68 64-bit registers, and 3 32-bit
		result.reserve(68*16 + 3*8);
		for (int i = 0; i < 71; ++i)
		{
			result += get_reg(ppu, i);
		}
		return send_cmd_ack(result);
	}

	if (selected_thread->try_get<named_thread<spu_thread>>())
	{
		// GDB expects the PPU register layout (556 bytes = 1112 hex chars) for all threads.
		// SPU has a completely different layout so we report all registers as unavailable.
		return send_cmd_ack(std::string(1112, 'x'));
	}

	GDB.warning("Unimplemented thread type %d.", selected_thread->id_type());
	return send_cmd_ack("");
}

bool gdb_thread::cmd_write_all_registers(gdb_cmd& cmd)
{
	select_thread(general_ops_thread_id);

	if (!selected_thread || selected_thread->state & cpu_flag::exit)
	{
		return send_cmd_ack("");
	}

	if (auto ppu = selected_thread->try_get<named_thread<ppu_thread>>())
	{
		int ptr = 0;
		for (int i = 0; i < 71; ++i)
		{
			int sz = get_reg_size(ppu, i);
			set_reg(ppu, i, cmd.data.substr(ptr, sz * 2));
			ptr += sz * 2;
		}
		return send_cmd_ack("OK");
	}

	if (selected_thread->try_get<named_thread<spu_thread>>())
	{
		// Can't apply PPU register format to SPU — silently accept
		return send_cmd_ack("OK");
	}

	GDB.warning("Unimplemented thread type %d.", selected_thread->id_type());
	return send_cmd_ack("E01");
}

bool gdb_thread::cmd_set_thread_ops(gdb_cmd& cmd)
{
	char type = cmd.data[0];
	std::string thread = cmd.data.substr(1);
	u64 id = thread == "-1" ? ALL_THREADS : hex_to_u64(thread);
	if (type == 'c')
	{
		continue_ops_thread_id = id;
	}
	else
	{
		general_ops_thread_id = id;
	}
	if (select_thread(id))
	{
		return send_cmd_ack("OK");
	}
	GDB.warning("Client asked to use thread 0x%x for %s, but no matching thread was found.", id, type == 'c' ? "continue ops" : "general ops");
	return send_cmd_ack("E01");
}

bool gdb_thread::cmd_attached_to_what(gdb_cmd&)
{
	//creating processes from client is not available yet
	return send_cmd_ack("1");
}

bool gdb_thread::cmd_kill(gdb_cmd&)
{
	GDB.notice("Kill command issued");
	Emu.CallFromMainThread([](){ Emu.GracefulShutdown(); });
	return true;
}

bool gdb_thread::cmd_continue_support(gdb_cmd&)
{
	return send_cmd_ack("vCont;c;s;C;S");
}

bool gdb_thread::cmd_vcont(gdb_cmd& cmd)
{
	this->from_breakpoint = false;

	if (cmd.data.size() < 2)
	{
		return send_cmd_ack("");
	}

	// cmd.data format: ";action[:pid.tid]" possibly followed by more ";action[:tid]" clauses
	const char action = cmd.data[1];

	if (action != 'c' && action != 's')
	{
		return send_cmd_ack("");
	}

	// Parse optional embedded thread ID: ";s:0000000001000001"
	u64 op_thread_id = continue_ops_thread_id;
	if (cmd.data.size() > 2 && cmd.data[2] == ':')
	{
		// Extract thread id string — ends at next ';' or end of data
		const usz start = 3;
		const usz semi = cmd.data.find(';', start);
		std::string tid_str = cmd.data.substr(start, semi == umax ? umax : semi - start);

		// Handle "pid.tid" format — take only the tid part
		const usz dot = tid_str.find('.');
		if (dot != umax)
		{
			tid_str = tid_str.substr(dot + 1);
		}

		if (!tid_str.empty())
		{
			try
			{
				op_thread_id = (tid_str == "-1") ? ALL_THREADS : hex_to_u64(tid_str);
			}
			catch (...)
			{
				// keep continue_ops_thread_id on parse failure
			}
		}
	}

	select_thread(op_thread_id);

	auto ppu = !selected_thread || selected_thread->state & cpu_flag::exit
		? nullptr
		: selected_thread->try_get<named_thread<ppu_thread>>();

	paused = false;

	if (ppu)
	{
		bs_t<cpu_flag> add_flags{};

		if (action == 's')
		{
			add_flags += cpu_flag::dbg_step;
		}

		ppu->add_remove_flags(add_flags, cpu_flag::dbg_pause);
	}
	else if (auto spu = !selected_thread || selected_thread->state & cpu_flag::exit
		? nullptr
		: selected_thread->try_get<named_thread<spu_thread>>())
	{
		bs_t<cpu_flag> add_flags{};
		if (action == 's')
		{
			add_flags += cpu_flag::dbg_step;
		}
		spu->add_remove_flags(add_flags, cpu_flag::dbg_pause);
	}

	if (Emu.IsReady())
	{
		Emu.Run(true);
	}
	else if (Emu.IsPaused())
	{
		Emu.Resume();
	}

	wait_with_interrupts();
	// all-stop mode: pause everything
	Emu.Pause();
	select_thread(pausedBy);

	ppu = !selected_thread || selected_thread->state & cpu_flag::exit
		? nullptr
		: selected_thread->try_get<named_thread<ppu_thread>>();

	if (ppu)
	{
		ppu->add_remove_flags({}, cpu_flag::dbg_pause);
	}
	else if (auto spu = !selected_thread || selected_thread->state & cpu_flag::exit
		? nullptr
		: selected_thread->try_get<named_thread<spu_thread>>())
	{
		spu->add_remove_flags({}, cpu_flag::dbg_pause);
	}

	return send_reason();
}

bool gdb_thread::cmd_thread_alive(gdb_cmd& cmd)
{
	u64 id;
	try
	{
		id = hex_to_u64(cmd.data);
	}
	catch (...)
	{
		return send_cmd_ack("E01");
	}
	const auto on_select = [id](u32, cpu_thread& cpu)
	{
		return cpu.id == id;
	};
	if (idm::select<named_thread<ppu_thread>>(on_select) ||
		idm::select<named_thread<spu_thread>>(on_select))
	{
		return send_cmd_ack("OK");
	}
	return send_cmd_ack("E01");
}

bool gdb_thread::cmd_detach(gdb_cmd&)
{
	if (!send_cmd_ack("OK"))
	{
		return false;
	}
	if (Emu.IsPaused())
	{
		Emu.Resume();
	}
	return false; // break connection loop
}

static const u32 INVALID_PTR = 0xffffffff;

bool gdb_thread::cmd_set_breakpoint(gdb_cmd& cmd)
{
	char type = cmd.data[0];
	//software breakpoint
	if (type == '0')
	{
		u32 addr = INVALID_PTR;
		if (cmd.data.find(';') != umax)
		{
			GDB.warning("Received request to set breakpoint with condition, but they are not supported.");
			return send_cmd_ack("E01");
		}
		sscanf_s(cmd.data.c_str(), "0,%x", &addr);
		if (addr == INVALID_PTR)
		{
			GDB.warning("Can't parse breakpoint request, data: '%s'.", cmd.data);
			return send_cmd_ack("E02");
		}
		ppu_breakpoint(addr, true);
		return send_cmd_ack("OK");
	}
	//other breakpoint types are not supported
	return send_cmd_ack("");
}

bool gdb_thread::cmd_remove_breakpoint(gdb_cmd& cmd)
{
	char type = cmd.data[0];
	//software breakpoint
	if (type == '0')
	{
		u32 addr = INVALID_PTR;
		sscanf_s(cmd.data.c_str(), "0,%x", &addr);
		if (addr == INVALID_PTR)
		{
			GDB.warning("Can't parse breakpoint remove request, data: '%s'.", cmd.data);
			return send_cmd_ack("E01");
		}
		ppu_breakpoint(addr, false);
		return send_cmd_ack("OK");
	}
	//other breakpoint types are not supported
	return send_cmd_ack("");

}

static const std::string_view ppu_target_xml = R"(<?xml version="1.0"?>
<!DOCTYPE target SYSTEM "gdb-target.dtd">
<target version="1.0">
  <architecture>powerpc:common64</architecture>
  <feature name="org.gnu.gdb.power.core">
    <reg name="r0"  bitsize="64" type="uint64" regnum="0"/>
    <reg name="r1"  bitsize="64" type="uint64"/>
    <reg name="r2"  bitsize="64" type="uint64"/>
    <reg name="r3"  bitsize="64" type="uint64"/>
    <reg name="r4"  bitsize="64" type="uint64"/>
    <reg name="r5"  bitsize="64" type="uint64"/>
    <reg name="r6"  bitsize="64" type="uint64"/>
    <reg name="r7"  bitsize="64" type="uint64"/>
    <reg name="r8"  bitsize="64" type="uint64"/>
    <reg name="r9"  bitsize="64" type="uint64"/>
    <reg name="r10" bitsize="64" type="uint64"/>
    <reg name="r11" bitsize="64" type="uint64"/>
    <reg name="r12" bitsize="64" type="uint64"/>
    <reg name="r13" bitsize="64" type="uint64"/>
    <reg name="r14" bitsize="64" type="uint64"/>
    <reg name="r15" bitsize="64" type="uint64"/>
    <reg name="r16" bitsize="64" type="uint64"/>
    <reg name="r17" bitsize="64" type="uint64"/>
    <reg name="r18" bitsize="64" type="uint64"/>
    <reg name="r19" bitsize="64" type="uint64"/>
    <reg name="r20" bitsize="64" type="uint64"/>
    <reg name="r21" bitsize="64" type="uint64"/>
    <reg name="r22" bitsize="64" type="uint64"/>
    <reg name="r23" bitsize="64" type="uint64"/>
    <reg name="r24" bitsize="64" type="uint64"/>
    <reg name="r25" bitsize="64" type="uint64"/>
    <reg name="r26" bitsize="64" type="uint64"/>
    <reg name="r27" bitsize="64" type="uint64"/>
    <reg name="r28" bitsize="64" type="uint64"/>
    <reg name="r29" bitsize="64" type="uint64"/>
    <reg name="r30" bitsize="64" type="uint64"/>
    <reg name="r31" bitsize="64" type="uint64"/>
    <reg name="pc"  bitsize="64" type="code_ptr" regnum="64"/>
    <reg name="msr" bitsize="64" regnum="65"/>
    <reg name="cr"  bitsize="32" regnum="66"/>
    <reg name="lr"  bitsize="64" type="code_ptr" regnum="67"/>
    <reg name="ctr" bitsize="64" regnum="68"/>
    <reg name="xer" bitsize="32" regnum="69"/>
  </feature>
  <feature name="org.gnu.gdb.power.fpu">
    <reg name="f0"  bitsize="64" type="ieee_double" regnum="32"/>
    <reg name="f1"  bitsize="64" type="ieee_double"/>
    <reg name="f2"  bitsize="64" type="ieee_double"/>
    <reg name="f3"  bitsize="64" type="ieee_double"/>
    <reg name="f4"  bitsize="64" type="ieee_double"/>
    <reg name="f5"  bitsize="64" type="ieee_double"/>
    <reg name="f6"  bitsize="64" type="ieee_double"/>
    <reg name="f7"  bitsize="64" type="ieee_double"/>
    <reg name="f8"  bitsize="64" type="ieee_double"/>
    <reg name="f9"  bitsize="64" type="ieee_double"/>
    <reg name="f10" bitsize="64" type="ieee_double"/>
    <reg name="f11" bitsize="64" type="ieee_double"/>
    <reg name="f12" bitsize="64" type="ieee_double"/>
    <reg name="f13" bitsize="64" type="ieee_double"/>
    <reg name="f14" bitsize="64" type="ieee_double"/>
    <reg name="f15" bitsize="64" type="ieee_double"/>
    <reg name="f16" bitsize="64" type="ieee_double"/>
    <reg name="f17" bitsize="64" type="ieee_double"/>
    <reg name="f18" bitsize="64" type="ieee_double"/>
    <reg name="f19" bitsize="64" type="ieee_double"/>
    <reg name="f20" bitsize="64" type="ieee_double"/>
    <reg name="f21" bitsize="64" type="ieee_double"/>
    <reg name="f22" bitsize="64" type="ieee_double"/>
    <reg name="f23" bitsize="64" type="ieee_double"/>
    <reg name="f24" bitsize="64" type="ieee_double"/>
    <reg name="f25" bitsize="64" type="ieee_double"/>
    <reg name="f26" bitsize="64" type="ieee_double"/>
    <reg name="f27" bitsize="64" type="ieee_double"/>
    <reg name="f28" bitsize="64" type="ieee_double"/>
    <reg name="f29" bitsize="64" type="ieee_double"/>
    <reg name="f30" bitsize="64" type="ieee_double"/>
    <reg name="f31" bitsize="64" type="ieee_double"/>
    <reg name="fpscr" bitsize="32" group="float" regnum="70"/>
  </feature>
</target>)";

bool gdb_thread::cmd_qxfer(gdb_cmd& cmd)
{
	// cmd.data has a leading ':' from the packet parser (the ':' after "qXfer")
	// Parse the offset,length range from the last ':'-delimited field
	const auto serve_data = [&](const std::string& data) -> bool
	{
		const usz colon = cmd.data.rfind(':');
		if (colon == umax)
		{
			return send_cmd_ack("E01");
		}
		const std::string_view range = std::string_view(cmd.data).substr(colon + 1);
		const usz comma = range.find(',');
		if (comma == umax)
		{
			return send_cmd_ack("E01");
		}
		u32 offset = 0;
		u32 length = 0;
		auto [p1, e1] = std::from_chars(range.data(), range.data() + comma, offset, 16);
		auto [p2, e2] = std::from_chars(range.data() + comma + 1, range.data() + range.size(), length, 16);
		if (e1 != std::errc() || e2 != std::errc())
		{
			return send_cmd_ack("E01");
		}
		if (offset >= data.size())
		{
			return send_cmd_ack("l");
		}
		const std::string_view chunk = std::string_view(data).substr(offset, length);
		const bool last = (offset + chunk.size() >= data.size());
		return send_cmd_ack(std::string(last ? "l" : "m") + std::string(chunk));
	};

	if (cmd.data.starts_with(":features:read:target.xml:"))
	{
		return serve_data(std::string(ppu_target_xml));
	}

	if (cmd.data.starts_with(":exec-file:read:"))
	{
		// annex is pid (hex) or empty — we ignore it and always return the main executable name
		std::string exec_name;
		if (const auto main_mod = g_fxo->try_get<main_ppu_module<lv2_obj>>())
		{
			exec_name = main_mod->name;
			if (exec_name.empty() && !main_mod->path.empty())
			{
				const usz slash = main_mod->path.find_last_of("/\\");
				exec_name = (slash == umax) ? main_mod->path : main_mod->path.substr(slash + 1);
			}
		}
		if (exec_name.empty())
			return send_cmd_ack("l");
		return serve_data("target:" + exec_name);
	}

	if (cmd.data.starts_with(":threads:read:"))
	{
		std::string xml = "<?xml version=\"1.0\"?>\n<threads>\n";

		idm::select<named_thread<ppu_thread>>([&](u32, cpu_thread& cpu)
		{
			xml += fmt::format("  <thread id=\"%.16x\" name=\"%s\"/>\n",
				static_cast<u64>(cpu.id),
				thread_ctrl::get_name(static_cast<named_thread<ppu_thread>&>(cpu)));
		});
		idm::select<named_thread<spu_thread>>([&](u32, cpu_thread& cpu)
		{
			xml += fmt::format("  <thread id=\"%.16x\" name=\"%s\"/>\n",
				static_cast<u64>(cpu.id),
				thread_ctrl::get_name(static_cast<named_thread<spu_thread>&>(cpu)));
		});

		xml += "</threads>";
		return serve_data(xml);
	}

	if (cmd.data.starts_with(":libraries:read:"))
	{
		std::string xml = "<?xml version=\"1.0\"?>\n<library-list>\n";

		const auto emit_module = [&](const ppu_module<lv2_obj>& mod)
		{
			// Use just the module name (bare filename), not the full host path.
			// Returning the host path would cause GDB to open the encrypted SELF/SPRX
			// and fail with "not in executable format". GDB can't find a bare filename
			// in its solib search path and skips symbol loading gracefully, but the
			// segment addresses are still registered.
			std::string mod_name = mod.name;
			if (mod_name.empty() && !mod.path.empty())
			{
				const usz slash = mod.path.find_last_of("/\\");
				mod_name = (slash == umax) ? mod.path : mod.path.substr(slash + 1);
			}
			if (mod_name.empty()) return;

			// Only emit PT_LOAD segments (type==1) — must exactly match what build_module_elf
			// puts in program headers, so GDB can pair XML segments with ELF PT_LOADs by index.
			// A count mismatch triggers GDB's base_addr fallback which causes the
			// addr_low > addr_high assertion when non-code sections (sh_addr=0) are encountered.
			std::vector<const ppu_segment*> pt_segs;
			for (const ppu_segment& seg : mod.segs)
				if (seg.type == 1u && seg.addr && seg.size)
					pt_segs.push_back(&seg);

			if (pt_segs.empty()) return; // no loaded PT_LOAD segments — skip

			// Ascending order must match build_module_elf so GDB pairs segments by index correctly
			std::sort(pt_segs.begin(), pt_segs.end(), [](const ppu_segment* a, const ppu_segment* b) {
				return a->addr < b->addr;
			});

			std::string seg_xml;
			for (const ppu_segment* seg : pt_segs)
				seg_xml += fmt::format("    <segment address=\"0x%x\"/>\n", seg->addr);

			xml += fmt::format("  <library name=\"target:%s\">\n", mod_name);
			xml += seg_xml;
			xml += "  </library>\n";
		};

		// Main executable first
		if (const auto main_mod = g_fxo->try_get<main_ppu_module<lv2_obj>>())
		{
			emit_module(*main_mod);
		}

		// Loaded PRX modules
		idm::select<lv2_obj, lv2_prx>([&](u32, lv2_prx& prx)
		{
			emit_module(prx);
		});

		xml += "</library-list>";
		return serve_data(xml);
	}

	if (cmd.data.starts_with(":memory-map:read:"))
	{
		std::string xml = "<?xml version=\"1.0\"?>\n"
			"<!DOCTYPE memory-map PUBLIC \"+//IDN gnu.org//DTD GDB Memory Map V1.0//EN\"\n"
			"  \"http://sourceware.org/gdb/gdb-memory-map.dtd\">\n"
			"<memory-map>\n";

		// Walk the 4KB page table and merge contiguous allocated regions.
		// type: 1 = ram (writable), 2 = rom (read-only)
		constexpr usz total_pages = 0x100000000ull / 4096u;
		u32 region_start = 0;
		u8 region_type = 0;

		for (usz i = 0; i <= total_pages; ++i)
		{
			u8 new_type = 0;
			if (i < total_pages)
			{
				const auto [allocated, flags] = vm::get_addr_flags(static_cast<u32>(i * 4096u));
				if (allocated)
				{
					new_type = (flags & vm::page_writable) ? 1 : 2;
				}
			}

			if (new_type != region_type)
			{
				if (region_type != 0)
				{
					// Use u32 arithmetic: 0x100000000 wraps to 0, giving correct unsigned length
					const u32 end = static_cast<u32>(i * 4096u);
					xml += fmt::format("  <memory type=\"%s\" start=\"0x%.8x\" length=\"0x%.8x\"/>\n",
						region_type == 1 ? "ram" : "rom",
						region_start,
						end - region_start);
				}
				region_start = static_cast<u32>(i * 4096u);
				region_type = new_type;
			}
		}

		xml += "</memory-map>";
		return serve_data(xml);
	}

	if (cmd.data.starts_with(":osdata:read:mappings:"))
	{
		std::string xml = "<?xml version=\"1.0\"?>\n"
			"<!DOCTYPE target SYSTEM \"osdata.dtd\">\n"
			"<osdata type=\"mappings\">\n";

		// Walk the 4KB page table, merge runs with identical permission bits.
		constexpr usz total_pages = 0x100000000ull / 4096u;
		u32 region_start = 0;
		u8 region_flags = 0;
		bool in_region = false;

		const auto flush = [&](u32 end_addr)
		{
			xml += fmt::format(
				"  <item>\n"
				"    <column name=\"Start Address\">0x%x</column>\n"
				"    <column name=\"End Address\">0x%x</column>\n"
				"    <column name=\"Size\">0x%x</column>\n"
				"    <column name=\"Offset\">0x0</column>\n"
				"    <column name=\"Permissions\">%s%s%sp</column>\n"
				"    <column name=\"Filename\"></column>\n"
				"  </item>\n",
				region_start, end_addr, end_addr - region_start,
				(region_flags & vm::page_readable)   ? "r" : "-",
				(region_flags & vm::page_writable)   ? "w" : "-",
				(region_flags & vm::page_executable) ? "x" : "-");
		};

		for (usz i = 0; i <= total_pages; ++i)
		{
			bool allocated = false;
			u8 flags = 0;

			if (i < total_pages)
			{
				const auto [alloc, f] = vm::get_addr_flags(static_cast<u32>(i * 4096u));
				allocated = alloc;
				if (allocated)
				{
					flags = f & (vm::page_readable | vm::page_writable | vm::page_executable);
				}
			}

			if (allocated && in_region && flags == region_flags)
			{
				continue; // extend current region
			}

			if (in_region)
			{
				// Use u32 arithmetic: 0x100000000 wraps to 0, giving correct unsigned difference
				flush(static_cast<u32>(i * 4096u));
				in_region = false;
			}

			if (allocated)
			{
				region_start = static_cast<u32>(i * 4096u);
				region_flags = flags;
				in_region = true;
			}
		}

		xml += "</osdata>";
		return serve_data(xml);
	}

	return send_cmd_ack("");
}

static u32 gdb_vm_read_be32(u32 ps3_addr)
{
	const u8* p = static_cast<const u8*>(vm::base(ps3_addr));
	return (u32(p[0]) << 24) | (u32(p[1]) << 16) | (u32(p[2]) << 8) | p[3];
}

// Walk the PRX export table in PS3 VM and return fn_addr -> name for known exports.
static std::map<u32, std::string> collect_prx_exports(const lv2_prx& prx)
{
	std::map<u32, std::string> result;
	if (prx.exports_end <= prx.exports_start) return result;

	const std::string mod_name = prx.module_info_name;
	constexpr u32 min_entry = 0x1C; // minimum ppu_prx_module_info size

	for (u32 ea = prx.exports_start; ea < prx.exports_end;)
	{
		if (!vm::check_addr(ea, vm::page_readable, min_entry)) break;

		const u8* e = static_cast<const u8*>(vm::base(ea));
		const u8  esz        = e[0];
		const u16 attributes = (u16(e[4]) << 8) | e[5];
		const u16 num_func   = (u16(e[6]) << 8) | e[7];
		// ppu_prx_module_info layout (0x1C minimal):
		//   0x00 size, 0x01 unk0, 0x02 version(2), 0x04 attributes(2),
		//   0x06 num_func(2), 0x08 num_var(2), 0x0A num_tlsvar(2),
		//   0x0C info_hash, 0x0D info_tlshash, 0x0E unk1[2],
		//   0x10 name*, 0x14 nids*, 0x18 addrs*
		const u32 nid_ptr  = (u32(e[0x14]) << 24) | (u32(e[0x15]) << 16) | (u32(e[0x16]) << 8) | e[0x17];
		const u32 addr_ptr = (u32(e[0x18]) << 24) | (u32(e[0x19]) << 16) | (u32(e[0x1A]) << 8) | e[0x1B];

		if ((attributes & 0x1) /*is_library*/ && num_func > 0
			&& vm::check_addr(nid_ptr,  vm::page_readable, num_func * 4)
			&& vm::check_addr(addr_ptr, vm::page_readable, num_func * 4))
		{
			for (u32 i = 0; i < num_func; i++)
			{
				const u32 fnid     = gdb_vm_read_be32(nid_ptr  + i * 4);
				const u32 opd_addr = gdb_vm_read_be32(addr_ptr + i * 4);

				// OPD entry: [fn_ptr(4), toc(4)] — first word is the actual function address
				if (!opd_addr || !vm::check_addr(opd_addr, vm::page_readable, 4)) continue;
				const u32 fn_addr = gdb_vm_read_be32(opd_addr);
				if (!fn_addr) continue;

				std::string name = ppu_get_function_name(mod_name, fnid);
				if (name.empty()) name = fmt::format("nid_%08x", fnid);
				result.emplace(fn_addr, std::move(name));
			}
		}

		ea += esz ? esz : min_entry;
	}

	return result;
}

// Build an ELF64 big-endian (PowerPC64) from a module's loaded VM segments,
// with .symtab/.strtab/.shstrtab sections populated from the analyser function list
// and (for PRX modules) named exports from the export table.
static std::vector<u8> build_module_elf(const ppu_module<lv2_obj>& mod,
	const std::map<u32, std::string>& named_syms = {})
{
	std::vector<const ppu_segment*> segs;
	for (const ppu_segment& seg : mod.segs)
	{
		if (seg.addr && seg.size && seg.type == 1u) // PT_LOAD
			segs.push_back(&seg);
	}
	if (segs.empty()) return {};

	// Sort by ascending load address so GDB's addr_low <= addr_high assertion holds
	std::sort(segs.begin(), segs.end(), [](const ppu_segment* a, const ppu_segment* b) {
		return a->addr < b->addr;
	});

	const u32 num_phdr  = static_cast<u32>(segs.size());
	constexpr u32 ehdr_size = 64;
	constexpr u32 phdr_size = 56;
	const u32 data_start = ehdr_size + num_phdr * phdr_size;

	// Precompute each segment's file offset
	std::vector<u32> seg_foff(num_phdr);
	u32 seg_total = 0;
	for (u32 i = 0; i < num_phdr; i++)
	{
		seg_foff[i] = data_start + seg_total;
		seg_total  += segs[i]->filesz;
	}

	// Helper: find which PT_LOAD section index (1-based) contains an address.
	// Returns SHN_ABS if no segment contains it.
	const auto find_shndx = [&](u32 addr) -> u16
	{
		for (u32 i = 0; i < num_phdr; i++)
		{
			if (addr >= segs[i]->addr && addr < segs[i]->addr + segs[i]->size)
				return static_cast<u16>(i + 1);
		}
		return 0xFFF1; // SHN_ABS
	};

	// .strtab
	std::string strtab;
	strtab.push_back('\0');

	struct SymEntry { u32 addr; u32 size; u32 name_off; u16 shndx; };
	std::vector<SymEntry> sym_list;
	sym_list.push_back({0, 0, 0, 0}); // NULL entry

	std::unordered_map<u32, bool> seen;
	for (const ppu_function& fn : mod.funcs)
	{
		if (!fn.addr) continue;
		seen[fn.addr] = true;
		const u32 noff = static_cast<u32>(strtab.size());
		auto it = named_syms.find(fn.addr);
		strtab += (it != named_syms.end() && !it->second.empty())
		           ? it->second
		           : fmt::format("sub_%08x", fn.addr);
		strtab.push_back('\0');
		sym_list.push_back({fn.addr, fn.size, noff, find_shndx(fn.addr)});
	}
	for (const auto& [addr, name] : named_syms)
	{
		if (!addr || seen.count(addr)) continue;
		const u32 noff = static_cast<u32>(strtab.size());
		strtab += name.empty() ? fmt::format("sub_%08x", addr) : name;
		strtab.push_back('\0');
		sym_list.push_back({addr, 0, noff, find_shndx(addr)});
	}

	// .shstrtab — one shared ".text" name reused by all PT_LOAD sections
	std::string shstrtab;
	shstrtab.push_back('\0');
	const u32 text_shname     = static_cast<u32>(shstrtab.size());
	shstrtab += ".text"; shstrtab.push_back('\0');
	const u32 symtab_shname   = static_cast<u32>(shstrtab.size());
	shstrtab += ".symtab"; shstrtab.push_back('\0');
	const u32 strtab_shname   = static_cast<u32>(shstrtab.size());
	shstrtab += ".strtab"; shstrtab.push_back('\0');
	const u32 shstrtab_shname = static_cast<u32>(shstrtab.size());
	shstrtab += ".shstrtab"; shstrtab.push_back('\0');

	const u32 strtab_sz   = static_cast<u32>(strtab.size());
	const u32 symtab_sz   = static_cast<u32>(sym_list.size()) * 24;
	const u32 shstrtab_sz = static_cast<u32>(shstrtab.size());

	// File layout: segments | .strtab | .symtab | .shstrtab | section headers
	const u32 strtab_off   = data_start + seg_total;
	const u32 symtab_off   = strtab_off + strtab_sz;
	const u32 shstrtab_off = symtab_off + symtab_sz;
	const u32 shoff        = shstrtab_off + shstrtab_sz;

	// Section layout: [0] NULL, [1..num_phdr] one .text per PT_LOAD,
	// [num_phdr+1] .symtab, [num_phdr+2] .strtab, [num_phdr+3] .shstrtab
	const u32 symtab_sec  = num_phdr + 1;
	const u32 strtab_sec  = num_phdr + 2;
	const u32 shstrtab_sec = num_phdr + 3;
	const u32 num_secs    = num_phdr + 4;

	const u32 file_size = shoff + num_secs * 64;

	std::vector<u8> elf(file_size, 0);

	const auto put16 = [&](usz off, u16 v)
	{
		elf[off]     = v >> 8;
		elf[off + 1] = v & 0xFF;
	};
	const auto put32 = [&](usz off, u32 v)
	{
		elf[off]     = v >> 24;
		elf[off + 1] = (v >> 16) & 0xFF;
		elf[off + 2] = (v >> 8) & 0xFF;
		elf[off + 3] = v & 0xFF;
	};
	const auto put64 = [&](usz off, u64 v)
	{
		elf[off]     = static_cast<u8>(v >> 56);
		elf[off + 1] = static_cast<u8>(v >> 48);
		elf[off + 2] = static_cast<u8>(v >> 40);
		elf[off + 3] = static_cast<u8>(v >> 32);
		elf[off + 4] = static_cast<u8>(v >> 24);
		elf[off + 5] = static_cast<u8>(v >> 16);
		elf[off + 6] = static_cast<u8>(v >> 8);
		elf[off + 7] = static_cast<u8>(v);
	};

	// ELF64 header
	elf[0] = 0x7F; elf[1] = 'E'; elf[2] = 'L'; elf[3] = 'F';
	elf[4] = 2; elf[5] = 2; elf[6] = 1; // ELFCLASS64, ELFDATA2MSB, EV_CURRENT

	put16(0x10, 2);                           // e_type = ET_EXEC
	put16(0x12, 21);                          // e_machine = EM_PPC64
	put32(0x14, 1);                           // e_version
	put64(0x18, 0);                           // e_entry
	put64(0x20, ehdr_size);                   // e_phoff
	put64(0x28, shoff);                       // e_shoff
	put32(0x30, 0);                           // e_flags
	put16(0x34, ehdr_size);                   // e_ehsize
	put16(0x36, phdr_size);                   // e_phentsize
	put16(0x38, static_cast<u16>(num_phdr));  // e_phnum
	put16(0x3A, 64);                          // e_shentsize
	put16(0x3C, static_cast<u16>(num_secs));  // e_shnum
	put16(0x3E, static_cast<u16>(shstrtab_sec)); // e_shstrndx

	// Program headers
	for (u32 i = 0; i < num_phdr; i++)
	{
		const ppu_segment& seg = *segs[i];
		const usz ph = ehdr_size + static_cast<usz>(i) * phdr_size;
		put32(ph + 0x00, 1);                          // PT_LOAD
		put32(ph + 0x04, seg.flags);                  // p_flags
		put64(ph + 0x08, seg.filesz ? seg_foff[i] : 0u); // p_offset
		put64(ph + 0x10, seg.addr);                   // p_vaddr
		put64(ph + 0x18, seg.addr);                   // p_paddr
		put64(ph + 0x20, seg.filesz);                 // p_filesz
		put64(ph + 0x28, seg.size);                   // p_memsz
		put64(ph + 0x30, 0x10000);                    // p_align
	}

	// Segment data from PS3 VM
	for (u32 i = 0; i < num_phdr; i++)
	{
		const ppu_segment& seg = *segs[i];
		if (seg.filesz && vm::check_addr(seg.addr, vm::page_readable, seg.filesz))
			std::memcpy(elf.data() + seg_foff[i], vm::base(seg.addr), seg.filesz);
	}

	// .strtab data
	std::memcpy(elf.data() + strtab_off, strtab.data(), strtab_sz);

	// .symtab data — ELF64 Sym (24 bytes, big-endian)
	for (u32 i = 0; i < static_cast<u32>(sym_list.size()); i++)
	{
		const usz soff = symtab_off + static_cast<usz>(i) * 24;
		const SymEntry& s = sym_list[i];
		put32(soff + 0x00, s.name_off);           // st_name
		elf[soff + 0x04] = (i == 0) ? 0 : 0x12;  // st_info: STB_GLOBAL|STT_FUNC
		elf[soff + 0x05] = 0;                      // st_other
		put16(soff + 0x06, s.shndx);              // st_shndx → .text[N] or SHN_ABS
		put64(soff + 0x08, static_cast<u64>(s.addr)); // st_value
		put64(soff + 0x10, static_cast<u64>(s.size)); // st_size
	}

	// .shstrtab data
	std::memcpy(elf.data() + shstrtab_off, shstrtab.data(), shstrtab_sz);

	// Section headers — [0] NULL (zeroed), [1..num_phdr] .text per segment,
	// [num_phdr+1] .symtab, [num_phdr+2] .strtab, [num_phdr+3] .shstrtab
	for (u32 i = 0; i < num_phdr; i++)
	{
		const usz sh = shoff + static_cast<usz>(i + 1) * 64;
		put32(sh + 0x00, text_shname);          // sh_name
		put32(sh + 0x04, 1);                    // sh_type = SHT_PROGBITS
		put64(sh + 0x08, 6);                    // sh_flags = SHF_ALLOC|SHF_EXECINSTR
		put64(sh + 0x10, segs[i]->addr);        // sh_addr  (matches p_vaddr exactly)
		put64(sh + 0x18, seg_foff[i]);          // sh_offset (matches p_offset exactly)
		put64(sh + 0x20, segs[i]->filesz);      // sh_size  (matches p_filesz exactly)
		put64(sh + 0x30, 0x10000);              // sh_addralign
	}

	{
		const usz sh = shoff + static_cast<usz>(symtab_sec) * 64;
		put32(sh + 0x00, symtab_shname);
		put32(sh + 0x04, 2);                    // SHT_SYMTAB
		put64(sh + 0x18, symtab_off);
		put64(sh + 0x20, symtab_sz);
		put32(sh + 0x28, strtab_sec);           // sh_link → .strtab
		put32(sh + 0x2C, 1);                    // sh_info: one local (NULL)
		put64(sh + 0x30, 8);
		put64(sh + 0x38, 24);
	}

	{
		const usz sh = shoff + static_cast<usz>(strtab_sec) * 64;
		put32(sh + 0x00, strtab_shname);
		put32(sh + 0x04, 3);                    // SHT_STRTAB
		put64(sh + 0x18, strtab_off);
		put64(sh + 0x20, strtab_sz);
		put64(sh + 0x30, 1);
	}

	{
		const usz sh = shoff + static_cast<usz>(shstrtab_sec) * 64;
		put32(sh + 0x00, shstrtab_shname);
		put32(sh + 0x04, 3);                    // SHT_STRTAB
		put64(sh + 0x18, shstrtab_off);
		put64(sh + 0x20, shstrtab_sz);
		put64(sh + 0x30, 1);
	}

	return elf;
}

bool gdb_thread::cmd_vfile(gdb_cmd& cmd)
{
	GDB.warning("vFile cmd.data = '%s'", cmd.data);

	// vFile:setfs:pid — select filesystem; 0 = target filesystem, always accept
	if (cmd.data.starts_with(":setfs:"))
	{
		return send_cmd_ack("F0");
	}

	// vFile:open:hexpath,flags,mode  (cmd.data starts with ":" from packet parser)
	if (cmd.data.starts_with(":open:"))
	{
		const std::string_view args = std::string_view(cmd.data).substr(6);
		const usz comma = args.find(',');
		if (comma == umax) return send_cmd_ack("F-1,2");

		// Decode hex-encoded path
		const std::string_view hex_path = args.substr(0, comma);
		std::string path;
		path.reserve(hex_path.size() / 2);
		for (usz i = 0; i + 1 < hex_path.size(); i += 2)
		{
			u8 byte = 0;
			std::from_chars(hex_path.data() + i, hex_path.data() + i + 2, byte, 16);
			path += static_cast<char>(byte);
		}

		// Strip "target:" prefix if GDB included it in the encoded path
		if (path.starts_with("target:"))
			path = path.substr(7);

		// Match by basename only — library names were emitted as "target:basename"
		const usz slash = path.find_last_of("/\\");
		const std::string filename = (slash == umax) ? path : path.substr(slash + 1);
		if (filename.empty()) return send_cmd_ack("F-1,2");

		GDB.warning("vFile:open requested for '%s'", filename);

		// Find the matching module and build its synthetic ELF
		std::vector<u8> elf_data;

		if (const auto main_mod = g_fxo->try_get<main_ppu_module<lv2_obj>>())
		{
			std::string main_name = main_mod->name;
			if (main_name.empty() && !main_mod->path.empty())
			{
				const usz s = main_mod->path.find_last_of("/\\");
				main_name = (s == umax) ? main_mod->path : main_mod->path.substr(s + 1);
			}
			GDB.warning("vFile:open main_mod name='%s' path='%s' segs=%d", main_name, main_mod->path, main_mod->segs.size());
			if (main_name == filename)
			{
				// Use import stub names as named_syms so HLE-intercepted functions appear named
				const auto& stubs = ppu_get_stub_code_names();
				std::map<u32, std::string> named(stubs.begin(), stubs.end());
				elf_data = build_module_elf(*main_mod, named);
				GDB.warning("vFile:open main_mod ELF size=%d named=%d", elf_data.size(), named.size());
			}
		}

		if (elf_data.empty())
		{
			idm::select<lv2_obj, lv2_prx>([&](u32 id, lv2_prx& prx)
			{
				if (!elf_data.empty()) return;
				const usz n_sl = prx.name.find_last_of("/\\");
				const std::string prx_base = (n_sl == umax) ? prx.name : prx.name.substr(n_sl + 1);
				if (prx_base == filename)
				{
					GDB.warning("vFile:open PRX id=%d name='%s' funcs=%d", id, prx.name, prx.funcs.size());
					auto named = collect_prx_exports(prx);
					// Also merge import stub names so imports from this PRX are visible
					const auto& stubs = ppu_get_stub_code_names();
					named.insert(stubs.begin(), stubs.end());
					GDB.warning("vFile:open PRX exports+stubs=%d", named.size());
					elf_data = build_module_elf(prx, named);
					GDB.warning("vFile:open PRX ELF size=%d", elf_data.size());
				}
			});
		}

		if (elf_data.empty())
		{
			GDB.warning("vFile:open: no module found for '%s'", filename);
			return send_cmd_ack("F-1,2"); // ENOENT
		}

		const int fd = vfile_next_fd++;
		const usz elf_size = elf_data.size();
		vfile_handles[fd] = std::move(elf_data);
		GDB.warning("vFile:open assigned fd=%d elf_size=%d", fd, elf_size);
		return send_cmd_ack(fmt::format("F%x", fd));
	}

	// vFile:pread:fd,count,offset
	if (cmd.data.starts_with(":pread:"))
	{
		const std::string_view args = std::string_view(cmd.data).substr(7);
		u32 fd_val = 0, count = 0;
		u64 offset = 0;

		const char* p = args.data();
		const char* end = args.data() + args.size();
		auto [p1, e1] = std::from_chars(p, end, fd_val, 16);
		if (e1 != std::errc{} || p1 >= end || *p1 != ',') return send_cmd_ack("F-1,9");
		p = p1 + 1;
		auto [p2, e2] = std::from_chars(p, end, count, 16);
		if (e2 != std::errc{} || p2 >= end || *p2 != ',') return send_cmd_ack("F-1,9");
		p = p2 + 1;
		auto [p3, e3] = std::from_chars(p, end, offset, 16);
		if (e3 != std::errc{}) return send_cmd_ack("F-1,9");

		auto it = vfile_handles.find(static_cast<int>(fd_val));
		GDB.warning("vFile:pread fd=%d count=%d offset=%lld found=%d", fd_val, count, offset, it != vfile_handles.end());
		if (it == vfile_handles.end()) return send_cmd_ack("F-1,9");

		const std::vector<u8>& data = it->second;
		GDB.warning("vFile:pread data.size=%d offset=%lld", data.size(), offset);
		if (offset >= data.size()) return send_cmd_ack("F0;");

		const usz actual = std::min<usz>(count, data.size() - static_cast<usz>(offset));
		GDB.warning("vFile:pread sending %d bytes", actual);

		// "Fcount;" prefix followed by raw binary bytes.
		// append_encoded_char handles RSP escaping (#, $, }, *) for each byte.
		std::string response = fmt::format("F%x;", actual);
		response.append(reinterpret_cast<const char*>(data.data() + static_cast<usz>(offset)), actual);
		return send_cmd_ack(response);
	}

	// vFile:close:fd
	if (cmd.data.starts_with(":close:"))
	{
		u32 fd_val = 0;
		const std::string_view args = std::string_view(cmd.data).substr(7);
		std::from_chars(args.data(), args.data() + args.size(), fd_val, 16);
		vfile_handles.erase(static_cast<int>(fd_val));
		return send_cmd_ack("F0");
	}

	// vFile:fstat:fd — return a minimal stat with the file size
	if (cmd.data.starts_with(":fstat:"))
	{
		u32 fd_val = 0;
		const std::string_view args = std::string_view(cmd.data).substr(7);
		std::from_chars(args.data(), args.data() + args.size(), fd_val, 16);

		auto it = vfile_handles.find(static_cast<int>(fd_val));
		GDB.warning("vFile:fstat fd=%d found=%d size=%d", fd_val, it != vfile_handles.end(), it != vfile_handles.end() ? it->second.size() : 0);
		if (it == vfile_handles.end()) return send_cmd_ack("F-1,9");

		// GDB remote fileio fio_stat — 64 bytes, no padding (spec explicit):
		//   0x00  fst_dev    (4)
		//   0x04  fst_ino    (4)
		//   0x08  fst_mode   (4)  — 0x000081A4 = S_IFREG | 0644
		//   0x0C  fst_nlink  (4)
		//   0x10  fst_uid    (4)
		//   0x14  fst_gid    (4)
		//   0x18  fst_rdev   (4)
		//   0x1C  fst_size   (8)  ← immediately after fst_rdev, NO padding
		//   0x24  fst_blksize(8)
		//   0x2C  fst_blocks (8)
		//   0x34  fst_atime  (4)
		//   0x38  fst_mtime  (4)
		//   0x3C  fst_ctime  (4)
		std::vector<u8> stat_buf(64, 0);
		// fst_mode: S_IFREG | 0644 = 0x000081A4
		stat_buf[0x0A] = 0x81;
		stat_buf[0x0B] = 0xA4;
		// fst_nlink: 1
		stat_buf[0x0F] = 1;
		// fst_size at 0x1C (8 bytes big-endian)
		const u64 sz = it->second.size();
		for (int i = 0; i < 8; i++)
		{
			stat_buf[0x1C + i] = static_cast<u8>(sz >> (56 - 8 * i));
		}

		std::string response = "F0;";
		response.append(reinterpret_cast<const char*>(stat_buf.data()), stat_buf.size());
		return send_cmd_ack(response);
	}

	return send_cmd_ack("F-1,88"); // ENOSYS for unknown vFile subcommands
}

#define PROCESS_CMD(cmds,handler) if (cmd.cmd == cmds) { if (!handler(cmd)) break; else continue; }

gdb_thread::gdb_thread() noexcept
{
}

gdb_thread::~gdb_thread()
{
	if (server_socket != -1)
	{
		closesocket(server_socket);
	}

	if (client_socket != -1)
	{
		closesocket(client_socket);
	}
}

void gdb_thread::operator()()
{
	start_server();

	for (u64 sleep_until = get_system_time(); server_socket != -1 && thread_ctrl::state() != thread_state::aborting;)
	{
		sockaddr_in client;
		socklen_t client_len = sizeof(client);
		client_socket = static_cast<int>(accept(server_socket, reinterpret_cast<struct sockaddr*>(&client), &client_len));

		if (client_socket == -1)
		{
			if (check_errno_again())
			{
				thread_ctrl::wait_until(&sleep_until, 5000);
				continue;
			}

			GDB.error("Could not establish new connection.");
			return;
		}
		//stop immediately
		if (Emu.IsRunning())
		{
			Emu.Pause();
		}

		{
			char hostbuf[32];
			inet_ntop(client.sin_family, reinterpret_cast<void*>(&client.sin_addr), hostbuf, 32);
			GDB.success("Got connection to GDB debug server from %s:%d.", hostbuf, client.sin_port);

			gdb_cmd cmd;

			while (thread_ctrl::state() != thread_state::aborting)
			{
				if (!read_cmd(cmd))
				{
					break;
				}
				GDB.warning("CMD: cmd='%s' data='%s'", cmd.cmd, cmd.data);
				PROCESS_CMD("!", cmd_extended_mode);
				PROCESS_CMD("?", cmd_reason);
				PROCESS_CMD("qSupported", cmd_supported);
				PROCESS_CMD("qfThreadInfo", cmd_thread_info);
				PROCESS_CMD("qsThreadInfo", cmd_thread_info_continued);
				PROCESS_CMD("qC", cmd_current_thread);
				PROCESS_CMD("p", cmd_read_register);
				PROCESS_CMD("P", cmd_write_register);
				PROCESS_CMD("m", cmd_read_memory);
				PROCESS_CMD("M", cmd_write_memory);
				PROCESS_CMD("g", cmd_read_all_registers);
				PROCESS_CMD("G", cmd_write_all_registers);
				PROCESS_CMD("H", cmd_set_thread_ops);
				PROCESS_CMD("T", cmd_thread_alive);
				PROCESS_CMD("qAttached", cmd_attached_to_what);
				PROCESS_CMD("qXfer", cmd_qxfer);
				// qSymbol:: = GDB offering symbol lookup; OK = we don't need any
				if (cmd.cmd == "qSymbol") { if (!send_cmd_ack("OK")) break; else continue; }
				// vMustReplyEmpty = probe for unknown-v-packet behaviour; must reply empty
				if (cmd.cmd == "vMustReplyEmpty") { if (!send_cmd_ack("")) break; else continue; }
				// qTStatus = tracepoint status; we have none
				if (cmd.cmd == "qTStatus") { if (!send_cmd_ack("")) break; else continue; }
				// qOffsets = text/data/bss relocation offsets; PS3 loads at fixed addresses
				if (cmd.cmd == "qOffsets") { if (!send_cmd_ack("Text=0;Data=0;Bss=0")) break; else continue; }
				// qThreadStopInfo<tid> — per-thread stop state; the hex tid is appended directly with no separator
				if (cmd.cmd.starts_with("qThreadStopInfo"))
				{
					const std::string tid_str = cmd.cmd.substr(std::string_view("qThreadStopInfo").size());
					u64 tid = 0;
					try { tid = hex_to_u64(tid_str); } catch (...) {}
					std::string reply = "E01";
					const auto find_thread = [&](u32, cpu_thread& cpu) -> bool
					{
						if (cpu.id != tid) return false;
						const u32 pc = cpu.get_pc();
						if (cpu.get_class() == thread_class::ppu)
							reply = fmt::format("T05thread:%.16x;40:%s;", tid, u64_to_padded_hex(static_cast<u64>(pc)));
						else
							reply = fmt::format("T05thread:%.16x;", tid);
						return true;
					};
					idm::select<named_thread<ppu_thread>>(find_thread) ||
					idm::select<named_thread<spu_thread>>(find_thread);
					if (!send_cmd_ack(reply)) break; else continue;
				}
				PROCESS_CMD("k", cmd_kill);
				PROCESS_CMD("D", cmd_detach);
				PROCESS_CMD("vCont?", cmd_continue_support);
				PROCESS_CMD("vCont", cmd_vcont);
				PROCESS_CMD("vFile", cmd_vfile);
				PROCESS_CMD("z", cmd_remove_breakpoint);
				PROCESS_CMD("Z", cmd_set_breakpoint);

				GDB.warning("Unsupported command received: cmd='%s' data='%s'", cmd.cmd, cmd.data);
				if (!send_cmd_ack(""))
				{
					break;
				}
			}
		}
	}
}

#undef PROCESS_CMD

void gdb_thread::pause_from(cpu_thread* t)
{
	if (paused)
	{
		return;
	}
	paused = true;
	pausedBy = t->id;
	thread_ctrl::notify(*static_cast<gdb_server*>(this));
}

#ifndef _WIN32
#undef sscanf_s
#endif

#undef HEX_U32
#undef HEX_U64
