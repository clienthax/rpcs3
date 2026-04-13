#include "stdafx.h"
#include "sys_net.h"

#include "Emu/IdManager.h"
#include "Emu/Cell/PPUThread.h"
#include "Utilities/Thread.h"

#include "sys_sync.h"
#include "sys_event.h"
#include "sys_cond.h"

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

error_code sys_net_infoctl(ppu_thread& ppu, s32 cmd, vm::ptr<void> arg)
{
	ppu.state += cpu_flag::wait;

	sys_net.todo("sys_net_infoctl(cmd=%d, arg=*0x%x)", cmd, arg);

	bool scream = true;

	// TODO
	switch (cmd)
	{
	case 4:
	{
		// SYS_NET_INFOCTL_GET_SYSTEM_TIME
	}
	case 5:
	{
		// SYS_NET_INFOCTL_GET_UNIX_TIME
	}
	case 6:
	{
		// SYS_NET_INFOCTL_GET_OPENSOCKETS
	}
	case 7:
	{
		// SYS_NET_INFOCTL_GET_SOCKINFO_SA
	}
	case 8:
	{
		// SYS_NET_INFOCTL_GET_BNET_HEAP_STATS
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
		vm::static_ptr_cast<net_infoctl_cmd_9_t>(arg)->zero = 0;
		std::memcpy(vm::static_ptr_cast<net_infoctl_cmd_9_t>(arg)->server_name.get_ptr(), name.data(), name.size());
		scream = false;
		break;
	}
	case 10:
	{
		// SYS_NET_INFOCTL_CMD_6 - Returns some network device structure.. todo..
	}
	case 11:
	{
		// SYS_NET_INFOCTL_GET_INPCBTABLES
	}
	case 52: // 0x34
	{
		// SYS_NET_INFOCTL_SET_NAMESERVER
		// arg->field0 = user pointer to new nameserver string (NULL = clear).
		// Kernel: frees old global nameserver string, copies new one via copyin_str.
		// Emulation: log the new nameserver string; the active DNS is managed by np_handler.
		const auto& a = *vm::static_ptr_cast<infoctl_arg_t>(arg);
		const u32 str_ptr = static_cast<u32>(a.field0);
		if (str_ptr)
		{
			const std::string ns(vm::ptr<char>::make(str_ptr).get_ptr());
			sys_net.warning("sys_net_infoctl(SET_NAMESERVER): \"%s\"", ns.c_str());
		}
		else
		{
			sys_net.warning("sys_net_infoctl(SET_NAMESERVER): NULL (clear)");
		}
		scream = false;
		break;
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

		sys_net.warning("sys_net_infoctl(SET_BNET_SYNC): mutex_id=0x%x cond_id=0x%x", mutex_id, cond_id);
		scream = false;
		break;
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
		const u32 queue_id   = static_cast<u32>(a.field0);

		const std::string ifname = ifname_ptr
			? std::string(vm::ptr<char>::make(ifname_ptr).get_ptr())
			: "eth0";

		if (queue_id == 0xffffffffu)
		{
			sys_net.warning("sys_net_infoctl(CREATE_LIBNETCTL_QUEUE): if=%s, no queue (0xffffffff)", ifname.c_str());
			scream = false;
			break;
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
				sys_event_port_send(existing_port_id, 0x20, 0x4, 0x20);


				sys_net.warning("sys_net_infoctl(CREATE_LIBNETCTL_QUEUE): if=%s subscription call → sent {data1=0x20, data2=8} on port=0x%x", ifname.c_str(), existing_port_id);
				scream = false;
				break;
			}
		}

		// First call: create a LOCAL event port.
		const u32 port_id = idm::make<lv2_obj, lv2_event_port>(SYS_EVENT_PORT_LOCAL, SYS_EVENT_PORT_NO_NAME);
		if (!port_id)
		{
			sys_net.error("sys_net_infoctl(CREATE_LIBNETCTL_QUEUE): if=%s failed to allocate event port", ifname.c_str());
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
				sys_net.error("sys_net_infoctl(CREATE_LIBNETCTL_QUEUE): if=%s queue 0x%x not found", ifname.c_str(), queue_id);
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
					addr.sin_family      = AF_INET;
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
			sys_event_port_send(port_id, 0x20, 0x4, 0x20);


			sys_net.warning("sys_net_infoctl(CREATE_LIBNETCTL_QUEUE): sent initial link-up+IP+DHCP events (ip=0x%08x)", local_ip);
		}

		sys_net.warning("sys_net_infoctl(CREATE_LIBNETCTL_QUEUE): if=%s queue=0x%x → port=0x%x", ifname.c_str(), queue_id, port_id);
		scream = false;
		break;
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
			sys_net.warning("sys_net_infoctl(DESTROY_LIBNETCTL_QUEUE): if=%s no port to destroy", ifname.c_str());
			scream = false;
			break;
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

		sys_net.warning("sys_net_infoctl(DESTROY_LIBNETCTL_QUEUE): if=%s port=0x%x destroyed", ifname.c_str(), port_id);
		scream = false;
		break;
	}
	case 57:
	{
		// SYS_NET_INFOCTL_SET_PSPEMU_STRING_0
	}
	case 58:
	{
		// SYS_NET_INFOCTL_SET_PSPEMU_STRING_1
	}
	case 59:
	{
		// SYS_NET_INFOCTL_GET_PSPEMU_STRING_0
	}
	case 60:
	{
		// SYS_NET_INFOCTL_GET_PSPEMU_STRING_1
	}
	case 100:
	{
		// NOP ?
	}
	default: break;
	}

	if (scream)
	{
		sys_net.todo("sys_net_infoctl: unhandled cmd=%d", cmd);
	}

	return CELL_OK;
}

