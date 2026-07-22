#include "stdafx.h"
#include "sys_net.h"

#include "Emu/IdManager.h"
#include "Emu/Cell/PPUThread.h"
#include "Utilities/Thread.h"

#include "sys_sync.h"
#include "sys_event.h"
#include "sys_cond.h"
#include "asmjit/x86/x86operand.h"
#include "Emu/Cell/timers.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <WS2tcpip.h>
#else
#ifdef __clang__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <poll.h>
#ifdef __clang__
#pragma GCC diagnostic pop
#endif
#endif

#include "Emu/NP/np_handler.h"
#include "Emu/NP/np_helpers.h"
#include "Emu/Cell/timers.hpp"
#include <shared_mutex>

#include "sys_net/network_context.h"
#include "sys_net/lv2_socket.h"
#include "sys_net/lv2_socket_native.h"
#include "sys_net/lv2_socket_raw.h"
#include "sys_net/lv2_socket_p2p.h"
#include "sys_net/lv2_socket_p2ps.h"
#include "sys_net/sys_net_helpers.h"

LOG_CHANNEL(sys_net);
LOG_CHANNEL(sys_net_dump);

template <>
void fmt_class_string<sys_net_error>::format(std::string& out, u64 arg)
{
	format_enum(out, arg, [](auto error)
		{
			switch (static_cast<s32>(error))
			{
#define SYS_NET_ERROR_CASE(x) \
	case -x: return "-" #x;   \
	case x:                   \
		return #x
				SYS_NET_ERROR_CASE(SYS_NET_ENOENT);
				SYS_NET_ERROR_CASE(SYS_NET_EINTR);
				SYS_NET_ERROR_CASE(SYS_NET_EBADF);
				SYS_NET_ERROR_CASE(SYS_NET_ENOMEM);
				SYS_NET_ERROR_CASE(SYS_NET_EACCES);
				SYS_NET_ERROR_CASE(SYS_NET_EFAULT);
				SYS_NET_ERROR_CASE(SYS_NET_EBUSY);
				SYS_NET_ERROR_CASE(SYS_NET_EINVAL);
				SYS_NET_ERROR_CASE(SYS_NET_EMFILE);
				SYS_NET_ERROR_CASE(SYS_NET_ENOSPC);
				SYS_NET_ERROR_CASE(SYS_NET_EPIPE);
				SYS_NET_ERROR_CASE(SYS_NET_EAGAIN);
				static_assert(SYS_NET_EWOULDBLOCK == SYS_NET_EAGAIN);
				SYS_NET_ERROR_CASE(SYS_NET_EINPROGRESS);
				SYS_NET_ERROR_CASE(SYS_NET_EALREADY);
				SYS_NET_ERROR_CASE(SYS_NET_EDESTADDRREQ);
				SYS_NET_ERROR_CASE(SYS_NET_EMSGSIZE);
				SYS_NET_ERROR_CASE(SYS_NET_EPROTOTYPE);
				SYS_NET_ERROR_CASE(SYS_NET_ENOPROTOOPT);
				SYS_NET_ERROR_CASE(SYS_NET_EPROTONOSUPPORT);
				SYS_NET_ERROR_CASE(SYS_NET_EOPNOTSUPP);
				SYS_NET_ERROR_CASE(SYS_NET_EPFNOSUPPORT);
				SYS_NET_ERROR_CASE(SYS_NET_EAFNOSUPPORT);
				SYS_NET_ERROR_CASE(SYS_NET_EADDRINUSE);
				SYS_NET_ERROR_CASE(SYS_NET_EADDRNOTAVAIL);
				SYS_NET_ERROR_CASE(SYS_NET_ENETDOWN);
				SYS_NET_ERROR_CASE(SYS_NET_ENETUNREACH);
				SYS_NET_ERROR_CASE(SYS_NET_ECONNABORTED);
				SYS_NET_ERROR_CASE(SYS_NET_ECONNRESET);
				SYS_NET_ERROR_CASE(SYS_NET_ENOBUFS);
				SYS_NET_ERROR_CASE(SYS_NET_EISCONN);
				SYS_NET_ERROR_CASE(SYS_NET_ENOTCONN);
				SYS_NET_ERROR_CASE(SYS_NET_ESHUTDOWN);
				SYS_NET_ERROR_CASE(SYS_NET_ETOOMANYREFS);
				SYS_NET_ERROR_CASE(SYS_NET_ETIMEDOUT);
				SYS_NET_ERROR_CASE(SYS_NET_ECONNREFUSED);
				SYS_NET_ERROR_CASE(SYS_NET_EHOSTDOWN);
				SYS_NET_ERROR_CASE(SYS_NET_EHOSTUNREACH);
#undef SYS_NET_ERROR_CASE
			}

			return unknown;
		});
}

template <>
void fmt_class_string<lv2_socket_type>::format(std::string& out, u64 arg)
{
	format_enum(out, arg, [](auto value)
		{
			switch (value)
			{
			case SYS_NET_SOCK_STREAM: return "STREAM";
			case SYS_NET_SOCK_DGRAM: return "DGRAM";
			case SYS_NET_SOCK_RAW: return "RAW";
			case SYS_NET_SOCK_DGRAM_P2P: return "DGRAM-P2P";
			case SYS_NET_SOCK_DGRAM_ETHER: return "DGRAM-ETHER";
			case SYS_NET_SOCK_STREAM_P2P: return "STREAM-P2P";
			}

			return unknown;
		});
}

template <>
void fmt_class_string<lv2_socket_family>::format(std::string& out, u64 arg)
{
	format_enum(out, arg, [](auto value)
		{
			switch (value)
			{
			case SYS_NET_AF_UNSPEC: return "UNSPEC";
			case SYS_NET_AF_LOCAL: return "LOCAL";
			case SYS_NET_AF_INET: return "INET";
			case SYS_NET_AF_INET6: return "INET6";
			case SYS_NET_AF_ROUTE: return "ROUTE";
			}

			return unknown;
		});
}

template <>
void fmt_class_string<lv2_ip_protocol>::format(std::string& out, u64 arg)
{
	format_enum(out, arg, [](auto value)
		{
			switch (value)
			{
			case SYS_NET_IPPROTO_IP: return "IPPROTO_IP";
			case SYS_NET_IPPROTO_ICMP: return "IPPROTO_ICMP";
			case SYS_NET_IPPROTO_IGMP: return "IPPROTO_IGMP";
			case SYS_NET_IPPROTO_TCP: return "IPPROTO_TCP";
			case SYS_NET_IPPROTO_UDP: return "IPPROTO_UDP";
			case SYS_NET_IPPROTO_ICMPV6: return "IPPROTO_ICMPV6";
			}

			return unknown;
		});
}

template <>
void fmt_class_string<lv2_tcp_option>::format(std::string& out, u64 arg)
{
	format_enum(out, arg, [](auto value)
		{
			switch (value)
			{
			case SYS_NET_TCP_NODELAY: return "TCP_NODELAY";
			case SYS_NET_TCP_MAXSEG: return "TCP_MAXSEG";
			case SYS_NET_TCP_MSS_TO_ADVERTISE: return "TCP_MSS_TO_ADVERTISE";
			}

			return unknown;
		});
}

template <>
void fmt_class_string<lv2_socket_option>::format(std::string& out, u64 arg)
{
	format_enum(out, arg, [](auto value)
		{
			switch (value)
			{
			case SYS_NET_SO_SNDBUF: return "SO_SNDBUF";
			case SYS_NET_SO_RCVBUF: return "SO_RCVBUF";
			case SYS_NET_SO_SNDLOWAT: return "SO_SNDLOWAT";
			case SYS_NET_SO_RCVLOWAT: return "SO_RCVLOWAT";
			case SYS_NET_SO_SNDTIMEO: return "SO_SNDTIMEO";
			case SYS_NET_SO_RCVTIMEO: return "SO_RCVTIMEO";
			case SYS_NET_SO_ERROR: return "SO_ERROR";
			case SYS_NET_SO_TYPE: return "SO_TYPE";
			case SYS_NET_SO_NBIO: return "SO_NBIO";
			case SYS_NET_SO_TPPOLICY: return "SO_TPPOLICY";
			case SYS_NET_SO_REUSEADDR: return "SO_REUSEADDR";
			case SYS_NET_SO_KEEPALIVE: return "SO_KEEPALIVE";
			case SYS_NET_SO_BROADCAST: return "SO_BROADCAST";
			case SYS_NET_SO_LINGER: return "SO_LINGER";
			case SYS_NET_SO_OOBINLINE: return "SO_OOBINLINE";
			case SYS_NET_SO_REUSEPORT: return "SO_REUSEPORT";
			case SYS_NET_SO_ONESBCAST: return "SO_ONESBCAST";
			case SYS_NET_SO_USECRYPTO: return "SO_USECRYPTO";
			case SYS_NET_SO_USESIGNATURE: return "SO_USESIGNATURE";
			case SYS_NET_SOL_SOCKET: return "SOL_SOCKET";
			}

			return unknown;
		});
}

template <>
void fmt_class_string<lv2_ip_option>::format(std::string& out, u64 arg)
{
	format_enum(out, arg, [](auto value)
		{
			switch (value)
			{
			case SYS_NET_IP_HDRINCL: return "IP_HDRINCL";
			case SYS_NET_IP_TOS: return "IP_TOS";
			case SYS_NET_IP_TTL: return "IP_TTL";
			case SYS_NET_IP_MULTICAST_IF: return "IP_MULTICAST_IF";
			case SYS_NET_IP_MULTICAST_TTL: return "IP_MULTICAST_TTL";
			case SYS_NET_IP_MULTICAST_LOOP: return "IP_MULTICAST_LOOP";
			case SYS_NET_IP_ADD_MEMBERSHIP: return "IP_ADD_MEMBERSHIP";
			case SYS_NET_IP_DROP_MEMBERSHIP: return "IP_DROP_MEMBERSHIP";
			case SYS_NET_IP_TTLCHK: return "IP_TTLCHK";
			case SYS_NET_IP_MAXTTL: return "IP_MAXTTL";
			case SYS_NET_IP_DONTFRAG: return "IP_DONTFRAG";
			}

			return unknown;
		});
}

template <>
void fmt_class_string<struct in_addr>::format(std::string& out, u64 arg)
{
	const u8* data = reinterpret_cast<const u8*>(&get_object(arg));

	fmt::append(out, "%u.%u.%u.%u", data[0], data[1], data[2], data[3]);
}

template <>
void fmt_class_string<lv2_net_ioctl>::format(std::string& out, u64 arg)
{
	format_enum(out, arg, [](auto value)
		{
			switch (value)
			{
			case SYS_NET_SIOCSIFFLAGS: return "SIOCSIFFLAGS";
			case SYS_NET_SIOCGIFFLAGS: return "SYS_NET_SIOCGIFFLAGS";
			case SYS_NET_SIOCSIFADDR: return "SYS_NET_SIOCSIFADDR";
			case SYS_NET_SIOCSIFDSTADDR: return "SYS_NET_SIOCSIFDSTADDR";
			case SYS_NET_SIOCSIFBRDADDR: return "SYS_NET_SIOCSIFBRDADDR";
			case SYS_NET_SIOCSIFNETMASK: return "SYS_NET_SIOCSIFNETMASK";
			case SYS_NET_SIOCDIFADDR: return "SYS_NET_SIOCDIFADDR";
			case SYS_NET_SIOCAIFADDR: return "SYS_NET_SIOCAIFADDR";
			case SYS_NET_SIOCGIFADDR: return "SYS_NET_SIOCGIFADDR";
			case SYS_NET_SIOCGIFDSTADDR: return "SYS_NET_SIOCGIFDSTADDR";
			case SYS_NET_SIOCGIFBRDADDR: return "SYS_NET_SIOCGIFBRDADDR";
			case SYS_NET_SIOCGIFNETMASK: return "SYS_NET_SIOCGIFNETMASK";
			case SYS_NET_SIOCSIFMTU: return "SYS_NET_SIOCSIFMTU";
			case SYS_NET_SIOCGIFMTU: return "SYS_NET_SIOCGIFMTU";
			case SYS_NET_SIOCGIFGENERIC: return "SYS_NET_SIOCGIFGENERIC";
			case SYS_NET_SIOCSIFGENERIC: return "SYS_NET_SIOCSIFGENERIC";
			case SYS_NET_SIOCGIFTESTPARAM: return "SYS_NET_SIOCGIFTESTPARAM";
			case SYS_NET_SIOCSIFTESTPARAM: return "SYS_NET_SIOCSIFTESTPARAM";
			case SYS_NET_SIOCFLUSHMC: return "SYS_NET_SIOCFLUSHMC";
			}

			return unknown;
		});
}

lv2_socket::lv2_socket(utils::serial& ar, lv2_socket_type _type)
	: family(ar)
	, type(_type)
	, protocol(ar)
	, so_nbio(ar)
	, so_error(ar)
	, so_tcp_maxseg(ar)