error_code sys_net_control(ppu_thread& ppu, vm::cptr<char> ifr_name, s32 cmd, vm::ptr<void> cmdbuf, s32 bufsize)
{
	ppu.state += cpu_flag::wait;

	// Kernel: sys_net_name_to_index treats null ifr_name as "eth0".
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

	case 0x81020000:
	{
		// net_control_0x81020000_set_neg_mode: sets Ethernet autoneg mode.
		// cmdbuf = be_u32 mode: 0=auto 1=10H 2=10F 4=100H 8=100F 0x10=1000H 0x20=1000F 0x80=forced
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
		// Emulation: write 0x13371337 as fake device ID.
		const u32 dev_id = 0x13371337;
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
	IFF_UP_          = 0x0001,  // interface is up
	IFF_DRV_RUNNING = 0x0040,  // driver DRV_RUNNING flag
	// user-settable bits allowed through SIOCSIFFLAGS
	IFF_USER_MASK   = 0x70ad,
	// kernel/driver private bits preserved on SIOCSIFFLAGS
	IFF_KERNEL_MASK = 0x8f52,
};

// IOCTL commands (BSD encoding: dir[31:30] | size[29:16] | group[15:8] | num[7:0])
// Verified against libnet.prx _sys_net_lib_ioctl whitelist (GEX 4.70).
// Group 'i' (0x69) = interface ioctls → sys_net_bnet_ioctl
// Group 'P' (0x50) = protocol/socket ioctls → socket proto handler
enum : u32
{
	// Flags
	SIOCSIFFLAGS   = 0x80206910,  // W   'i'/0x10  ifreq(32)       set interface flags
	SIOCGIFFLAGS   = 0xc0206911,  // R/W 'i'/0x11  ifreq(32)       get interface flags
	// Addresses — set (forwarded to in_control in kernel; confirmed in _sce_net_set_ip_and_mask)
	SIOCSIFADDR    = 0x8020690c,  // W   'i'/0x0c  ifreq(32)       set unicast address
	SIOCSIFDSTADDR = 0x8020690e,  // W   'i'/0x0e  ifreq(32)       set point-to-point destination address
	SIOCSIFBRDADDR = 0x80206913,  // W   'i'/0x13  ifreq(32)       set broadcast address
	SIOCSIFNETMASK = 0x80206916,  // W   'i'/0x16  ifreq(32)       set network mask
	SIOCDIFADDR    = 0x80206919,  // W   'i'/0x19  ifreq(32)       delete interface address
	SIOCAIFADDR    = 0x8040691a,  // W   'i'/0x1a  ifaliasreq(64)  add/change interface address
	// Addresses — get
	SIOCGIFADDR    = 0xc0206921,  // R/W 'i'/0x21  ifreq(32)       get unicast address
	SIOCGIFDSTADDR = 0xc0206922,  // R/W 'i'/0x22  ifreq(32)       get point-to-point dst address
	SIOCGIFBRDADDR = 0xc0206923,  // R/W 'i'/0x23  ifreq(32)       get broadcast address
	SIOCGIFNETMASK = 0xc0206925,  // R/W 'i'/0x25  ifreq(32)       get network mask
	// MTU
	SIOCSIFMTU     = 0x8020697f,  // W   'i'/0x7f  ifreq(32)       set interface MTU
	SIOCGIFMTU     = 0xc020697e,  // R/W 'i'/0x7e  ifreq(32)       get interface MTU
	// Generic driver pass-through (forwarded to if_ioctl unchanged)
	SIOCGIFGENERIC = 0xc020698c,  // R/W 'i'/0x8c  ifreq(32)       get driver-generic data
	SIOCSIFGENERIC = 0x8020698d,  // W   'i'/0x8d  ifreq(32)       set driver-generic data
	// PS3-specific test parameter ioctls (used by sys_net_get/set_test_param in libnet)
	SIOCGIFTESTPARAM = 0xc210698e,  // R/W 'i'/0x8e  528-byte buf  get test parameters
	SIOCSIFTESTPARAM = 0x8210698f,  // W   'i'/0x8f  528-byte buf  set test parameters
	// Socket-level multicast flush (_IO 'P'/0xc8, no data parameter)
	SIOCFLUSHMC    = 0x200050c8,  // -   'P'/0xc8  (none)          flush all multicast group subscriptions
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
	{ "eth0", IFF_UP | IFF_DRV_RUNNING },
	{ "lo0",  IFF_UP | IFF_DRV_RUNNING },
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
error_code sys_net_bnet_ioctl(ppu_thread& ppu, s32 socket_id, u32 cmd, vm::ptr<struct ifreq> ifr)
{
	ppu.state += cpu_flag::wait;

	sys_net.todo("sys_net_bnet_ioctl(socket=%d, cmd=0x%x, ifr=0x%x)", socket_id, cmd, ifr);

	static std::unordered_map<std::string, u16> s_ifflags = {
		{"eth0", IFF_UP_ | IFF_DRV_RUNNING},
		{"lo0",  IFF_UP_ | IFF_DRV_RUNNING},
	};

	static std::unordered_map<std::string, s32> s_ifmtu = {
		{"eth0", 1500},
		{"lo0",  16384},
	};

	switch (cmd)
	{
	case SIOCGIFFLAGS:
	{
		const std::string name(ifr->ifr_name, strnlen(ifr->ifr_name, 16));
		const u16 flags = s_ifflags.count(name) ? s_ifflags[name] : u16{IFF_UP_};
		ifr->ifr_flags = flags;

		sys_net.warning("SIOCGIFFLAGS(if=%s): 0x%04x", name.c_str(), flags);

		return CELL_OK;
	}

	case SIOCSIFFLAGS:
	{
		const std::string name(ifr->ifr_name, strnlen(ifr->ifr_name, 16));
		u16& flags = s_ifflags[name];
		const u16 new_flags = ifr->ifr_flags;
		const u16 old_flags = flags;

		if ((old_flags & IFF_UP_) && !(new_flags & IFF_UP_))
			flags &= ~IFF_UP_;
		else if (!(old_flags & IFF_UP_) && (new_flags & IFF_UP_))
			flags |= IFF_UP_;

		flags = (flags & IFF_KERNEL_MASK) | (new_flags & IFF_USER_MASK);

		if (old_flags != flags)
			sys_net.warning("SIOCSIFFLAGS(if=%s): 0x%04x -> 0x%04x (req=0x%04x)",
				name.c_str(), old_flags, flags, new_flags);

		return CELL_OK;
	}

	// Address get — fill ifr_data (sockaddr_in: sa_len, AF_INET, port=0, addr, zero[8])
	case SIOCGIFADDR:
	case SIOCGIFDSTADDR:
	case SIOCGIFBRDADDR:
	case SIOCGIFNETMASK:
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

		u32 addr_h = 0;
		if (cmd == SIOCGIFADDR || cmd == SIOCGIFDSTADDR)
			addr_h = ip_h;
		else if (cmd == SIOCGIFBRDADDR)
			addr_h = bcast_h;
		else // SIOCGIFNETMASK
			addr_h = mask_h;

		// Write as be_t<u32> so PS3 big-endian memory sees correct network-order bytes.
		auto* sa = reinterpret_cast<u8*>(ifr->ifr_data);
		sa[0] = 16; sa[1] = 2; sa[2] = 0; sa[3] = 0;
		*reinterpret_cast<be_t<u32>*>(sa + 4) = addr_h;
		std::memset(sa + 8, 0, 8);

		sys_net.warning("SIOCGIF%s(if=%s): %u.%u.%u.%u",
			cmd == SIOCGIFADDR ? "ADDR" : cmd == SIOCGIFDSTADDR ? "DSTADDR" :
			cmd == SIOCGIFBRDADDR ? "BRDADDR" : "NETMASK",
			name.c_str(),
			(addr_h >> 24) & 0xff, (addr_h >> 16) & 0xff, (addr_h >> 8) & 0xff, addr_h & 0xff);

		return CELL_OK;
	}

	// Address set — no-op in emulation (real kernel forwards to in_control)
	case SIOCSIFADDR:
	case SIOCSIFDSTADDR:
	case SIOCSIFBRDADDR:
	case SIOCSIFNETMASK:
	{
		const std::string name(ifr->ifr_name, strnlen(ifr->ifr_name, 16));
		const auto* sa  = reinterpret_cast<const u8*>(ifr->ifr_data);
		const u32   addr = *reinterpret_cast<const be_t<u32>*>(sa + 4);
		sys_net.warning("SIOCS%s(if=%s): %u.%u.%u.%u",
			cmd == SIOCSIFADDR ? "IFADDR" : cmd == SIOCSIFDSTADDR ? "IFDSTADDR" :
			cmd == SIOCSIFBRDADDR ? "IFBRDADDR" : "IFNETMASK",
			name.c_str(),
			(addr >> 24) & 0xff, (addr >> 16) & 0xff, (addr >> 8) & 0xff, addr & 0xff);
		return CELL_OK;
	}

	case SIOCGIFMTU:
	{
		const std::string name(ifr->ifr_name, strnlen(ifr->ifr_name, 16));
		const s32 mtu = s_ifmtu.count(name) ? s_ifmtu[name] : 1500;
		ifr->ifr_mtu = mtu;

		sys_net.warning("SIOCGIFMTU(if=%s): %d", name.c_str(), mtu);

		return CELL_OK;
	}

	case SIOCSIFMTU:
	{
		const std::string name(ifr->ifr_name, strnlen(ifr->ifr_name, 16));
		const s32 new_mtu = ifr->ifr_mtu;

		if (new_mtu < 68 || new_mtu > 9000)
		{
			sys_net.warning("SIOCSIFMTU(if=%s): invalid MTU %d", name.c_str(), new_mtu);
			return -SYS_NET_EINVAL;
		}

		const s32 old_mtu = s_ifmtu.count(name) ? s_ifmtu[name] : 1500;
		s_ifmtu[name] = new_mtu;

		sys_net.warning("SIOCSIFMTU(if=%s): %d -> %d", name.c_str(), old_mtu, new_mtu);

		return CELL_OK;
	}

	case SIOCDIFADDR:
	{
		// Delete interface address — called by DHCP client to release an IP.
		// In the kernel this falls to the protocol-layer (in_control); in emulation just log it.
		const std::string name(ifr->ifr_name, strnlen(ifr->ifr_name, 16));
		sys_net.warning("SIOCDIFADDR(if=%s): address delete request", name.c_str());
		return CELL_OK;
	}

	case SIOCAIFADDR:
	{
		// Add/change interface alias address — called by DHCP client to assign an IP.
		// The argument is a 64-byte ifaliasreq: [name 16][addr 16][broadaddr 16][mask 16].
		// In the kernel this falls to in_control; in emulation store and log the assignment.
		const auto& alias = *vm::ptr<ifaliasreq>::make(ifr.addr());
		const std::string name(alias.ifra_name, strnlen(alias.ifra_name, 16));
		const u32 ip   = alias.ifra_addr.sin_addr;
		const u32 bcast = alias.ifra_broadaddr.sin_addr;
		const u32 mask  = alias.ifra_mask.sin_addr;
		sys_net.warning("SIOCAIFADDR(if=%s): ip=%u.%u.%u.%u mask=%u.%u.%u.%u bcast=%u.%u.%u.%u",
			name.c_str(),
			(ip >> 24) & 0xff, (ip >> 16) & 0xff, (ip >> 8) & 0xff, ip & 0xff,
			(mask >> 24) & 0xff, (mask >> 16) & 0xff, (mask >> 8) & 0xff, mask & 0xff,
			(bcast >> 24) & 0xff, (bcast >> 16) & 0xff, (bcast >> 8) & 0xff, bcast & 0xff);
		return CELL_OK;
	}

	case SIOCGIFGENERIC:
	{
		// Get driver-generic data — forwarded to if_ioctl unchanged in the kernel.
		// No known useful data to return; zero ifr_value and succeed.
		const std::string name(ifr->ifr_name, strnlen(ifr->ifr_name, 16));
		ifr->ifr_value = 0;
		sys_net.warning("SIOCGIFGENERIC(if=%s): → 0", name.c_str());
		return CELL_OK;
	}

	case SIOCSIFGENERIC:
	{
		// Set driver-generic data — forwarded to if_ioctl in kernel; no-op in emulation.
		const std::string name(ifr->ifr_name, strnlen(ifr->ifr_name, 16));
		sys_net.warning("SIOCSIFGENERIC(if=%s): value=0x%x", name.c_str(), +ifr->ifr_value);
		return CELL_OK;
	}

	case SIOCGIFTESTPARAM:
	{
		// sys_net_get_test_param in libnet issues this ioctl into a 528-byte buffer,
		// then copies bytes [16..527] to the caller. Zero-fill is correct for emulation.
		sys_net.warning("SIOCGIFTESTPARAM");
		std::memset(reinterpret_cast<u8*>(ifr.get_ptr()) + 0x10, 0, 0x200);
		return CELL_OK;
	}

	case SIOCSIFTESTPARAM:
	{
		// sys_net_set_test_param in libnet issues this ioctl. No-op in emulation.
		sys_net.warning("SIOCSIFTESTPARAM");
		return CELL_OK;
	}

	case SIOCFLUSHMC:
	{
		// Flush all multicast group subscriptions on this socket (_IO 'P'/0xc8, no data).
		// In the kernel: FUN_8000000000161c58 walks and frees the socket's mc group list.
		// In emulation: no multicast state to flush.
		sys_net.warning("SIOCFLUSHMC: flush multicast groups");
		return CELL_OK;
	}

	default:
		sys_net.warning("sys_net_bnet_ioctl: unhandled cmd=0x%x", cmd);
		return CELL_OK;
	}
}

error_code sys_net_bnet_sysctl(ppu_thread& ppu, vm::cptr<be_t<s32>> ops, u32 ops_count, u32 oldp, vm::ptr<be_t<u64>> oldlenp, u32 newp, u32 newlen)
{
	ppu.state += cpu_flag::wait;

	sys_net.todo("sys_net_bnet_sysctl(ops=0x%x, nops=%d, buf=0x%x, buflen=*0x%x, newbuf=0x%x, newlen=%d)", ops, ops_count, oldp, oldlenp, newp, newlen);

	// Only handle: {CTL_NET=4, AF_ROUTE=17, 0, family, NET_RT_IFLIST=4, flags}
	// Real kernel: sys_net_sysctl_net_route → NET_RT_IFLIST (type=4) →
	//   iterates the ifnet linked list, emits one RTM_IFINFO (0xf) per interface.
	// With an empty stub netctl sees no interfaces and stays "cable not connected".
	// Above may not be correct for cable connection.. actualy handled in control
	if (!ops || ops_count < 6 || !oldlenp)
	{
		sys_net.todo("sys_net_bnet_sysctl: bad args (ops=%s, nops=%d, oldlenp=%s)", ops, ops_count, oldlenp);
		return CELL_OK;
	}

	if (ops[0] != 4 || ops[1] != 17 || (ops[4] != 4 && ops[4] != 1))
	{
		std::string ops_str;
		for (u32 i = 0; i < ops_count; i++)
			fmt::append(ops_str, "%s%d", i ? "," : "", +ops[i]);
		sys_net.todo("sys_net_bnet_sysctl: unhandled ops=[%s]", ops_str);
		return CELL_OK;
	}

	// NET_RT_DUMP (ops[4] == 1): routing table dump.
	// TODO: synthesise real route entries once we understand the PS3's calling convention
	// (probe vs. combined probe+data, and what *oldlenp means when 0 on a data call).
	// For now, log the call parameters so we can observe what the PS3 actually passes,
	// then return empty (safe: same as the old unhandled-ops path).
	if (ops[4] == 1)
	{
		sys_net.todo("sys_net_bnet_sysctl NET_RT_DUMP: oldp=0x%x *oldlenp=%llu", oldp, +*oldlenp);

		// Synthesize a minimal routing table so netctl thinks the network is already
		// configured and skips the DHCP timeout path.
		// Hardcoded stub: 192.168.1.2/24 via 192.168.1.1
		//
		// rt_msghdr layout (confirmed from lv2_446 FUN_0x141f4c / rt_msg2):
		//   +0x00 be_u16 rtm_msglen
		//   +0x02 u8     rtm_version = 3
		//   +0x03 u8     rtm_type    = 4 (RTM_GET)
		//   +0x04 be_u16 rtm_index
		//   +0x06 u16    pad
		//   +0x08 be_s32 rtm_flags
		//   +0x0c be_s32 rtm_addrs  (bitmask of following sockaddrs)
		//   +0x10..+0x78 pid/seq/errno/use/inits/metrics — zeroed
		//   +0x78 sockaddrs (sockaddr_in, 16 bytes each, big-endian)

		constexpr u32 hdr  = 0x78; // rt_msghdr base
		constexpr u32 sa   = 16;   // sizeof(sockaddr_in)

		// Entry 1: default route — 0.0.0.0/0 via 192.168.1.1
		//   RTA_DST(0x1)|RTA_GATEWAY(0x2)|RTA_NETMASK(0x4) = 0x7 → 3 sockaddrs
		const u32 e1 = hdr + 3 * sa; // 0xa8

		// Entry 2: subnet route — 192.168.1.0/24 (connected, no gateway)
		//   RTA_DST(0x1)|RTA_NETMASK(0x4) = 0x5 → 2 sockaddrs
		const u32 e2 = hdr + 2 * sa; // 0x98

		const u32 total = e1 + e2;

		if (!oldp)
		{
			*oldlenp = static_cast<u64>(total * 11u / 10u);
			return CELL_OK;
		}

		// sockaddr_in builder: writes 16 bytes at p (sa_len, AF_INET, port=0, addr, padding)
		auto make_sin = [](u8* p, u32 addr)
		{
			p[0] = 16; p[1] = 2; p[2] = p[3] = 0;
			*reinterpret_cast<be_t<u32>*>(p + 4) = addr;
			std::memset(p + 8, 0, 8);
		};

		std::vector<u8> buf(total, 0);
		u8* w = buf.data();

		// Entry 1: default route
		*reinterpret_cast<be_t<u16>*>(w + 0x00) = static_cast<u16>(e1);
		w[0x02] = 3; w[0x03] = 4; // version=3, type=RTM_GET
		*reinterpret_cast<be_t<u16>*>(w + 0x04) = 1;     // rtm_index = eth0
		*reinterpret_cast<be_t<s32>*>(w + 0x08) = 0x803; // RTF_UP|RTF_GATEWAY|RTF_STATIC
		*reinterpret_cast<be_t<s32>*>(w + 0x0c) = 0x7;   // RTA_DST|RTA_GATEWAY|RTA_NETMASK
		make_sin(w + hdr + 0 * sa, 0x00000000u); // dst  0.0.0.0
		make_sin(w + hdr + 1 * sa, 0xc0a80101u); // gw   192.168.1.1
		make_sin(w + hdr + 2 * sa, 0x00000000u); // mask 0.0.0.0
		w += e1;

		// Entry 2: subnet route
		*reinterpret_cast<be_t<u16>*>(w + 0x00) = static_cast<u16>(e2);
		w[0x02] = 3; w[0x03] = 4;
		*reinterpret_cast<be_t<u16>*>(w + 0x04) = 1;
		*reinterpret_cast<be_t<s32>*>(w + 0x08) = 0x41;  // RTF_UP|RTF_DONE
		*reinterpret_cast<be_t<s32>*>(w + 0x0c) = 0x5;   // RTA_DST|RTA_NETMASK
		make_sin(w + hdr + 0 * sa, 0xc0a80100u); // dst  192.168.1.0
		make_sin(w + hdr + 1 * sa, 0xffffff00u); // mask 255.255.255.0

		std::memcpy(vm::_ptr<u8>(oldp), buf.data(), total);
		*oldlenp = total;
		sys_net.todo("sys_net_bnet_sysctl NET_RT_DUMP: wrote %u bytes (2 entries)", total);
		return CELL_OK;
	}

	// NET_RT_IFLIST (ops[4] == 4): interface list.
	// RTM_IFINFO for eth0: 0x98-byte header + 12-byte sockaddr_dl (RTA_IFP).
	// Layout from sys_net_rt_ifmsg (lv2_446 §sys_net_sysctl_net_route):
	//   +0x00: be_u16 ifm_msglen  = total size (0xa4)
	//   +0x02: u8     ifm_version = 3 (written by rt_msg2: stb r0,0x2(r23) where r0=3)
	//   +0x03: u8     ifm_type    = 0xf (RTM_IFINFO)
	//   +0x04: be_s32 ifm_addrs   = 0x10 (RTA_IFP) — required so libnet.prx FUN_00004634
	//                                registers the interface in its name→index table
	//   +0x08: be_s32 ifm_flags   = IFF_UP | IFF_DRV_RUNNING (0x41)
	//   +0x0c: be_u16 ifm_index   = 1 (eth0)
	//   +0x0e: u16    _pad        = 0
	//   +0x10: if_data (0x88 bytes):
	//     [+0x00] ifi_type       = 6 (IFT_ETHER)
	//     [+0x02] ifi_addrlen    = 6 (MAC)
	//     [+0x03] ifi_hdrlen     = 14
	//     [+0x04] ifi_link_state = 2 (LINK_STATE_UP)
	//     [+0x08] ifi_mtu        = 1500
	//   +0x98: sockaddr_dl for eth0 (12 bytes, RTA_IFP payload):
	//     [+0x00] sdl_len    = 12
	//     [+0x01] sdl_family = 0x12 (AF_LINK)
	//     [+0x02] sdl_index  = 1 (be_u16, eth0)
	//     [+0x04] sdl_type   = 6 (IFT_ETHER)
	//     [+0x05] sdl_nlen   = 4 (strlen("eth0"))
	//     [+0x06] sdl_alen   = 0
	//     [+0x07] sdl_slen   = 0
	//     [+0x08] sdl_data   = "eth0"
	constexpr u32 sdl_len = 12;
	constexpr u32 msg_len = 0x98 + sdl_len; // 0xa4
	u8 msg[msg_len]{};
	*reinterpret_cast<be_t<u16>*>(&msg[0x00]) = msg_len;
	msg[0x02] = 3;
	msg[0x03] = 0xf;
	*reinterpret_cast<be_t<s32>*>(&msg[0x04]) = 0x10; // ifm_addrs = RTA_IFP
	*reinterpret_cast<be_t<s32>*>(&msg[0x08]) = 0x41;
	*reinterpret_cast<be_t<u16>*>(&msg[0x0c]) = 1;
	msg[0x10] = 6;
	msg[0x12] = 6;
	msg[0x13] = 14;
	msg[0x14] = 2;
	*reinterpret_cast<be_t<u32>*>(&msg[0x18]) = 1500;
	// sockaddr_dl at +0x98
	msg[0x98] = sdl_len;
	msg[0x99] = 0x12; // AF_LINK
	*reinterpret_cast<be_t<u16>*>(&msg[0x9a]) = 1; // sdl_index = 1
	msg[0x9c] = 6;    // sdl_type = IFT_ETHER
	msg[0x9d] = 4;    // sdl_nlen = strlen("eth0")
	// sdl_alen, sdl_slen = 0 (already zeroed)
	msg[0xa0] = 'e'; msg[0xa1] = 't'; msg[0xa2] = 'h'; msg[0xa3] = '0';

	if (!oldp)
	{
		// Size probe (oldp=NULL): kernel returns 110% of actual size.
		// PS3 always passes *oldlenp=0 on probe (libnet.prx u64/u32 layout) — ignored as input.
		// We write the full u64 as the real kernel does (writes as 'long', 8 bytes big-endian).
		const u64 est = static_cast<u64>(msg_len * 11u / 10u);
		*oldlenp = est;
		sys_net.todo("sys_net_bnet_sysctl NET_RT_IFLIST: size probe → %llu", est);
		return CELL_OK;
	}

	// Data call: write RTM_IFINFO unconditionally.
	// PS3 always passes *oldlenp=0 on data calls too (same u64/u32 layout mismatch),
	// so we ignore *oldlenp as input and write unconditionally, matching real kernel behaviour
	// which copies data whenever local_ec=0 (oldp is present).
	std::memcpy(vm::_ptr<u8>(oldp), msg, msg_len);
	*oldlenp = msg_len;
	sys_net.todo("sys_net_bnet_sysctl NET_RT_IFLIST: wrote %u bytes RTM_IFINFO+sockaddr_dl", msg_len);
	return CELL_OK;
}

// Returns weird stuff instead of error_code?
error_code sys_net_eurus_post_command(ppu_thread& ppu, u16 cmd, vm::ptr<u8> cmdbuf, u32 cmdbuf_size)
{
	ppu.state += cpu_flag::wait;
	// WIFI specifically

	sys_net.todo("sys_net_eurus_post_command(cmd=0x%x, cmdbuf=*0x%x, cmdbuf_size=0x%x)", cmd, cmdbuf, cmdbuf_size);

	if (!cmdbuf)
	{
		return CELL_OK;
	}

	if (cmd == 0xffff)
	{
		// Get LV2 device state??
		// Some functions check if [0xc] is 1
		u8 moo[] = {
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x01 };
		memcpy(cmdbuf.get_ptr(), moo, 16);
		return CELL_OK;
	}
	else if (cmd == 0x1031)
	{
		// Unknown
		return -1;
	}



	sys_net.todo("not implemented");
	return CELL_OK;
}