#ifdef _WIN32
	, so_reuseaddr(ar)
	, so_reuseport(ar)
{
#else
{
	// Try to match structure between different platforms
	ar.pos += 8;
#endif

	[[maybe_unused]] const s32 version = GET_SERIALIZATION_VERSION(lv2_net);

	ar(so_rcvtimeo, so_sendtimeo);

	lv2_id = idm::last_id();

	ar(last_bound_addr);
}

std::function<void(void*)> lv2_socket::load(utils::serial& ar)
{
	const lv2_socket_type type{ar};

	shared_ptr<lv2_socket> sock_lv2;

	switch (type)
	{
	case SYS_NET_SOCK_STREAM:
	case SYS_NET_SOCK_DGRAM:
	{
		auto lv2_native = make_shared<lv2_socket_native>(ar, type);
		ensure(lv2_native->create_socket() >= 0);
		sock_lv2 = std::move(lv2_native);
		break;
	}
	case SYS_NET_SOCK_RAW:
	case SYS_NET_SOCK_DGRAM_ETHER: sock_lv2 = make_shared<lv2_socket_raw>(ar, type); break;
	case SYS_NET_SOCK_DGRAM_P2P: sock_lv2 = make_shared<lv2_socket_p2p>(ar, type); break;
	case SYS_NET_SOCK_STREAM_P2P: sock_lv2 = make_shared<lv2_socket_p2ps>(ar, type); break;
	}

	if (std::memcmp(&sock_lv2->last_bound_addr, std::array<u8, 16>{}.data(), 16))
	{
		// NOTE: It is allowed fail
		sock_lv2->bind(sock_lv2->last_bound_addr);
	}

	return [ptr = sock_lv2](void* storage) { *static_cast<atomic_ptr<lv2_socket>*>(storage) = ptr; };;
}

void lv2_socket::save(utils::serial& ar, bool save_only_this_class)
{
	USING_SERIALIZATION_VERSION(lv2_net);

	if (save_only_this_class)
	{
		ar(family, protocol, so_nbio, so_error, so_tcp_maxseg);
#ifdef _WIN32
		ar(so_reuseaddr, so_reuseport);
#else
		ar(std::array<char, 8>{});
#endif
		ar(so_rcvtimeo, so_sendtimeo);
		ar(last_bound_addr);
		return;
	}

	ar(type);

	switch (type)
	{
	case SYS_NET_SOCK_STREAM:
	case SYS_NET_SOCK_DGRAM:
	{
		static_cast<lv2_socket_native*>(this)->save(ar);
		break;
	}
	case SYS_NET_SOCK_RAW:
	case SYS_NET_SOCK_DGRAM_ETHER: static_cast<lv2_socket_raw*>(this)->save(ar); break;
	case SYS_NET_SOCK_DGRAM_P2P: static_cast<lv2_socket_p2p*>(this)->save(ar); break;
	case SYS_NET_SOCK_STREAM_P2P: static_cast<lv2_socket_p2ps*>(this)->save(ar); break;
	}
}

void sys_net_dump_data(std::string_view desc, const u8* data, s32 len, const void* addr)
{
	const sys_net_sockaddr_in_p2p* p2p_addr = reinterpret_cast<const sys_net_sockaddr_in_p2p*>(addr);

	if (p2p_addr)
		sys_net_dump.trace("%s(%s:%d:%d): %s", desc, np::ip_to_string(std::bit_cast<u32>(p2p_addr->sin_addr)), p2p_addr->sin_port, p2p_addr->sin_vport, fmt::buf_to_hexstring(data, len));
	else
		sys_net_dump.trace("%s: %s", desc, fmt::buf_to_hexstring(data, len));
}

error_code sys_net_bnet_accept(ppu_thread& ppu, s32 s, vm::ptr<sys_net_sockaddr> addr, vm::ptr<u32> paddrlen)
{
	ppu.state += cpu_flag::wait;

	sys_net.warning("sys_net_bnet_accept(s=%d, addr=*0x%x, paddrlen=*0x%x)", s, addr, paddrlen);

	if (addr.operator bool() != paddrlen.operator bool() || (paddrlen && *paddrlen < addr.size()))
	{
		return -SYS_NET_EINVAL;
	}

	s32 result = 0;
	sys_net_sockaddr sn_addr{};
	shared_ptr<lv2_socket> new_socket{};

	const auto sock = idm::check<lv2_socket>(s, [&, notify = lv2_obj::notify_all_t()](lv2_socket& sock)
		{
			auto [success, res, res_socket, res_addr] = sock.accept();

			if (success)
			{
				result  = res;
				sn_addr = res_addr;
				new_socket = std::move(res_socket);
				return true;
			}

			auto lock = sock.lock();

			sock.poll_queue(idm::get_unlocked<named_thread<ppu_thread>>(ppu.id), lv2_socket::poll_t::read, [&](bs_t<lv2_socket::poll_t> events) -> bool
				{
					if (events & lv2_socket::poll_t::read)
					{
						auto [success, res, res_socket, res_addr] = sock.accept(false);
						if (success)
						{
							result  = res;
							sn_addr = res_addr;
							new_socket = std::move(res_socket);
							lv2_obj::awake(&ppu);
							return success;
						}
					}

					sock.set_poll_event(lv2_socket::poll_t::read);
					return false;
				});

			lv2_obj::prepare_for_sleep(ppu);
			lv2_obj::sleep(ppu);
			return false;
		});

	if (!sock)
	{
		return -SYS_NET_EBADF;
	}

	if (!sock.ret)
	{
		while (auto state = ppu.state.fetch_sub(cpu_flag::signal))
		{
			if (is_stopped(state))
			{
				return {};
			}

			if (state & cpu_flag::signal)
			{
				break;
			}

			ppu.state.wait(state);
		}

		if (ppu.gpr[3] == static_cast<u64>(-SYS_NET_EINTR))
		{
			return -SYS_NET_EINTR;
		}

		if (result < 0)
		{
			return sys_net_error{result};
		}
	}

	if (result < 0)
	{
		return sys_net_error{result};
	}

	s32 id_ps3 = result;

	if (!id_ps3)
	{
		ensure(new_socket);
		id_ps3 = idm::import_existing<lv2_socket>(new_socket);
		if (id_ps3 == id_manager::id_traits<lv2_socket>::invalid)
		{
			return -SYS_NET_EMFILE;
		}
	}

	static_cast<void>(ppu.test_stopped());

	if (addr)
	{
		*paddrlen = sizeof(sys_net_sockaddr_in);
		*addr     = sn_addr;
	}

	// Socket ID
	return not_an_error(id_ps3);
}

error_code sys_net_bnet_bind(ppu_thread& ppu, s32 s, vm::cptr<sys_net_sockaddr> addr, u32 addrlen)
{
	ppu.state += cpu_flag::wait;

	sys_net.warning("sys_net_bnet_bind(s=%d, addr=*0x%x, addrlen=%u)", s, addr, addrlen);

	if (!addr || addrlen < addr.size())
	{
		return -SYS_NET_EINVAL;
	}

	if (!idm::check_unlocked<lv2_socket>(s))
	{
		return -SYS_NET_EBADF;
	}

	const sys_net_sockaddr sn_addr = *addr;

	// 0 presumably defaults to AF_INET(to check?)
	if (sn_addr.sa_family != SYS_NET_AF_INET && sn_addr.sa_family != SYS_NET_AF_UNSPEC)
	{
		sys_net.error("sys_net_bnet_bind: unsupported sa_family (%d)", sn_addr.sa_family);
		return -SYS_NET_EAFNOSUPPORT;
	}

	const auto sock = idm::check<lv2_socket>(s, [&, notify = lv2_obj::notify_all_t()](lv2_socket& sock) -> s32
		{
			return sock.bind(sn_addr);
		});

	if (!sock)
	{
		return -SYS_NET_EBADF;
	}

	if (sock.ret)
	{
		return sys_net_error{sock.ret};
	}

	return CELL_OK;
}

error_code sys_net_bnet_connect(ppu_thread& ppu, s32 s, vm::ptr<sys_net_sockaddr> addr, u32 addrlen)
{
	ppu.state += cpu_flag::wait;

	sys_net.warning("sys_net_bnet_connect(s=%d, addr=*0x%x, addrlen=%u)", s, addr, addrlen);

	if (!addr || addrlen < addr.size())
	{
		return -SYS_NET_EINVAL;
	}

	if (addr->sa_family != SYS_NET_AF_INET)
	{
		sys_net.error("sys_net_bnet_connect(s=%d): unsupported sa_family (%d)", s, addr->sa_family);
		return -SYS_NET_EAFNOSUPPORT;
	}

	if (!idm::check_unlocked<lv2_socket>(s))
	{
		return -SYS_NET_EBADF;
	}

	s32 result               = 0;
	sys_net_sockaddr sn_addr = *addr;

	const auto sock = idm::check<lv2_socket>(s, [&, notify = lv2_obj::notify_all_t()](lv2_socket& sock)
		{
			const auto success = sock.connect(sn_addr);

			if (success)
			{
				result = *success;
				return true;
			}

			auto lock = sock.lock();

			sock.poll_queue(idm::get_unlocked<named_thread<ppu_thread>>(ppu.id), lv2_socket::poll_t::write, [&](bs_t<lv2_socket::poll_t> events) -> bool
				{
					if (events & lv2_socket::poll_t::write)
					{
						result = sock.connect_followup();

						lv2_obj::awake(&ppu);
						return true;
					}
					sock.set_poll_event(lv2_socket::poll_t::write);
					return false;
				});

			lv2_obj::prepare_for_sleep(ppu);
			lv2_obj::sleep(ppu);
			return false;
		});

	if (!sock)
	{
		return -SYS_NET_EBADF;
	}

	if (sock.ret)
	{
		if (result < 0)
		{
			return sys_net_error{result};
		}

		return not_an_error(result);
	}

	while (auto state = ppu.state.fetch_sub(cpu_flag::signal))
	{
		if (is_stopped(state))
		{
			return {};
		}

		if (state & cpu_flag::signal)
		{
			break;
		}

		ppu.state.wait(state);
	}

	if (ppu.gpr[3] == static_cast<u64>(-SYS_NET_EINTR))
	{
		return -SYS_NET_EINTR;
	}

	if (result)
	{
		if (result < 0)
		{
			return sys_net_error{result};
		}

		return not_an_error(result);
	}

	return CELL_OK;
}

error_code sys_net_bnet_getpeername(ppu_thread& ppu, s32 s, vm::ptr<sys_net_sockaddr> addr, vm::ptr<u32> paddrlen)
{
	ppu.state += cpu_flag::wait;

	sys_net.warning("sys_net_bnet_getpeername(s=%d, addr=*0x%x, paddrlen=*0x%x)", s, addr, paddrlen);

	// Note: paddrlen is both an input and output argument
	if (!addr || !paddrlen || *paddrlen < addr.size())
	{
		return -SYS_NET_EINVAL;
	}

	const auto sock = idm::check<lv2_socket>(s, [&, notify = lv2_obj::notify_all_t()](lv2_socket& sock) -> s32
		{
			auto [res, sn_addr] = sock.getpeername();

			if (res == CELL_OK)
			{
				*paddrlen = sizeof(sys_net_sockaddr);
				*addr     = sn_addr;
			}

			return res;
		});

	if (!sock)
	{
		return -SYS_NET_EBADF;
	}

	if (sock.ret < 0)
	{
		return sys_net_error{sock.ret};
	}

	return CELL_OK;
}

error_code sys_net_bnet_getsockname(ppu_thread& ppu, s32 s, vm::ptr<sys_net_sockaddr> addr, vm::ptr<u32> paddrlen)
{
	ppu.state += cpu_flag::wait;

	sys_net.warning("sys_net_bnet_getsockname(s=%d, addr=*0x%x, paddrlen=*0x%x)", s, addr, paddrlen);

	// Note: paddrlen is both an input and output argument
	if (!addr || !paddrlen || *paddrlen < addr.size())
	{
		return -SYS_NET_EINVAL;
	}

	const auto sock = idm::check<lv2_socket>(s, [&, notify = lv2_obj::notify_all_t()](lv2_socket& sock) -> s32
		{
			auto [res, sn_addr] = sock.getsockname();

			if (res == CELL_OK)
			{
				*paddrlen = sizeof(sys_net_sockaddr);
				*addr     = sn_addr;
			}

			return res;
		});

	if (!sock)
	{
		return -SYS_NET_EBADF;
	}

	if (sock.ret < 0)
	{
		return sys_net_error{sock.ret};
	}

	return CELL_OK;
}

error_code sys_net_bnet_getsockopt(ppu_thread& ppu, s32 s, s32 level, s32 optname, vm::ptr<void> optval, vm::ptr<u32> optlen)
{
	ppu.state += cpu_flag::wait;

	switch (level)
	{
	case SYS_NET_SOL_SOCKET:
		sys_net.warning("sys_net_bnet_getsockopt(s=%d, level=SYS_NET_SOL_SOCKET, optname=%s, optval=*0x%x, optlen=%u)", s, static_cast<lv2_socket_option>(optname), optval, optlen);
		break;
	case SYS_NET_IPPROTO_TCP:
		sys_net.warning("sys_net_bnet_getsockopt(s=%d, level=SYS_NET_IPPROTO_TCP, optname=%s, optval=*0x%x, optlen=%u)", s, static_cast<lv2_tcp_option>(optname), optval, optlen);
		break;
	case SYS_NET_IPPROTO_IP:
		sys_net.warning("sys_net_bnet_getsockopt(s=%d, level=SYS_NET_IPPROTO_IP, optname=%s, optval=*0x%x, optlen=%u)", s, static_cast<lv2_ip_option>(optname), optval, optlen);
		break;
	default:
		sys_net.warning("sys_net_bnet_getsockopt(s=%d, level=0x%x, optname=0x%x, optval=*0x%x, optlen=%u)", s, level, optname, optval, optlen);
		break;
	}

	if (!optval || !optlen)
	{
		return -SYS_NET_EINVAL;
	}

	const u32 len = *optlen;

	if (!len)
	{
		return -SYS_NET_EINVAL;
	}

	const auto sock = idm::check<lv2_socket>(s, [&, notify = lv2_obj::notify_all_t()](lv2_socket& sock) -> s32
		{
			if (len < sizeof(s32))
			{
				return -SYS_NET_EINVAL;
			}

			const auto& [res, out_val, out_len] = sock.getsockopt(level, optname, *optlen);

			if (res == CELL_OK)
			{
				std::memcpy(optval.get_ptr(), out_val.ch, out_len);
				*optlen = out_len;
			}

			return res;
		});

	if (!sock)
	{
		return -SYS_NET_EBADF;
	}

	if (sock.ret < 0)
	{
		return sys_net_error{sock.ret};
	}

	return CELL_OK;
}

error_code sys_net_bnet_listen(ppu_thread& ppu, s32 s, s32 backlog)
{
	ppu.state += cpu_flag::wait;

	sys_net.warning("sys_net_bnet_listen(s=%d, backlog=%d)", s, backlog);

	if (backlog <= 0)
	{
		return -SYS_NET_EINVAL;
	}

	const auto sock = idm::check<lv2_socket>(s, [&, notify = lv2_obj::notify_all_t()](lv2_socket& sock) -> s32
		{
			return sock.listen(backlog);
		});

	if (!sock)
	{
		return -SYS_NET_EBADF;
	}

	if (sock.ret < 0)
	{
		return sys_net_error{sock.ret};
	}

	return CELL_OK;
}

error_code sys_net_bnet_recvfrom(ppu_thread& ppu, s32 s, vm::ptr<void> buf, u32 len, s32 flags, vm::ptr<sys_net_sockaddr> addr, vm::ptr<u32> paddrlen)
{
	ppu.state += cpu_flag::wait;

	sys_net.trace("sys_net_bnet_recvfrom(s=%d, buf=*0x%x, len=%u, flags=0x%x, addr=*0x%x, paddrlen=*0x%x)", s, buf, len, flags, addr, paddrlen);

	// If addr is null, paddrlen must be null as well
	if (!buf || !len || addr.operator bool() != paddrlen.operator bool())
	{
		return -SYS_NET_EINVAL;
	}

	if (flags & ~(SYS_NET_MSG_PEEK | SYS_NET_MSG_DONTWAIT | SYS_NET_MSG_WAITALL | SYS_NET_MSG_USECRYPTO | SYS_NET_MSG_USESIGNATURE))
	{
		fmt::throw_exception("sys_net_bnet_recvfrom(s=%d): unknown flags (0x%x)", flags);
	}

	s32 result = 0;
	sys_net_sockaddr sn_addr{};

	const auto sock = idm::check<lv2_socket>(s, [&, notify = lv2_obj::notify_all_t()](lv2_socket& sock)
		{
			const auto success = sock.recvfrom(flags, len);

			if (success)
			{
				const auto& [res, vec, res_addr] = *success;
				if (res > 0)
				{
					sn_addr = res_addr;
					std::memcpy(buf.get_ptr(), vec.data(), res);
					sys_net_dump_data("recvfrom", vec.data(), res, &res_addr);
				}

				result = res;
				return true;
			}

			auto lock = sock.lock();

			sock.poll_queue(idm::get_unlocked<named_thread<ppu_thread>>(ppu.id), lv2_socket::poll_t::read, [&](bs_t<lv2_socket::poll_t> events) -> bool
				{
					if (events & lv2_socket::poll_t::read)
					{
						const auto success = sock.recvfrom(flags, len, false);

						if (success)
						{
							const auto& [res, vec, res_addr] = *success;
							if (res > 0)
							{
								sn_addr = res_addr;
								std::memcpy(buf.get_ptr(), vec.data(), res);
								sys_net_dump_data("recvfrom", vec.data(), res, &res_addr);
							}
							result = res;
							lv2_obj::awake(&ppu);
							return true;
						}
					}

					if (sock.so_rcvtimeo && get_guest_system_time() - ppu.start_time > sock.so_rcvtimeo)
					{
						result = -SYS_NET_EWOULDBLOCK;
						lv2_obj::awake(&ppu);
						return true;
					}

					sock.set_poll_event(lv2_socket::poll_t::read);
					return false;
				});

			lv2_obj::prepare_for_sleep(ppu);
			lv2_obj::sleep(ppu);
			return false;
		});

	if (!sock)
	{
		return -SYS_NET_EBADF;
	}

	if (!sock.ret)
	{

		while (auto state = ppu.state.fetch_sub(cpu_flag::signal))
		{
			if (is_stopped(state))
			{
				return {};
			}

			if (state & cpu_flag::signal)
			{
				break;
			}

			ppu.state.wait(state);
		}

		if (ppu.gpr[3] == static_cast<u64>(-SYS_NET_EINTR))
		{
			return -SYS_NET_EINTR;
		}
	}

	static_cast<void>(ppu.test_stopped());

	if (result == -SYS_NET_EWOULDBLOCK)
	{
		return not_an_error(result);
	}

	if (result >= 0)
	{
		if (addr)
		{
			*paddrlen = sizeof(sys_net_sockaddr_in);
			*addr     = sn_addr;
		}

		return not_an_error(result);
	}

	return sys_net_error{result};
}

error_code sys_net_bnet_recvmsg(ppu_thread& ppu, s32 s, vm::ptr<sys_net_msghdr> msg, s32 flags)
{
	ppu.state += cpu_flag::wait;

	sys_net.todo("sys_net_bnet_recvmsg(s=%d, msg=*0x%x, flags=0x%x)", s, msg, flags);
	return CELL_OK;
}

error_code sys_net_bnet_sendmsg(ppu_thread& ppu, s32 s, vm::cptr<sys_net_msghdr> msg, s32 flags)
{
	ppu.state += cpu_flag::wait;

	sys_net.warning("sys_net_bnet_sendmsg(s=%d, msg=*0x%x, flags=0x%x)", s, msg, flags);

	if (flags & ~(SYS_NET_MSG_DONTWAIT | SYS_NET_MSG_WAITALL | SYS_NET_MSG_USECRYPTO | SYS_NET_MSG_USESIGNATURE))
	{
		fmt::throw_exception("sys_net_bnet_sendmsg(s=%d): unknown flags (0x%x)", flags);
	}

	s32 result{};

	const auto sock = idm::check<lv2_socket>(s, [&](lv2_socket& sock)
		{
			auto netmsg = msg.get_ptr();
			const auto success = sock.sendmsg(flags, *netmsg);

			if (success)
			{
				result = *success;

				return true;
			}

			sock.poll_queue(idm::get_unlocked<named_thread<ppu_thread>>(ppu.id), lv2_socket::poll_t::write, [&](bs_t<lv2_socket::poll_t> events) -> bool
				{
					if (events & lv2_socket::poll_t::write)
					{
						const auto success = sock.sendmsg(flags, *netmsg, false);

						if (success)
						{
							result = *success;
							lv2_obj::awake(&ppu);
							return true;
						}
					}

					sock.set_poll_event(lv2_socket::poll_t::write);
					return false;
				});

			lv2_obj::sleep(ppu);
			return false;
		});

	if (!sock)
	{
		return -SYS_NET_EBADF;
	}

	if (!sock.ret)
	{
		while (true)
		{
			const auto state = ppu.state.fetch_sub(cpu_flag::signal);
			if (is_stopped(state) || state & cpu_flag::signal)
			{
				break;
			}
			thread_ctrl::wait_on(ppu.state, state);
		}

		if (ppu.gpr[3] == static_cast<u64>(-SYS_NET_EINTR))
		{
			return -SYS_NET_EINTR;
		}
	}

	if (result >= 0 || result == -SYS_NET_EWOULDBLOCK)
	{
		return not_an_error(result);
	}


	return sys_net_error{result};
}

error_code sys_net_bnet_sendto(ppu_thread& ppu, s32 s, vm::cptr<void> buf, u32 len, s32 flags, vm::cptr<sys_net_sockaddr> addr, u32 addrlen)
{
	ppu.state += cpu_flag::wait;

	sys_net.trace("sys_net_bnet_sendto(s=%d, buf=*0x%x, len=%u, flags=0x%x, addr=*0x%x, addrlen=%u)", s, buf, len, flags, addr, addrlen);

	if (flags & ~(SYS_NET_MSG_DONTWAIT | SYS_NET_MSG_WAITALL | SYS_NET_MSG_USECRYPTO | SYS_NET_MSG_USESIGNATURE))
	{
		fmt::throw_exception("sys_net_bnet_sendto(s=%d): unknown flags (0x%x)", flags);
	}

	if (addr && addrlen < sizeof(sys_net_sockaddr))
	{
		sys_net.error("sys_net_bnet_sendto(s=%d): bad addrlen (%u)", s, addrlen);
		return -SYS_NET_EINVAL;
	}

	if (addr && addr->sa_family != SYS_NET_AF_INET)
	{
		sys_net.error("sys_net_bnet_sendto(s=%d): unsupported sa_family (%d)", s, addr->sa_family);
		return -SYS_NET_EAFNOSUPPORT;
	}

	sys_net_dump_data("sendto", static_cast<const u8*>(buf.get_ptr()), len, addr ? addr.get_ptr() : nullptr);

	const std::optional<sys_net_sockaddr> sn_addr = addr ? std::optional<sys_net_sockaddr>(*addr) : std::nullopt;
	const std::vector<u8> buf_copy(vm::_ptr<const char>(buf.addr()), vm::_ptr<const char>(buf.addr()) + len);
	s32 result{};

	const auto sock = idm::check<lv2_socket>(s, [&, notify = lv2_obj::notify_all_t()](lv2_socket& sock)
		{
			auto success = sock.sendto(flags, buf_copy, sn_addr);

			if (success)
			{
				result = *success;
				return true;
			}

			auto lock = sock.lock();

			// Enable write event
			sock.poll_queue(idm::get_unlocked<named_thread<ppu_thread>>(ppu.id), lv2_socket::poll_t::write, [&](bs_t<lv2_socket::poll_t> events) -> bool
				{
					if (events & lv2_socket::poll_t::write)
					{
						auto success = sock.sendto(flags, buf_copy, sn_addr, false);
						if (success)
						{
							result = *success;
							lv2_obj::awake(&ppu);
							return true;
						}
					}

					if (sock.so_sendtimeo && get_guest_system_time() - ppu.start_time > sock.so_sendtimeo)
					{
						result = -SYS_NET_EWOULDBLOCK;
						lv2_obj::awake(&ppu);
						return true;
					}

					sock.set_poll_event(lv2_socket::poll_t::write);
					return false;
				});

			lv2_obj::prepare_for_sleep(ppu);
			lv2_obj::sleep(ppu);
			return false;
		});

	if (!sock)
	{
		return -SYS_NET_EBADF;
	}

	if (!sock.ret)
	{
		while (true)
		{
			const auto state = ppu.state.fetch_sub(cpu_flag::signal);
			if (is_stopped(state) || state & cpu_flag::signal)
			{
				break;
			}
			ppu.state.wait(state);
		}

		if (ppu.gpr[3] == static_cast<u64>(-SYS_NET_EINTR))
		{
			return -SYS_NET_EINTR;
		}
	}

	if (result >= 0 || result == -SYS_NET_EWOULDBLOCK)
	{
		return not_an_error(result);
	}

	return sys_net_error{result};
}

error_code sys_net_bnet_setsockopt(ppu_thread& ppu, s32 s, s32 level, s32 optname, vm::cptr<void> optval, u32 optlen)
{
	ppu.state += cpu_flag::wait;

	switch (level)
	{
	case SYS_NET_SOL_SOCKET:
		sys_net.warning("sys_net_bnet_setsockopt(s=%d, level=SYS_NET_SOL_SOCKET, optname=%s, optval=*0x%x, optlen=%u)", s, static_cast<lv2_socket_option>(optname), optval, optlen);
		break;
	case SYS_NET_IPPROTO_TCP:
		sys_net.warning("sys_net_bnet_setsockopt(s=%d, level=SYS_NET_IPPROTO_TCP, optname=%s, optval=*0x%x, optlen=%u)", s, static_cast<lv2_tcp_option>(optname), optval, optlen);
		break;
	case SYS_NET_IPPROTO_IP:
		sys_net.warning("sys_net_bnet_setsockopt(s=%d, level=SYS_NET_IPPROTO_IP, optname=%s, optval=*0x%x, optlen=%u)", s, static_cast<lv2_ip_option>(optname), optval, optlen);
		break;
	default:
		sys_net.warning("sys_net_bnet_setsockopt(s=%d, level=0x%x, optname=0x%x, optval=*0x%x, optlen=%u)", s, level, optname, optval, optlen);
		break;
	}

	switch (optlen)
	{
	case 1:
		sys_net.warning("optval: 0x%02X", *static_cast<const u8*>(optval.get_ptr()));
		break;
	case 2:
		sys_net.warning("optval: 0x%04X", *static_cast<const be_t<u16>*>(optval.get_ptr()));
		break;
	case 4:
		sys_net.warning("optval: 0x%08X", *static_cast<const be_t<u32>*>(optval.get_ptr()));
		break;
	case 8:
		sys_net.warning("optval: 0x%016X", *static_cast<const be_t<u64>*>(optval.get_ptr()));
		break;
	}

	if (optlen < sizeof(s32))
	{
		return -SYS_NET_EINVAL;
	}

	std::vector<u8> optval_copy(vm::_ptr<u8>(optval.addr()), vm::_ptr<u8>(optval.addr() + optlen));

	const auto sock = idm::check<lv2_socket>(s, [&, notify = lv2_obj::notify_all_t()](lv2_socket& sock) -> s32
		{
			return sock.setsockopt(level, optname, optval_copy);
		});

	if (!sock)
	{
		return -SYS_NET_EBADF;
	}

	if (sock.ret < 0)
	{
		return sys_net_error{sock.ret};
	}

	return not_an_error(sock.ret);
}

error_code sys_net_bnet_shutdown(ppu_thread& ppu, s32 s, s32 how)
{
	ppu.state += cpu_flag::wait;

	sys_net.warning("sys_net_bnet_shutdown(s=%d, how=%d)", s, how);

	if (how < 0 || how > 2)
	{
		return -SYS_NET_EINVAL;
	}

	const auto sock = idm::check<lv2_socket>(s, [&, notify = lv2_obj::notify_all_t()](lv2_socket& sock) -> s32
		{
			return sock.shutdown(how);
		});

	if (!sock)
	{
		return -SYS_NET_EBADF;
	}

	if (sock.ret < 0)
	{
		return sys_net_error{sock.ret};
	}

	return CELL_OK;
}

error_code sys_net_bnet_socket(ppu_thread& ppu, lv2_socket_family family, lv2_socket_type type, lv2_ip_protocol protocol)
{
	ppu.state += cpu_flag::wait;

	sys_net.warning("sys_net_bnet_socket(family=%s, type=%s, protocol=%s)", family, type, protocol);

	if (family != SYS_NET_AF_INET && family != SYS_NET_AF_ROUTE)
	{
		sys_net.error("sys_net_bnet_socket(): unknown family (%d)", family);
	}

	if (type != SYS_NET_SOCK_STREAM && type != SYS_NET_SOCK_DGRAM && type != SYS_NET_SOCK_RAW && type != SYS_NET_SOCK_DGRAM_P2P && type != SYS_NET_SOCK_DGRAM_ETHER && type != SYS_NET_SOCK_STREAM_P2P)
	{
		sys_net.error("sys_net_bnet_socket(): unsupported type (%d)", type);
		return -SYS_NET_EPROTONOSUPPORT;
	}

	if (family == SYS_NET_AF_ROUTE)
	{
		//sys_net.todo("REEEEEE");
	}

	shared_ptr<lv2_socket> sock_lv2;

	switch (type)
	{
	case SYS_NET_SOCK_STREAM:
	case SYS_NET_SOCK_DGRAM:
	{
		auto lv2_native = make_shared<lv2_socket_native>(family, type, protocol);
		if (s32 result = lv2_native->create_socket(); result < 0)
		{
			return sys_net_error{result};
		}

		sock_lv2 = std::move(lv2_native);
		break;
	}
	case SYS_NET_SOCK_RAW:
	case SYS_NET_SOCK_DGRAM_ETHER: sock_lv2 = make_shared<lv2_socket_raw>(family, type, protocol); break;
	case SYS_NET_SOCK_DGRAM_P2P: sock_lv2 = make_shared<lv2_socket_p2p>(family, type, protocol); break;
	case SYS_NET_SOCK_STREAM_P2P: sock_lv2 = make_shared<lv2_socket_p2ps>(family, type, protocol); break;
	}

	const s32 s = idm::import_existing<lv2_socket>(sock_lv2);

	// Can't allocate more than 1000 sockets
	if (s == id_manager::id_traits<lv2_socket>::invalid)
	{
		return -SYS_NET_EMFILE;
	}

	sock_lv2->set_lv2_id(s);

	return not_an_error(s);
}

error_code sys_net_bnet_close(ppu_thread& ppu, s32 s)
{
	ppu.state += cpu_flag::wait;

	sys_net.warning("sys_net_bnet_close(s=%d)", s);

	auto sock = idm::withdraw<lv2_socket>(s);

	if (!sock)
	{
		return -SYS_NET_EBADF;
	}

	if (sock->get_queue_size())
	{
		sock->abort_socket(0);
	}

	sock->close();

	{
		// Ensures the socket has no lingering copy from the network thread
		std::lock_guard nw_lock(g_fxo->get<network_context>().mutex_thread_loop);
		sock.reset();
	}

	return CELL_OK;
}

error_code sys_net_bnet_poll(ppu_thread& ppu, vm::ptr<sys_net_pollfd> fds, s32 nfds, s32 ms)
{
	ppu.state += cpu_flag::wait;

	sys_net.trace("sys_net_bnet_poll(fds=*0x%x, nfds=%d, ms=%d)", fds, nfds, ms);

	if (nfds <= 0)
	{
		return not_an_error(0);
	}

	atomic_t<s32> signaled{0};

	u64 timeout = ms < 0 ? 0 : ms * 1000ull;

	std::vector<sys_net_pollfd> fds_buf;

	{
		fds_buf.assign(fds.get_ptr(), fds.get_ptr() + nfds);

		lv2_obj::prepare_for_sleep(ppu);

		std::unique_lock nw_lock(g_fxo->get<network_context>().mutex_thread_loop);
		std::shared_lock lock(id_manager::g_mutex);

		std::vector<::pollfd> _fds(nfds);
#ifdef _WIN32
		std::vector<bool> connecting(nfds);
#endif

		for (s32 i = 0; i < nfds; i++)
		{
			_fds[i].fd         = -1;
			fds_buf[i].revents = 0;

			if (fds_buf[i].fd < 0)
			{
				continue;
			}

			if (auto sock = idm::check_unlocked<lv2_socket>(fds_buf[i].fd))
			{
				sock->poll(fds_buf[i], _fds[i]);
#ifdef _WIN32
				connecting[i] = sock->is_connecting();
#endif
			}
			else
			{
				fds_buf[i].revents |= SYS_NET_POLLNVAL;
			}
		}

#ifdef _WIN32
		windows_poll(_fds, nfds, 0, connecting);
#else
		::poll(_fds.data(), nfds, 0);
#endif
		for (s32 i = 0; i < nfds; i++)
		{
			if (_fds[i].revents & (POLLIN | POLLHUP))
				fds_buf[i].revents |= SYS_NET_POLLIN;
			if (_fds[i].revents & POLLOUT)
				fds_buf[i].revents |= SYS_NET_POLLOUT;
			if (_fds[i].revents & POLLERR)
				fds_buf[i].revents |= SYS_NET_POLLERR;

			if (fds_buf[i].revents)
			{
				signaled++;
			}
		}

		if (ms == 0 || signaled)
		{
			lock.unlock();
			nw_lock.unlock();
			std::memcpy(fds.get_ptr(), fds_buf.data(), nfds * sizeof(sys_net_pollfd));
			return not_an_error(signaled);
		}

		for (s32 i = 0; i < nfds; i++)
		{
			if (fds_buf[i].fd < 0)
			{
				continue;
			}

			if (auto sock = idm::check_unlocked<lv2_socket>(fds_buf[i].fd))
			{
				auto lock = sock->lock();

#ifdef _WIN32
				sock->set_connecting(connecting[i]);
#endif

				bs_t<lv2_socket::poll_t> selected = +lv2_socket::poll_t::error;

				if (fds_buf[i].events & SYS_NET_POLLIN)
					selected += lv2_socket::poll_t::read;
				if (fds_buf[i].events & SYS_NET_POLLOUT)
					selected += lv2_socket::poll_t::write;
				// if (fds_buf[i].events & SYS_NET_POLLPRI) // Unimplemented
				//	selected += lv2_socket::poll::error;

				sock->poll_queue(idm::get_unlocked<named_thread<ppu_thread>>(ppu.id), selected, [sock, selected, &fds_buf, i, &signaled, &ppu](bs_t<lv2_socket::poll_t> events)
					{
						if (events & selected)
						{
							if (events & selected & lv2_socket::poll_t::read)
								fds_buf[i].revents |= SYS_NET_POLLIN;
							if (events & selected & lv2_socket::poll_t::write)
								fds_buf[i].revents |= SYS_NET_POLLOUT;
							if (events & selected & lv2_socket::poll_t::error)
								fds_buf[i].revents |= SYS_NET_POLLERR;

							signaled++;
							sock->queue_wake(&ppu);
							return true;
						}

						sock->set_poll_event(selected);
						return false;
					});
			}
		}

		lv2_obj::sleep(ppu, timeout);
	}

	bool has_timedout = false;

	while (auto state = ppu.state.fetch_sub(cpu_flag::signal))
	{
		if (is_stopped(state))
		{
			return {};
		}

		if (state & cpu_flag::signal)
		{
			break;
		}

		if (timeout)
		{
			if (lv2_obj::wait_timeout(timeout, &ppu))
			{
				// Wait for rescheduling
				if (ppu.check_state())
				{
					return {};
				}

				has_timedout = network_clear_queue(ppu);
				clear_ppu_to_awake(ppu);
				ppu.state -= cpu_flag::signal;
				break;
			}
		}
		else
		{
			ppu.state.wait(state);
		}
	}

	if (!has_timedout && !signaled)
	{
		return -SYS_NET_EINTR;
	}

	std::memcpy(fds.get_ptr(), fds_buf.data(), nfds * sizeof(fds[0]));

	return not_an_error(signaled);
}

error_code sys_net_bnet_select(ppu_thread& ppu, s32 nfds, vm::ptr<sys_net_fd_set> readfds, vm::ptr<sys_net_fd_set> writefds, vm::ptr<sys_net_fd_set> exceptfds, vm::ptr<sys_net_timeval> _timeout)
{
	ppu.state += cpu_flag::wait;

	sys_net.trace("sys_net_bnet_select(nfds=%d, readfds=*0x%x, writefds=*0x%x, exceptfds=*0x%x, timeout=*0x%x(%d:%d))", nfds, readfds, writefds, exceptfds, _timeout, _timeout ? _timeout->tv_sec.value() : 0, _timeout ? _timeout->tv_usec.value() : 0);

	atomic_t<s32> signaled{0};

	if (exceptfds)
	{
		struct log_t
		{
			atomic_t<bool> logged = false;
		};

		if (!g_fxo->get<log_t>().logged.exchange(true))
		{
			sys_net.error("sys_net_bnet_select(): exceptfds not implemented");
		}
	}

	sys_net_fd_set rread{}, _readfds{};
	sys_net_fd_set rwrite{}, _writefds{};
	sys_net_fd_set rexcept{}, _exceptfds{};
	u64 timeout = !_timeout ? 0 : _timeout->tv_sec * 1000000ull + _timeout->tv_usec;

	if (nfds > 0 && nfds <= 1024)
	{
		if (readfds)
			_readfds = *readfds;
		if (writefds)
			_writefds = *writefds;
		if (exceptfds)
			_exceptfds = *exceptfds;

		std::lock_guard nw_lock(g_fxo->get<network_context>().mutex_thread_loop);
		reader_lock lock(id_manager::g_mutex);

		std::vector<::pollfd> _fds(nfds);
#ifdef _WIN32
		std::vector<bool> connecting(nfds);
#endif

		for (s32 i = 0; i < nfds; i++)
		{
			_fds[i].fd = -1;
			bs_t<lv2_socket::poll_t> selected{};

			if (readfds && _readfds.bit(i))
				selected += lv2_socket::poll_t::read;
			if (writefds && _writefds.bit(i))
				selected += lv2_socket::poll_t::write;
			// if (exceptfds && _exceptfds.bit(i))
			//	selected += lv2_socket::poll::error;

			if (selected)
			{
				selected += lv2_socket::poll_t::error;
			}
			else
			{
				continue;
			}

			if (auto sock = idm::check_unlocked<lv2_socket>((lv2_socket::id_base & -1024) + i))
			{
				auto [read_set, write_set, except_set] = sock->select(selected, _fds[i]);

				if (read_set || write_set || except_set)
				{
					signaled++;
				}

				if (read_set)
				{
					rread.set(i);
				}

				if (write_set)
				{
					rwrite.set(i);
				}

				if (except_set)
				{
					rexcept.set(i);
				}

#ifdef _WIN32
				connecting[i] = sock->is_connecting();
#endif
			}
			else
			{
				return -SYS_NET_EBADF;
			}
		}

#ifdef _WIN32
		windows_poll(_fds, nfds, 0, connecting);
#else
		::poll(_fds.data(), nfds, 0);
#endif
		for (s32 i = 0; i < nfds; i++)
		{
			bool sig = false;
			if ((_fds[i].revents & (POLLIN | POLLHUP | POLLERR)) && _readfds.bit(i))
				sig = true, rread.set(i);
			if ((_fds[i].revents & (POLLOUT | POLLERR)) && _writefds.bit(i))
				sig = true, rwrite.set(i);

			if (sig)
			{
				signaled++;
			}
		}

		if ((_timeout && !timeout) || signaled)
		{
			if (readfds)
				*readfds = rread;
			if (writefds)
				*writefds = rwrite;
			if (exceptfds)
				*exceptfds = rexcept;
			return not_an_error(signaled);
		}

		for (s32 i = 0; i < nfds; i++)
		{
			bs_t<lv2_socket::poll_t> selected{};

			if (readfds && _readfds.bit(i))
				selected += lv2_socket::poll_t::read;
			if (writefds && _writefds.bit(i))
				selected += lv2_socket::poll_t::write;
			// if (exceptfds && _exceptfds.bit(i))
			//	selected += lv2_socket::poll_t::error;

			if (selected)
			{
				selected += lv2_socket::poll_t::error;
			}
			else
			{
				continue;
			}

			if (auto sock = idm::check_unlocked<lv2_socket>((lv2_socket::id_base & -1024) + i))
			{
				auto lock = sock->lock();
#ifdef _WIN32
				sock->set_connecting(connecting[i]);
#endif

				sock->poll_queue(idm::get_unlocked<named_thread<ppu_thread>>(ppu.id), selected, [sock, selected, i, &rread, &rwrite, &rexcept, &signaled, &ppu](bs_t<lv2_socket::poll_t> events)
					{
						if (events & selected)
						{
							if (selected & lv2_socket::poll_t::read && events & (lv2_socket::poll_t::read + lv2_socket::poll_t::error))
								rread.set(i);
							if (selected & lv2_socket::poll_t::write && events & (lv2_socket::poll_t::write + lv2_socket::poll_t::error))
								rwrite.set(i);
							// if (events & (selected & lv2_socket::poll::error))
						    //	rexcept.set(i);

							signaled++;
							sock->queue_wake(&ppu);
							return true;
						}

						sock->set_poll_event(selected);
						return false;
					});
			}
			else
			{
				return -SYS_NET_EBADF;
			}
		}

		lv2_obj::sleep(ppu, timeout);
	}
	else
	{
		return -SYS_NET_EINVAL;
	}

	bool has_timedout = false;

	while (auto state = ppu.state.fetch_sub(cpu_flag::signal))
	{
		if (is_stopped(state))
		{
			return {};
		}

		if (state & cpu_flag::signal)
		{
			break;
		}

		if (timeout)
		{
			if (lv2_obj::wait_timeout(timeout, &ppu))
			{
				// Wait for rescheduling
				if (ppu.check_state())
				{
					return {};
				}

				has_timedout = network_clear_queue(ppu);
				clear_ppu_to_awake(ppu);
				ppu.state -= cpu_flag::signal;
				break;
			}
		}
		else
		{
			ppu.state.wait(state);
		}
	}

	if (!has_timedout && !signaled)
	{
		return -SYS_NET_EINTR;
	}

	if (readfds)
		*readfds = rread;
	if (writefds)
		*writefds = rwrite;
	if (exceptfds)
		*exceptfds = rexcept;

	return not_an_error(signaled);
}

error_code _sys_net_open_dump(ppu_thread& ppu, s32 len, s32 flags)
{
	ppu.state += cpu_flag::wait;

	sys_net.todo("_sys_net_open_dump(len=%d, flags=0x%x)", len, flags);
	return CELL_OK;
}

error_code _sys_net_read_dump(ppu_thread& ppu, s32 id, vm::ptr<void> buf, s32 len, vm::ptr<s32> pflags)
{
	ppu.state += cpu_flag::wait;

	sys_net.todo("_sys_net_read_dump(id=0x%x, buf=*0x%x, len=%d, pflags=*0x%x)", id, buf, len, pflags);
	return CELL_OK;
}

error_code _sys_net_close_dump(ppu_thread& ppu, s32 id, vm::ptr<s32> pflags)
{
	ppu.state += cpu_flag::wait;

	sys_net.todo("_sys_net_close_dump(id=0x%x, pflags=*0x%x)", id, pflags);
	return CELL_OK;
}

error_code _sys_net_write_dump(ppu_thread& ppu, s32 id, vm::cptr<void> buf, s32 len, u32 unknown)
{
	ppu.state += cpu_flag::wait;

	sys_net.todo("_sys_net_write_dump(id=0x%x, buf=*0x%x, len=%d, unk=0x%x)", id, buf, len, unknown);
	return CELL_OK;
}

error_code lv2_socket::abort_socket(s32 flags)
{
	decltype(queue) qcopy;
	{
		std::lock_guard lock(mutex);

		if (queue.empty())
		{
			if (flags & SYS_NET_ABORT_STRICT_CHECK)
			{
				// Strict error checking: ENOENT if nothing happened
				return -SYS_NET_ENOENT;
			}

			// TODO: Abort the subsequent function called on this socket (need to investigate correct behaviour)
			return CELL_OK;
		}

		qcopy = std::move(queue);
		queue = {};
		events.store({});
	}

	for (auto& [ppu, _] : qcopy)
	{
		if (!ppu)
			continue;

		// Avoid possible double signaling
		network_clear_queue(*ppu);
		clear_ppu_to_awake(*ppu);

		sys_net.warning("lv2_socket::abort_socket(): waking up \"%s\": (func: %s, r3=0x%x, r4=0x%x, r5=0x%x, r6=0x%x)", ppu->get_name(), ppu->current_function, ppu->gpr[3], ppu->gpr[4], ppu->gpr[5], ppu->gpr[6]);
		ppu->gpr[3] = static_cast<u64>(-SYS_NET_EINTR);
		lv2_obj::append(ppu.get());
	}

	const u32 num_waiters = ::size32(qcopy);
	if (num_waiters && (type == SYS_NET_SOCK_STREAM || type == SYS_NET_SOCK_DGRAM))
	{
		auto& nc = g_fxo->get<network_context>();
		const u32 prev_value = nc.num_polls.fetch_sub(num_waiters);
		ensure(prev_value >= num_waiters);
	}

	lv2_obj::awake_all();
	return CELL_OK;
}

error_code sys_net_abort(ppu_thread& ppu, s32 type, u64 arg, s32 flags)
{
	ppu.state += cpu_flag::wait;

	sys_net.warning("sys_net_abort(type=%d, arg=0x%x, flags=0x%x)", type, arg, flags);

	enum abort_type : s32
	{
		_socket,
		resolver,
		type_2, // ??
		type_3, // ??
		all,
	};

	switch (type)
	{
	case _socket:
	{
		std::lock_guard nw_lock(g_fxo->get<network_context>().mutex_thread_loop);

		const auto sock = idm::get_unlocked<lv2_socket>(static_cast<u32>(arg));

		if (!sock)
		{
			return -SYS_NET_EBADF;
		}

		return sock->abort_socket(flags);
	}
	case all:
	{
		std::vector<u32> sockets;

		idm::select<lv2_socket>([&](u32 id, lv2_socket&)
		{
			sockets.emplace_back(id);
		});

		s32 failed = 0;

		for (u32 id : sockets)
		{
			const auto sock = idm::withdraw<lv2_socket>(id);

			if (!sock)
			{
				failed++;
				continue;
			}

			if (sock->get_queue_size())
				sys_net.error("ABORT 4");

			sock->close();

			sys_net.success("lv2_socket::handle_abort(): Closed socket %d", id);
		}

		// Ensures the socket has no lingering copy from the network thread
		g_fxo->get<network_context>().mutex_thread_loop.lock_unlock();

		return not_an_error(::narrow<s32>(sockets.size()) - failed);
	}
	case resolver:
	case type_2:
	case type_3:
	{
		break;
	}
	default: return -SYS_NET_EINVAL;
	}

	return CELL_OK;
}

// Per-emulation-session state for sys_net_infoctl.
// Stored via g_fxo so it resets cleanly on emulator stop/restart.
struct bnet_netctl_state
{
	shared_mutex mutex;

	// SET_BNET_SYNC (cmd 53): LV2 mutex+cond IDs the bnet layer uses to wake up
	// a PPU thread that is blocking on a network operation.
	u32 sync_mutex_id = 0;
	u32 sync_cond_id  = 0;

	// CREATE/DESTROY_LIBNETCTL_QUEUE (cmd 55/56): event port IDs keyed by
	// interface name. The port is connected to the userland event queue so
	// libnetctl can receive async link-state / DHCP events.
	std::unordered_map<std::string, u32> queue_ports;
};

struct net_infoctl_cmd_9_t
{
	be_t<u32> zero;
	vm::bptr<char> server_name;
	// More (TODO)
};

// Generic 32-byte argument block passed to sys_net_infoctl (via copyin in LV2).
// Each field is 8 bytes, big-endian. Usage varies per cmd — see infoctl_docs.
struct infoctl_arg_t
{
	be_t<u64> field0;  // +0x00: often a user pointer or combined (mutex_hi:cond_lo)
	be_t<u64> field1;  // +0x08: often size or extra param
	be_t<u64> field2;  // +0x10: often a second user pointer (e.g. interface name)
	be_t<u64> field3;  // +0x18: usually unused
};


error_code sys_net_infoctl(ppu_thread& ppu, s32 cmd, vm::ptr<infoctl_arg_t> arg)
{
	ppu.state += cpu_flag::wait;

	sys_net.todo("sys_net_infoctl(cmd=%s, arg=*0x%x)", cmd, arg);

	// todo process permission check

	if (!arg)
	{
		//sys_net_set_errorno(SYS_NET_EFAULT);
		return -SYS_NET_EFAULT;
	}

	if (cmd < 4 || cmd > 100)
	{
		//sys_net_set_errno(SYS_NET_EINVAL);
		return -SYS_NET_EINVAL;
	}

	// TODO
	switch (cmd)
	{
	case 4:
	{
		// SYS_NET_INFOCTL_GET_SYSTEM_TIME
		const auto  out = vm::ptr<be_t<u64>>::make(static_cast<u32>(arg->field0));
		*out = get_system_time();
		sys_net.todo("^-- SYS_NET_INFOCTL_GET_SYSTEM_TIME");
		return CELL_OK;
	}
	case 5:
	{
		// SYS_NET_INFOCTL_GET_UNIX_TIME
		const auto  out = vm::ptr<be_t<u64>>::make(static_cast<u32>(arg->field0));
		*out = get_timebased_time();
		sys_net.todo("^-- GET_UNIX_TIME");
		return CELL_OK;
	}
	case 6:
	{
		// SYS_NET_INFOCTL_GET_OPENSOCKETS / bnet_get_sockinfo
		sys_net.todo("^-- GET_OPEN_SOCKETS unimplemented");
		return CELL_OK;
	}
	case 7:
	{
		// SYS_NET_INFOCTL_GET_SOCKINFO_SA
		sys_net.todo("^-- GET_SOCKINFO_SA unimplemented");
		return CELL_OK;
	}
	case 8:
	{
		// SYS_NET_INFOCTL_GET_BNET_HEAP_STATS / bnet_get_meminfo
		sys_net.todo("^-- GET_BNET_HEAP_STATS unimplemented");
		return CELL_OK;
	}
	case 9:
	{
		// Nameserver.
		constexpr auto nameserver = "nameserver \0"sv;

		char buffer[nameserver.size() + 80]{};
		std::memcpy(buffer, nameserver.data(), nameserver.size());

		auto& nph          = g_fxo->get<named_thread<np::np_handler>>();
		const auto dns_str = np::ip_to_string(nph.get_dns_ip());
		std::memcpy(buffer + nameserver.size() - 1, dns_str.data(), dns_str.size());

		std::string_view name{buffer};
		vm::static_ptr_cast<net_infoctl_cmd_9_t>(vm::ptr<void>(arg))->zero = 0; // prob a return code?
		std::memcpy(vm::static_ptr_cast<net_infoctl_cmd_9_t>(vm::ptr<void>(arg))->server_name.get_ptr(), name.data(), name.size());

		sys_net.todo("^-- GET_NAMESERVER");

		return CELL_OK;
	}
	case 10:
	{
		// SYS_NET_INFOCTL_CMD_6 - Returns some counter todo..
		sys_net.todo("^-- GET_SOME_COUNTER unimplemented");
		return CELL_OK;
	}
	case 11:
	{
		// SYS_NET_INFOCTL_GET_INPCBTABLES
		sys_net.todo("^-- GET_INPCTABLES unimplemented");
		return CELL_OK;
	}
	case 52: // 0x34
	{
		// SYS_NET_INFOCTL_SET_NAMESERVER
		// arg->field0 = user pointer to new nameserver string (NULL = clear).
		// Kernel: frees old global nameserver string, copies new one via copyin_str.
		// Emulation: log the new nameserver string; the active DNS is managed by np_handler.
		const u32 str_ptr = static_cast<u32>(arg->field0);
		if (str_ptr)
		{
			const std::string ns(vm::ptr<char>::make(str_ptr).get_ptr());
			sys_net.todo("^-- (SET_RESOLV_CONF): \"%s\"", ns.c_str());
		}
		else
		{
			sys_net.todo("^-- (SET_RESOLV_CONF): NULL (clear)");
		}
		return CELL_OK;
	}
	case 53: // 0x35
	{
		// SYS_NET_INFOCTL_SET_BNET_SYNC
		// arg->field0 upper 32 = mutex_id, lower 32 = cond_id (both LV2 object IDs).
		// Kernel: attaches the mutex+cond to the current bnet thread's sync struct so
		// blocking socket ops can wake up the PPU thread via sys_cond_signal.
		const auto& a = *vm::static_ptr_cast<infoctl_arg_t>(arg);
		const u32 mutex_id = static_cast<u32>(a.field0 >> 32);
		const u32 cond_id  = static_cast<u32>(a.field0);

		auto& state = g_fxo->get<bnet_netctl_state>();
		{
			std::lock_guard lock(state.mutex);
			state.sync_mutex_id = mutex_id;
			state.sync_cond_id  = cond_id;
		}

		sys_net.todo("^-- (SET_BNET_SYNC): mutex_id=0x%x cond_id=0x%x", mutex_id, cond_id);
		return CELL_OK;
	}
	case 55: // 0x36
	{
		// SYS_NET_INFOCTL_CREATE_LIBNETCTL_QUEUE
		// arg->field2 = user ptr to interface name string
		// arg->field0 = event queue ID (u32 in lower bits; 0xffffffff = no queue)
		// arg->field1 = additional creation parameter
		// Kernel: creates a LOCAL event port, connects it to the specified queue, stores
		// port handle in netdev+0x58 so link/DHCP events can be sent to userland.
		const auto& a = *vm::static_ptr_cast<infoctl_arg_t>(arg);
		const u32 ifname_ptr = static_cast<u32>(a.field2);
		const u32 event_data = a.field1;
		const u32 queue_id   = static_cast<u32>(a.field0);

		const std::string ifname = ifname_ptr ? std::string(vm::ptr<char>::make(ifname_ptr).get_ptr()) : "eth0";

		if (queue_id == 0xffffffffu)
		{
			sys_net.todo("^-- (CREATE_LIBNETCTL_QUEUE): if=%s, no queue (0xffffffff)", ifname.c_str());
			return CELL_OK;
		}

		auto& state = g_fxo->get<bnet_netctl_state>();

		// Check if a port already exists for this interface.
		// _sys_net_lib_set_libnetctl_queue calls cmd=55 twice:
		//   1st call: create the port and send initial link-up + IP-assigned events.
		//   2nd call: subscription variant — send event {data1=0x20, data2=8} to signal
		//             "link negotiation complete / connected" (raw LV1 link state 8).
		//             This unblocks vsh SceNetCtl's FUN_00327b38(mask=0x2a) wait loop.
		{
			std::lock_guard lock(state.mutex);
			const auto it = state.queue_ports.find(ifname);
			if (it != state.queue_ports.end())
			{
				const u32 existing_port_id = it->second;
				//sys_event_port_send(existing_port_id, 0x20, 8, 0);
				//sys_event_port_send(existing_port_id, 2, 0, 0);
				//sys_event_port_send(existing_port_id, 0x20, 0, 0);
				//sys_event_port_send(existing_port_id, 0x10, 0, 0);
				//sys_event_port_send(existing_port_id, 0x20, 0x4, 0x20); // This seems to work but also causes it to think its in pppoe error mode xD

				// 20 8 0 = cable disconnected / network down
				// 20 4 20 = cable up / network up ?

				//sys_event_port_send(existing_port_id, 0x20, 8, 0);
				sys_event_port_send(existing_port_id, 0x20, 4, 20); // Cable connected?
				sys_event_port_send(existing_port_id, 0x10, 0, 0);

				sys_net.todo("^-- (CREATE_LIBNETCTL_QUEUE): if=%s subscription call → sent {data1=0x20, data2=8} on port=0x%x", ifname.c_str(), existing_port_id);
				return CELL_OK;
			}
		}

		// First call: create a LOCAL event port.
		const u32 port_id = idm::make<lv2_obj, lv2_event_port>(SYS_EVENT_PORT_LOCAL, SYS_EVENT_PORT_NO_NAME);
		if (!port_id)
		{
			sys_net.error("^-- (CREATE_LIBNETCTL_QUEUE): if=%s failed to allocate event port", ifname.c_str());
			return -SYS_NET_EINVAL;
		}

		// Connect port to the userland event queue.
		{
			std::lock_guard lock(id_manager::g_mutex);
			const auto port  = idm::check_unlocked<lv2_obj, lv2_event_port>(port_id);
			auto queue = idm::get_unlocked<lv2_obj, lv2_event_queue>(queue_id);

			if (!port || !queue)
			{
				// Queue not found — destroy the port we just created.
				idm::withdraw<lv2_obj, lv2_event_port>(port_id, [](lv2_event_port&) -> CellError { return {}; });
				sys_net.error("^-- (CREATE_LIBNETCTL_QUEUE): if=%s queue 0x%x not found", ifname.c_str(), queue_id);
				return -SYS_NET_EINVAL;
			}

			port->queue = std::move(queue);
		}

		// Store port_id for this interface so cmd 56 can clean up and the second
		// call above can find it.
		{
			std::lock_guard lock(state.mutex);
			state.queue_ports[ifname] = port_id;
		}

		// Immediately synthesise the "network is ready" event sequence so the netctl
		// library does not have to wait for hardware interrupts that will never come.
		// On real hardware sys_netdev_notify() is called with these flags:
		//   0x80  — link/STA state up   (hw interrupt sub-code 0x01 → flag 0x80)
		//   0x100 — IP address assigned  (hw interrupt sub-code 0x02 → flag 0x100,
		//                                 with the assigned IP in data3)
		// Only flags whose bits fall inside the kernel mask 0x66fcc reach event ports;
		// both 0x80 and 0x100 are within that mask.
		{
			auto& nph2 = g_fxo->get<named_thread<np::np_handler>>();
			u32 local_ip = nph2.get_local_ip_addr(); // already big-endian / network order
			if (local_ip == 0)
			{
				// np_handler hasn't connected to RPCN yet — discover the local IP ourselves
				// using the same UDP-connect/getsockname trick discover_ip_address() uses.
				auto sockfd = ::socket(AF_INET, SOCK_DGRAM, 0);
#ifdef _WIN32
				if (sockfd != INVALID_SOCKET)
#else
				if (sockfd != -1)
#endif
				{
					::sockaddr_in addr{};
					addr.sin_family      = SYS_NET_AF_INET;
					addr.sin_port        = 53;
					addr.sin_addr.s_addr = 0x08080808; // 8.8.8.8
					if (::connect(sockfd, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) == 0)
					{
						::sockaddr_in name{};
						::socklen_t namelen = sizeof(name);
						if (::getsockname(sockfd, reinterpret_cast<sockaddr*>(&name), &namelen) == 0)
							local_ip = name.sin_addr.s_addr;
					}
#ifdef _WIN32
					::closesocket(sockfd);
#else
					::close(sockfd);
#endif
				}
			}
			//sys_event_port_send(port_id, 0x80, 0, 0);         // link up
			//sys_event_port_send(port_id, 0x100, local_ip, 0); // IP address assigned
			//sys_event_port_send(port_id, 0x20, 8, 0);         // link negotiation complete (unblocks state 2 FUN_00327b38 mask=0x2a)
			//sys_event_port_send(port_id, 0x08, 0, 0);         // DHCP complete (unblocks state 4 FUN_003289c0 mask=0x23a)
			//sys_event_port_send(port_id, 2, 0, 0);
			//sys_event_port_send(port_id, 0x20, 0, 0);
			//sys_event_port_send(port_id, 0x10, 0, 0);
			//sys_event_port_send(port_id, 0x20, 0x4, 0x20); // works but puts into pppoe error state
			sys_event_port_send(port_id, 0x20, 4, 20); // Cable connected?
			sys_event_port_send(port_id, 0x10, 0, 0);

			sys_net.todo("^-- (CREATE_LIBNETCTL_QUEUE): sent initial link-up+IP+DHCP events (ip=0x%08x)", local_ip);
		}

		sys_net.todo("^-- (CREATE_LIBNETCTL_QUEUE): if=%s queue=0x%x → port=0x%x", ifname.c_str(), queue_id, port_id);
		return CELL_OK;
	}
	case 56: // 0x38
	{
		// SYS_NET_INFOCTL_DESTROY_LIBNETCTL_QUEUE
		// arg->field2 = user ptr to interface name string
		// Kernel: disconnects event port from queue, then destroys the port.
		const auto& a = *vm::static_ptr_cast<infoctl_arg_t>(arg);
		const u32 ifname_ptr = static_cast<u32>(a.field2);

		const std::string ifname = ifname_ptr
			? std::string(vm::ptr<char>::make(ifname_ptr).get_ptr())
			: "eth0";

		auto& state = g_fxo->get<bnet_netctl_state>();
		u32 port_id = 0;
		{
			std::lock_guard lock(state.mutex);
			const auto it = state.queue_ports.find(ifname);
			if (it != state.queue_ports.end())
			{
				port_id = it->second;
				state.queue_ports.erase(it);
			}
		}

		if (!port_id)
		{
			sys_net.todo("^-- (DESTROY_LIBNETCTL_QUEUE): if=%s no port to destroy", ifname.c_str());
			return -SYS_NET_EINVAL; // 0xffffffffffffffea
		}

		// Disconnect port from queue.
		{
			std::lock_guard lock(id_manager::g_mutex);
			const auto port = idm::check_unlocked<lv2_obj, lv2_event_port>(port_id);
			if (port && lv2_obj::check(port->queue) && !port->is_busy)
				port->queue.reset();
		}

		// Destroy port.
		idm::withdraw<lv2_obj, lv2_event_port>(port_id, [](lv2_event_port& port) -> CellError
		{
			if (lv2_obj::check(port.queue)) return CELL_EISCONN;
			return {};
		});

		sys_net.todo("^-- (DESTROY_LIBNETCTL_QUEUE): if=%s port=0x%x destroyed", ifname.c_str(), port_id);
		return CELL_OK;
	}
	case 57:
	{
		// SYS_NET_INFOCTL_SET_PSPEMU_STRING_0
		sys_net.todo("^--- SET_PSPEMU_STRING_0 unimplemented");
		return CELL_OK;
	}
	case 58:
	{
		// SYS_NET_INFOCTL_SET_PSPEMU_STRING_1
		sys_net.todo("^-- SET_PSPEMU_STRING_1 unimplemented");
		return CELL_OK;
	}
	case 59:
	{
		// SYS_NET_INFOCTL_GET_PSPEMU_STRING_0
		sys_net.todo("^-- GET_PSPEMU_STRING_0 unimplemented");
		return CELL_OK;
	}
	case 60:
	{
		// SYS_NET_INFOCTL_GET_PSPEMU_STRING_1
		sys_net.todo("^-- GET_PSPEMU_STRING_1 unimplemented");
		return CELL_OK;
	}
	case 100:
	{
		// sys_net_bnet_show_memory_status
		return CELL_OK;
		sys_net.todo("^-- SHOW_MEMORY_STATUS unimplemented");
	}
	}

	return CELL_OK;
}

error_code sys_net_control(ppu_thread& ppu, vm::cptr<char> ifr_name, s32 cmd, vm::ptr<void> cmdbuf, s32 bufsize)
{
	ppu.state += cpu_flag::wait;

	// Kernel: sys_net_name_to_index treats null ifr_name as "eth0".
	// eth0/1 = cable, eth2 = wifi?
	const char* name = ifr_name ? ifr_name.get_ptr() : "eth0";

	switch (cmd)
	{
	case 0x80020001:
	{
		// Called from sceNetCtlGetEtherInfoVsh
		// net_control_logic_switch → net_control_something (EINVAL on real HW).
		// netctl reads this 4-byte result to determine connection state.
		// result - netctl state
		// 0x01 - 1 - Disconnected
		// 0x02 - 2 - Connecting
		// 0x04 - 3 - Negotiating / finalizing
		// 0x08 - 4 - Connected (link + IP up)
		// 0x10 - 5 -
		// 0x20 - 6 -
		// Else - Invalid
		const u32 result = 0x08;
		if (cmdbuf && bufsize >= 4)
			*vm::ptr<be_t<u32>>::make(cmdbuf.addr()) = result;
		sys_net.warning("sys_net_control(ifr=%s, cmd=LINK_STATUS): → %u", name, result);
		return CELL_OK;
	}

	case 0x80020002:
	{
		// net_get_lv1_eth_port_status_2: returns 1 << port_index if HV
		// reports link up, else 0.  For eth0 (primary wired, port 0)
		// that's 0x01 = "link up on port 0".
		//
		// Currently unused by netctl in RPCS3 (cmd 55 event synthesis
		// bypasses the polling loop), but implementing it correctly
		// protects against games that bypass libnetctl and poll directly.
		if (cmdbuf && bufsize >= 4)
			*vm::ptr<be_t<u32>>::make(cmdbuf.addr()) = 0x01;
		return CELL_OK;
	}

	case 0x81020000:
	{
		// net_control_0x81020000_set_neg_mode: sets Ethernet autoneg mode.
		// cmdbuf = be_u32 mode: 0=auto 1=10H 2=10F 4=100H 8=100F 0x10=1000H 0x20=1000F 0x80=forced/unknown?
		// Mapped to lv1_net_control cmd=3 flag word and sent to HV. No output buffer.
		if (!cmdbuf || bufsize < 4)
		{
			sys_net.warning("sys_net_control(ifr=%s, cmd=SET_NEG_MODE): missing/short cmdbuf", name);
			return CELL_OK;
		}
		const u32 mode = *vm::ptr<be_t<u32>>::make(cmdbuf.addr());
		sys_net.warning("sys_net_control(ifr=%s, cmd=SET_NEG_MODE): mode=0x%x", name, mode);
		return CELL_OK;
	}

	case 0x80080005:
	{
		// FUN_80000000000a1278: lv1_net_control cmd=5 → returns 32-bit device ID.
		// Gated on SM param bit 0x400; returns ESRCH without it on real HW.
		// Emulation: write 0x00000000 as fake device ID as this seems to be what the decr returns.
		const u32 dev_id = 0x00000000;
		if (cmdbuf && bufsize >= 4)
			*vm::ptr<be_t<u32>>::make(cmdbuf.addr()) = dev_id;
		sys_net.warning("sys_net_control(ifr=%s, cmd=GET_DEV_ID): → 0x%x", name, dev_id);
		return CELL_OK;
	}

	default:
		sys_net.todo("sys_net_control(ifr=%s, cmd=0x%x, *0x%x, bufsize=%d)", name, cmd, cmdbuf, bufsize);
		sys_net.todo("sys_net_control: unhandled cmd=0x%x", cmd);
		return CELL_OK;
	}
}

// struct ifreq — 32 bytes, PS3 bnet (sys_net_ifioctl_docs.md §Common Data Structures)
// All multi-byte fields are big-endian on PPC64.
struct ifreq
{
	char ifr_name[16];          // +0x00: interface name, null-terminated

	union                       // +0x10: ifr_ifru (16 bytes)
	{
		be_t<u16> ifr_flags;    // SIOCGIFFLAGS / SIOCSIFFLAGS
		be_t<s32> ifr_mtu;      // SIOCGIFMTU   / SIOCSIFMTU
		be_t<s32> ifr_value;    // SIOCGIFGENERIC
		be_t<u64> ifr_cap;      // SIOCGIFCAP out  (+0x10, 8 bytes)
		char      ifr_data[16]; // raw / addr commands
		struct
		{
			be_t<u64> _unused;
			be_t<u64> ifr_capenable; // +0x18: SIOCSIFCAP new_capenable in
		};
	};

	//   must be subset of ifp->if_capabilities
};                              // total: 32 bytes
static_assert(sizeof(ifreq) == 32);

// struct ifaliasreq — 64 bytes, used with SIOCAIFADDR / SIOCDIFADDR
// Each sockaddr_in: [sa_len u8][sa_family u8][port be16][addr be32][zero 8]
struct ifaliasreq
{
	char ifra_name[16];    // +0x00: interface name
	struct {               // +0x10: ifra_addr (address to add)
		u8            sa_len;
		u8            sa_family;   // AF_INET = 2
		be_t<u16>     sin_port;
		be_t<u32>     sin_addr;    // IPv4 address
		u8            sin_zero[8];
	} ifra_addr;
	struct {               // +0x20: ifra_broadaddr / ifra_dstaddr
		u8            sa_len;
		u8            sa_family;
		be_t<u16>     sin_port;
		be_t<u32>     sin_addr;    // broadcast address
		u8            sin_zero[8];
	} ifra_broadaddr;
	struct {               // +0x30: ifra_mask (netmask)
		u8            sa_len;
		u8            sa_family;
		be_t<u16>     sin_port;
		be_t<u32>     sin_addr;    // netmask
		u8            sin_zero[8];
	} ifra_mask;
};                         // total: 64 bytes
static_assert(sizeof(ifaliasreq) == 64);

// PS3 IFF flag constants (from sys_net_ifioctl RE)
enum : u16
{
	SYS_NET_IFF_UP          = 0x0001,  // interface is up
	SYS_NET_IFF_DRV_RUNNING = 0x0040,  // driver DRV_RUNNING flag
	// user-settable bits allowed through SIOCSIFFLAGS
	SYS_NET_IFF_USER_MASK   = 0x70ad,
	// kernel/driver private bits preserved on SIOCSIFFLAGS
	SYS_NET_IFF_KERNEL_MASK = 0x8f52,
};

// Fake ifnet entry for the emulator — one per virtual PS3 network interface.
// PS3 netctl always uses "eth0"; loopback "lo0" is the other known interface.
struct bnet_ifnet
{
	char     name[16];   // matches ifreq::ifr_name field width
	u16      if_flags;   // current IFF_* flags (mirrors ifp+0x4c in kernel)
};

// Emulator interface table. Initialize before first ioctl.
// eth0: UP+DRV_RUNNING when host network is available; adjust as needed.
// lo0:  always UP+DRV_RUNNING (loopback).
static std::array<bnet_ifnet, 2> g_bnet_ifnet_table = {{
	{ "eth0", SYS_NET_IFF_UP | SYS_NET_IFF_DRV_RUNNING },
	{ "lo0",  SYS_NET_IFF_UP | SYS_NET_IFF_DRV_RUNNING },
}};

static bnet_ifnet* bnet_ifunit(const char* name)
{
	for (auto& ifp : g_bnet_ifnet_table)
		if (std::strncmp(ifp.name, name, sizeof(ifp.name)) == 0)
			return &ifp;
	return nullptr;
}

// ifreq layout (from doc):
//   +0x00..+0x0f  char ifr_name[16]   — interface name
//   +0x10..+0x11  uint16_t ifr_flags  — flags in/out (union first member)
error_code sys_net_bnet_ioctl(ppu_thread& ppu, s32 socket_id, lv2_net_ioctl cmd, vm::ptr<ifreq> ifr)
{
	ppu.state += cpu_flag::wait;

	sys_net.todo("sys_net_bnet_ioctl(socket=%d, cmd=%s, ifr=0x%x)", socket_id, cmd, ifr);

	static std::unordered_map<std::string, u16> s_ifflags = {
		{"eth0", SYS_NET_IFF_UP | SYS_NET_IFF_DRV_RUNNING},
		{"lo0",  SYS_NET_IFF_UP | SYS_NET_IFF_DRV_RUNNING},
	};

	static std::unordered_map<std::string, s32> s_ifmtu = {
		{"eth0", 1500},
		{"lo0",  16384},
	};

	switch (cmd)
	{
	case SYS_NET_SIOCGIFFLAGS:
	{
		const std::string name(ifr->ifr_name, strnlen(ifr->ifr_name, 16));
		const u16 flags = s_ifflags.count(name) ? s_ifflags[name] : u16{SYS_NET_IFF_UP};
		ifr->ifr_flags = flags;

		sys_net.todo("SIOCGIFFLAGS(if=%s): 0x%04x", name.c_str(), flags);

		return CELL_OK;
	}

	case SYS_NET_SIOCSIFFLAGS:
	{
		const std::string name(ifr->ifr_name, strnlen(ifr->ifr_name, 16));
		u16& flags = s_ifflags[name];
		const u16 new_flags = ifr->ifr_flags;
		const u16 old_flags = flags;

		if ((old_flags & SYS_NET_IFF_UP) && !(new_flags & SYS_NET_IFF_UP))
			flags &= ~SYS_NET_IFF_UP;
		else if (!(old_flags & SYS_NET_IFF_UP) && (new_flags & SYS_NET_IFF_UP))
			flags |= SYS_NET_IFF_UP;

		flags = (flags & SYS_NET_IFF_KERNEL_MASK) | (new_flags & SYS_NET_IFF_USER_MASK);

		if (old_flags != flags)
			sys_net.todo("SIOCSIFFLAGS(if=%s): 0x%04x -> 0x%04x (req=0x%04x)",
				name.c_str(), old_flags, flags, new_flags);

		return CELL_OK;
	}

	// Address get — fill ifr_data (sockaddr_in: sa_len, AF_INET, port=0, addr, zero[8])
	case SYS_NET_SIOCGIFADDR:
	case SYS_NET_SIOCGIFDSTADDR:
	case SYS_NET_SIOCGIFBRDADDR:
	case SYS_NET_SIOCGIFNETMASK:
	{
		const std::string name(ifr->ifr_name, strnlen(ifr->ifr_name, 16));

		auto& nph = g_fxo->get<named_thread<np::np_handler>>();
		// get_local_ip_addr() returns be_t<u32>::operator u32() on LE host:
		// the IP bytes (network order, big-endian) end up byte-swapped as a native u32.
		// std::byteswap restores presentation order (0xc0a80102 = 192.168.1.2).
		const u32 raw    = nph.get_local_ip_addr();
		const u32 ip_h   = raw ? std::byteswap(raw) : 0xc0a80102u; // fallback 192.168.1.2
		const u32 mask_h = 0xffffff00u; // /24
		const u32 bcast_h = (ip_h & mask_h) | ~mask_h;

		u32 addr_h;
		if (cmd == SYS_NET_SIOCGIFADDR || cmd == SYS_NET_SIOCGIFDSTADDR)
			addr_h = ip_h;
		else if (cmd == SYS_NET_SIOCGIFBRDADDR)
			addr_h = bcast_h;
		else // SIOCGIFNETMASK
			addr_h = mask_h;

		// Write as be_t<u32> so PS3 big-endian memory sees correct network-order bytes.
		auto* sa = reinterpret_cast<u8*>(ifr->ifr_data);
		sa[0] = 16; sa[1] = 2; sa[2] = 0; sa[3] = 0;
		*reinterpret_cast<be_t<u32>*>(sa + 4) = addr_h;
		std::memset(sa + 8, 0, 8);

		sys_net.todo("SIOCGIF%s(if=%s): %u.%u.%u.%u",
			cmd == SYS_NET_SIOCGIFADDR ? "ADDR" : cmd == SYS_NET_SIOCGIFDSTADDR ? "DSTADDR" :
			cmd == SYS_NET_SIOCGIFBRDADDR ? "BRDADDR" : "NETMASK",
			name.c_str(),
			(addr_h >> 24) & 0xff, (addr_h >> 16) & 0xff, (addr_h >> 8) & 0xff, addr_h & 0xff);

		return CELL_OK;
	}

	// Address set — no-op in emulation (real kernel forwards to in_control)
	case SYS_NET_SIOCSIFADDR:
	case SYS_NET_SIOCSIFDSTADDR:
	case SYS_NET_SIOCSIFBRDADDR:
	case SYS_NET_SIOCSIFNETMASK:
	{
		const std::string name(ifr->ifr_name, strnlen(ifr->ifr_name, 16));
		const auto* sa  = reinterpret_cast<const u8*>(ifr->ifr_data);
		const u32   addr = *reinterpret_cast<const be_t<u32>*>(sa + 4);
		sys_net.todo("SIOCS%s(if=%s): %u.%u.%u.%u",
			cmd == SYS_NET_SIOCSIFADDR ? "IFADDR" : cmd == SYS_NET_SIOCSIFDSTADDR ? "IFDSTADDR" :
			cmd == SYS_NET_SIOCSIFBRDADDR ? "IFBRDADDR" : "IFNETMASK",
			name.c_str(),
			(addr >> 24) & 0xff, (addr >> 16) & 0xff, (addr >> 8) & 0xff, addr & 0xff);
		return CELL_OK;
	}

	case SYS_NET_SIOCGIFMTU:
	{
		const std::string name(ifr->ifr_name, strnlen(ifr->ifr_name, 16));
		const s32 mtu = s_ifmtu.count(name) ? s_ifmtu[name] : 1500;
		ifr->ifr_mtu = mtu;

		sys_net.todo("SIOCGIFMTU(if=%s): %d", name.c_str(), mtu);

		return CELL_OK;
	}

	case SYS_NET_SIOCSIFMTU:
	{
		const std::string name(ifr->ifr_name, strnlen(ifr->ifr_name, 16));
		const s32 new_mtu = ifr->ifr_mtu;

		if (new_mtu < 68 || new_mtu > 9000)
		{
			sys_net.todo("SIOCSIFMTU(if=%s): invalid MTU %d", name.c_str(), new_mtu);
			return -SYS_NET_EINVAL;
		}

		const s32 old_mtu = s_ifmtu.count(name) ? s_ifmtu[name] : 1500;
		s_ifmtu[name] = new_mtu;

		sys_net.todo("SIOCSIFMTU(if=%s): %d -> %d", name.c_str(), old_mtu, new_mtu);

		return CELL_OK;
	}

	case SYS_NET_SIOCDIFADDR:
	{
		// Delete interface address — called by DHCP client to release an IP.
		// In the kernel this falls to the protocol-layer (in_control); in emulation just log it.
		const std::string name(ifr->ifr_name, strnlen(ifr->ifr_name, 16));
		sys_net.todo("SIOCDIFADDR(if=%s): address delete request", name.c_str());
		return CELL_OK;
	}

	case SYS_NET_SIOCAIFADDR:
	{
		// Add/change interface alias address — called by DHCP client to assign an IP.
		// The argument is a 64-byte ifaliasreq: [name 16][addr 16][broadaddr 16][mask 16].
		// In the kernel this falls to in_control; in emulation store and log the assignment.
		const auto& alias = *vm::ptr<ifaliasreq>::make(ifr.addr());
		const std::string name(alias.ifra_name, strnlen(alias.ifra_name, 16));
		const u32 ip   = alias.ifra_addr.sin_addr;
		const u32 bcast = alias.ifra_broadaddr.sin_addr;
		const u32 mask  = alias.ifra_mask.sin_addr;
		sys_net.todo("SIOCAIFADDR(if=%s): ip=%u.%u.%u.%u mask=%u.%u.%u.%u bcast=%u.%u.%u.%u",
			name.c_str(),
			(ip >> 24) & 0xff, (ip >> 16) & 0xff, (ip >> 8) & 0xff, ip & 0xff,
			(mask >> 24) & 0xff, (mask >> 16) & 0xff, (mask >> 8) & 0xff, mask & 0xff,
			(bcast >> 24) & 0xff, (bcast >> 16) & 0xff, (bcast >> 8) & 0xff, bcast & 0xff);
		return CELL_OK;
	}

	case SYS_NET_SIOCGIFGENERIC:
	{
		// Get driver-generic data — forwarded to if_ioctl unchanged in the kernel.
		// No known useful data to return; zero ifr_value and succeed.
		const std::string name(ifr->ifr_name, strnlen(ifr->ifr_name, 16));
		ifr->ifr_value = 0;
		sys_net.todo("SIOCGIFGENERIC(if=%s): → 0", name.c_str());
		return CELL_OK;
	}

	case SYS_NET_SIOCSIFGENERIC:
	{
		// Set driver-generic data — forwarded to if_ioctl in kernel; no-op in emulation.
		const std::string name(ifr->ifr_name, strnlen(ifr->ifr_name, 16));
		sys_net.todo("SIOCSIFGENERIC(if=%s): value=0x%x", name.c_str(), +ifr->ifr_value);
		return CELL_OK;
	}

	case SYS_NET_SIOCGIFTESTPARAM:
	{
		// sys_net_get_test_param in libnet issues this ioctl into a 528-byte buffer,
		// then copies bytes [16..527] to the caller. Zero-fill is correct for emulation.
		sys_net.todo("SIOCGIFTESTPARAM");
		std::memset(reinterpret_cast<u8*>(ifr.get_ptr()) + 0x10, 0, 0x200);
		return CELL_OK;
	}

	case SYS_NET_SIOCSIFTESTPARAM:
	{
		// sys_net_set_test_param in libnet issues this ioctl. No-op in emulation.
		sys_net.todo("SIOCSIFTESTPARAM");
		return CELL_OK;
	}

	case SYS_NET_SIOCFLUSHMC:
	{
		// Flush all multicast group subscriptions on this socket (_IO 'P'/0xc8, no data).
		// In the kernel: FUN_8000000000161c58 walks and frees the socket's mc group list.
		// In emulation: no multicast state to flush.
		sys_net.todo("SIOCFLUSHMC: flush multicast groups");
		return CELL_OK;
	}

	// PPPOE TESTING

	case 0x8020697a:  // SIOCIFCREATE       (in: ifreq, 32B)
	case 0x80206979:  // SIOCIFDESTROY      (in: ifreq, 32B)
	case 0x8040696e:  // PPPOESETPARMS      (in: caller buffer, 64B)
	case 0x80506979:  // SPPPSETAUTHCFG     (in: caller buffer, 80B)
	case 0x80146980:  // SPPPSETAUTHFAILURE (in: ifreq-like, 20B)
	case 0x80146981:  // SPPPSETDNSOPTS     (in: ifreq-like, 20B)
		sys_net.todo("PPPoE setup ioctl 0x%x: fake success (no concentrator simulation)", cmd);
		return 0;

	case 0xc0146974:  // PPPOEGETSESSIONSTATE — out: ifreq with ifr_value = state
		// state = 1 means "PADI sent, no PADO received" — the natural error
		// when no PPPoE concentrator exists on the network.
		sys_net.todo("PPPOEGETSESSIONSTATE: reporting state=1 (NO_PADO)");
		ifr->ifr_value = 1;
		return 0;

	case 0xc014697c:  // (related PPPoE query — likely SPPPGETSTATUS variant, 20B)
		sys_net.todo("PPPoE ioctl 0xc014697c: zeroed output");
		ifr->ifr_value = 0;
		return 0;

	case 0xc0146988:  // PPPOEGETLINKSTATE — out: ifreq with ifr_value = LCP phase
		// state = 0 means LCP never came up — consistent with discovery failure.
		sys_net.todo("PPPOEGETLINKSTATE: reporting state=0 (LCP not up)");
		ifr->ifr_value = 0;
		return 0;

	case 0xc1146975:  // PPPOEGETSESSIONERRTAG (276B caller buffer)
		// Only called when SESSIONSTATE returned 2 or 4. We return 1, so this
		// shouldn't be reached. If it is, zero the caller buffer.
		sys_net.todo("PPPOEGETSESSIONERRTAG: zeroed (shouldn't be called)");
		// Need to zero the caller's 276-byte buffer through the user pointer.
		// Implementation depends on how RPCS3 marshals it; if the syscall
		// dispatcher already gives you a pointer, memset it to zero.
		return 0;


	default:
		sys_net.todo("sys_net_bnet_ioctl: unhandled cmd=0x%x", cmd);
		return CELL_OK;
	}
}

namespace sysctl_mib
{
    // CTL_* top-level
    static const char* ctl_name(s32 v)
    {
        switch (v)
        {
        case 1:  return "kern";
        case 2:  return "vm";
        case 3:  return "fs";
        case 4:  return "net";
        case 5:  return "debug";
        case 6:  return "hw";
        case 7:  return "machdep";
        case 8:  return "user";
        default: return nullptr;
        }
    }

    // PF_* under CTL_NET
    static const char* pf_name(s32 v)
    {
        switch (v)
        {
        case 0:  return "unspec";
        case 1:  return "local";
        case 2:  return "inet";
        case 17: return "route";   // PF_ROUTE = 0x11
        case 18: return "link";    // AF_LINK
        case 30: return "inet6";
        default: return nullptr;
        }
    }

    // AF_* filter (mib[3] under net.route)
    static const char* af_name(s32 v)
    {
        switch (v)
        {
        case 0:  return "unspec";  // 0 = all families
        case 2:  return "inet";
        case 18: return "link";
        case 30: return "inet6";
        default: return nullptr;
        }
    }

    // NET_RT_* commands (mib[4] under net.route)
    static const char* net_rt_name(s32 v)
    {
        switch (v)
        {
        case 0: return "noop";
        case 1: return "dump";     // NET_RT_DUMP
        case 2: return "flags";    // NET_RT_FLAGS
        case 3: return "iflist";   // NET_RT_IFLIST (old)
        case 4: return "iflist2";  // NET_RT_IFLIST2
        default: return nullptr;
        }
    }

    // Decode a sysctl MIB into a human-readable path string.
    // Returns e.g. "net.route.1.unspec.dump.0"
    static std::string decode(const be_t<s32>* ops, u32 count)
    {
        std::string out;

        for (u32 i = 0; i < count; i++)
        {
            const s32 v = ops[i];
            const char* name = nullptr;

            if (i == 0)
            {
                name = ctl_name(v);
            }
            else if (i == 1 && ops[0] == 4) // CTL_NET subtree
            {
                name = pf_name(v);
            }
            else if (i == 2 && ops[0] == 4 && ops[1] == 17) // net.route protocol
            {
                // mib[2] is the protocol number — always 1 for IP, ignored by kernel
                // render as bare integer
            }
            else if (i == 3 && ops[0] == 4 && ops[1] == 17) // net.route AF filter
            {
                name = af_name(v);
            }
            else if (i == 4 && ops[0] == 4 && ops[1] == 17) // net.route command
            {
                name = net_rt_name(v);
            }
            // mib[5] = flags filter — always bare integer

            if (i > 0)
                out += '.';

            if (name)
                out += name;
            else
                fmt::append(out, "%d", v);
        }

        return out;
    }

    // Return a brief annotation for the command (for log context).
    static const char* cmd_desc(const be_t<s32>* ops, u32 count)
    {
        if (count < 5 || ops[0] != 4 || ops[1] != 17)
            return "unknown";

        switch (+ops[4])
        {
        case 1: return "NET_RT_DUMP (routing table)";
        case 4: return "NET_RT_IFLIST (interface list)";
        default: return "NET_RT_?";
        }
    }
}

// bsd style sysctl mib
error_code sys_net_bnet_sysctl(ppu_thread& ppu, vm::cptr<be_t<s32>> mib_parts, u32 mib_parts_count, vm::ptr<void> oldp, vm::ptr<be_t<u64>> oldlenp, vm::ptr<void> newp, u32 newlen)
{

	ppu.state += cpu_flag::wait;

	//sys_net.todo("enter, val at 0xd00b51c0 = 0x%x", vm::read32(0xd00b51c0));

	sys_net.todo("r31=0x%x, *r31=0x%x, oldlenp=0x%x",
	static_cast<u32>(ppu.gpr[31]),
	vm::read32(static_cast<u32>(ppu.gpr[31])),
	oldlenp.addr());

	sys_net.todo("gpr[6]=0x%x, gpr[1]+0x70=0x%x, oldlenp.addr()=0x%x",
	static_cast<u32>(ppu.gpr[6]),
	static_cast<u32>(ppu.gpr[1]) + 0x70,
	oldlenp.addr());

	if (mib_parts && mib_parts_count)
	{
		sys_net.todo("sys_net_bnet_sysctl(path=%s [%s], oldp=*0x%x, *oldlenp=0x%x, newp=*0x%x, newlen=%u)", sysctl_mib::decode(mib_parts.get_ptr(), mib_parts_count), sysctl_mib::cmd_desc(mib_parts.get_ptr(), mib_parts_count), oldp, oldlenp, newp, newlen);
	}
	else
	{
		sys_net.todo("sys_net_bnet_sysctl(ops=null/empty, oldp=*0x%x, newp=0x%x)", oldp, newp);
	}

	auto path = sysctl_mib::decode(mib_parts.get_ptr(), mib_parts_count);

	const u8* data;
	u64 data_size;

	sys_net.todo("maow raw: 0x%x", vm::read32(oldlenp.addr()));

	if (path == "net.route.0.unspec.iflist2.0")
	{
		// Capture from devkit
		static const u8 iflist2data[] = {
			0x00, 0xb0, 0x03, 0x0f, 0x00, 0x00, 0x00, 0x10, 0xff, 0xff, 0x80, 0x09, 0x00, 0x01, 0x00, 0x00,
			0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x81, 0x70,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x1d, 0x66, 0xaa,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x09, 0xeb, 0x10, 0x18, 0x12, 0x00, 0x01, 0x18, 0x03, 0x00, 0x00,
			0x6c, 0x6f, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x3c, 0x03, 0x0c, 0x00, 0x00, 0x00, 0xa4, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x05, 0x02, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x10, 0x02, 0x00, 0x00,
			0x7f, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x02, 0x00, 0x00,
			0x7f, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xb0, 0x03, 0x0f,
			0x00, 0x00, 0x00, 0x10, 0xff, 0xff, 0x88, 0x43, 0x00, 0x02, 0x00, 0x00, 0x06, 0x06, 0x0e, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0xdc, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x98, 0x96, 0x80, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x1d, 0x66, 0xaa, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x09, 0xeb, 0x10, 0x18, 0x12, 0x00, 0x02, 0x06, 0x04, 0x06, 0x00, 0x65, 0x74, 0x68, 0x30,
			0x00, 0x15, 0xc1, 0xb0, 0xf4, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3c, 0x03, 0x0c,
			0x00, 0x00, 0x00, 0xa4, 0x00, 0x00, 0x01, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x05, 0x02, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x10, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x02, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xb0, 0x03, 0x0f, 0x00, 0x00, 0x00, 0x10,
			0xff, 0xff, 0x88, 0x02, 0x00, 0x03, 0x00, 0x00, 0x06, 0x06, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0xdc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x98, 0x96, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x18, 0x12, 0x00, 0x03, 0x06, 0x04, 0x06, 0x00, 0x65, 0x74, 0x68, 0x31, 0x00, 0x15, 0xc1, 0xb0,
			0xf4, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xb0, 0x03, 0x0f, 0x00, 0x00, 0x00, 0x10,
			0xff, 0xff, 0x88, 0x02, 0x00, 0x04, 0x00, 0x00, 0x06, 0x06, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0xdc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x98, 0x96, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x18, 0x12, 0x00, 0x04, 0x06, 0x04, 0x06, 0x00, 0x65, 0x74, 0x68, 0x32, 0x00, 0x15, 0xc1, 0xb0,
			0xf4, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		};
		data = iflist2data;
		data_size = std::size(iflist2data);
	}
	else if (path == "net.route.0.inet.dump.0")
	{
		static const u8 inetdump0data[] = {
			0x00, 0xc8, 0x03, 0x04, 0x00, 0x02, 0x5d, 0x24, 0x00, 0x00, 0x08, 0x03, 0x00, 0x00, 0x00, 0x37,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0xa8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x02, 0x00, 0x00, 0xc0, 0xa8, 0x01, 0x01,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x18, 0x12, 0x00, 0x02, 0x06, 0x04, 0x06, 0x00, 0x65, 0x74, 0x68, 0x30, 0x00, 0x15, 0xc1, 0xb0,
			0xf4, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x02, 0x00, 0x00, 0xc0, 0xa8, 0x01, 0x90,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc8, 0x03, 0x04, 0x00, 0x01, 0x5d, 0x24,
			0x00, 0x00, 0x08, 0x0b, 0x00, 0x00, 0x00, 0x37, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0xa8,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x81, 0x70,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x10, 0x02, 0x00, 0x00, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x10, 0x02, 0x00, 0x00, 0x7f, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x05, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x18, 0x12, 0x00, 0x01, 0x18, 0x03, 0x00, 0x00,
			0x6c, 0x6f, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x10, 0x02, 0x00, 0x00, 0x7f, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0xc0, 0x03, 0x04, 0x00, 0x01, 0x5d, 0x24, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x33,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0xa8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x81, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x02, 0x00, 0x00, 0x7f, 0x00, 0x00, 0x01,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x02, 0x00, 0x00, 0x7f, 0x00, 0x00, 0x01,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x12, 0x00, 0x01, 0x18, 0x03, 0x00, 0x00,
			0x6c, 0x6f, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x10, 0x02, 0x00, 0x00, 0x7f, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0xd0, 0x03, 0x04, 0x00, 0x02, 0x00, 0x01, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x37,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x0c, 0x1d, 0x66, 0xae, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x02, 0x00, 0x00, 0xc0, 0xa8, 0x01, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14, 0x12, 0x00, 0x02, 0x06, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x07, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x18, 0x12, 0x00, 0x02, 0x06, 0x04, 0x06, 0x00,
			0x65, 0x74, 0x68, 0x30, 0x00, 0x15, 0xc1, 0xb0, 0xf4, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x10, 0x02, 0x00, 0x00, 0xc0, 0xa8, 0x01, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0xc8, 0x03, 0x04, 0x00, 0x02, 0x00, 0x01, 0x00, 0x00, 0x24, 0x05, 0x00, 0x00, 0x00, 0x33,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x0c, 0x1d, 0x66, 0xae, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x02, 0x00, 0x00, 0xc0, 0xa8, 0x01, 0x01,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14, 0x12, 0x00, 0x02, 0x06, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x18, 0x12, 0x00, 0x02, 0x06, 0x04, 0x06, 0x00, 0x65, 0x74, 0x68, 0x30, 0x00, 0x15, 0xc1, 0xb0,
			0xf4, 0x1e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x02, 0x00, 0x00, 0xc0, 0xa8, 0x01, 0x90,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xd0, 0x03, 0x04, 0x00, 0x02, 0x00, 0x01,
			0x00, 0x00, 0x09, 0x01, 0x00, 0x00, 0x00, 0x37
		};
		data = inetdump0data;
		data_size = std::size(inetdump0data);
	}
	else {
		sys_net.warning("Unhandled sysctl path: %s", path);
		return CELL_OK;
	}

	if (!data || data_size == 0)
	{
		sys_net.warning("sys_net_bnet_sysctl: no data for path %s", path);
		return CELL_OK;
	}

	if (!oldp)
	{
		sys_net.todo("Size probe");
		vm::write<be_t<u64>>(oldlenp.addr(), data_size);
		sys_net.todo("wrote %d to oldlenp addr=0x%x, readback=%d", data_size, oldlenp.addr(), static_cast<u32>(*oldlenp));

		sys_net.todo("requesting: %d", data_size);

		//sys_net.todo("before return, val at 0xd00b51c0 = 0x%x", vm::read32(0xd00b51c0));

		return CELL_OK;
	}

	if (*oldlenp < data_size)
	{
		// TODO make new buffer and assign to newp?
		sys_net.todo("Help i need a bigger buffer, current size: %d, needed size: %d", *oldlenp, data_size);
		return CELL_OK;
	}

	std::memcpy(oldp.get_ptr(), data, data_size);
	return CELL_OK;
}

error_code sys_net_eurus_post_command(ppu_thread& ppu, u16 cmd, vm::ptr<u8> cmdbuf, u32 cmdbuf_size)
{
	ppu.state += cpu_flag::wait;
	// WIFI specifically

	sys_net.todo("sys_net_eurus_post_command(cmd=0x%x, buf=*0x%x, size=0x%x)", cmd, cmdbuf, cmdbuf_size);

	if (!cmdbuf)
	{
		return CELL_OK;
	}

	auto* buf = static_cast<u8*>(cmdbuf.get_ptr());

	// WiFi chip readiness poll
	if (cmd == 0xffff)
	{
		// WiFi chip readiness poll — vsh loops until buf[12:15]==1
		// buf[4:5] = BE uint16 firmware status (must be 1 or FUN_0033a638 returns error)
		// buf[12:15] = lower word of u64 at buf+8, ==1 → chip ready → loop exits
		if (cmdbuf_size >= 16)
		{
			memset(buf, 0, 16);
			buf[4]  = 0x01;  // buf[4:5] BE uint16 = 1 (fw status ok) - eurus buffer is LE
			buf[15] = 0x01;  // buf[12:15] = 1 (chip ready, read via PPC ld+cmpwi, stays BE)
		}
		return CELL_OK;
	}

	// Get Scan Results
	if (cmd == 0x1033)
	{
		// VSH FUN_0033b808: iterates buf[0x0C] AP entries starting at buf[0x0D].
		// Each entry: LE u16 body_len at [0x00], fixed fields at [0x02..0x14], then 802.11 IEs.
		// body_len is measured from entry offset 0x02 (bssid) through end of IEs.
		if (cmdbuf_size >= 0x30)
		{
			memset(buf, 0, cmdbuf_size);
			buf[4]     = 0x01;  // LE fw status = 1
			buf[0x0c]  = 1;     // 1 AP in results

			u8* ap = buf + 0x0d;

			// body_len (LE u16): 0x13 fixed (bssid+rssi+tsf+beacon+cap) + 7 (SSID IE) + 3 (DS-Param IE) = 0x1D
			ap[0x00] = 0x1d;
			ap[0x01] = 0x00;

			// bssid: locally-administered unicast MAC
			ap[0x02] = 0x02;
			ap[0x03] = 0x00;
			ap[0x04] = 0x00;
			ap[0x05] = 0x00;
			ap[0x06] = 0x00;
			ap[0x07] = 0x01;

			// ap[0x08] = 0x00: raw_rssi = 0 → 100% signal quality via rssi_to_quality
			// ap[0x09..0x10] = 0: TSF timestamp zeroed

			// beacon_interval (LE u16) = 100 TU (~102.4 ms, standard)
			ap[0x11] = 0x64;
			ap[0x12] = 0x00;

			// capability (LE u16): ESS bit set, IBSS clear → bss_mode=1 (infrastructure)
			ap[0x13] = 0x01;
			ap[0x14] = 0x00;

			// IEs at ap[0x15]: SSID before DS-Param per 802.11 ordering
			ap[0x15] = 0x00;  // tag: SSID
			ap[0x16] = 0x05;  // length
			ap[0x17] = 'R';
			ap[0x18] = 'P';
			ap[0x19] = 'C';
			ap[0x1a] = 'S';
			ap[0x1b] = '3';
			// DS-Param IE: channel
			ap[0x1c] = 0x03;
			ap[0x1d] = 0x01;
			ap[0x1e] = 0x06;  // channel 6
		}
		return CELL_OK;
	}

	// All other commands: vsh's wrapper checks buf[4:5] as LE uint16.
	// If != 1 it turns CELL_OK into 0x80130800. Always ack with status=1.
	if (cmdbuf_size >= 6)
	{
		buf[4] = 0x01; // LE low byte
		buf[5] = 0x00; // LE high byte
	}

	if (cmd == 0x1ed)
	{
		return CELL_OK;
	}

	// Chip PHY programming for speed rates (likely).
	if (cmd == 0x1025)
	{
		return CELL_OK;
	}

	if (cmd == 0x1033)
	{
		return CELL_OK;
	}



	sys_net.todo("not implemented");
	return CELL_OK;
}
