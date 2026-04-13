#include "stdafx.h"
#include "lv2_socket_raw.h"
#include "Emu/NP/vport0.h"

LOG_CHANNEL(sys_net);

template <typename T>
struct socket_raw_logging
{
	socket_raw_logging() = default;

	socket_raw_logging(const socket_raw_logging&) = delete;
	socket_raw_logging& operator=(const socket_raw_logging&) = delete;

	atomic_t<bool> logged = false;
};

#define LOG_ONCE(raw_var, message)                                              \
	if (!g_fxo->get<socket_raw_logging<class raw_var>>().logged.exchange(true)) \
	{                                                                           \
		sys_net.todo(message);                                                  \
	}

lv2_socket_raw::lv2_socket_raw(lv2_socket_family family, lv2_socket_type type, lv2_ip_protocol protocol)
	: lv2_socket(family, type, protocol)
{
}

lv2_socket_raw::lv2_socket_raw(utils::serial& ar, lv2_socket_type type)
	: lv2_socket(stx::make_exact(ar), type)
{
}

void lv2_socket_raw::save(utils::serial& ar)
{
	lv2_socket::save(ar, true);
}

std::tuple<bool, s32, shared_ptr<lv2_socket>, sys_net_sockaddr> lv2_socket_raw::accept([[maybe_unused]] bool is_lock)
{
	sys_net.fatal("[RAW] accept() called on a RAW socket");
	return {};
}

std::optional<s32> lv2_socket_raw::connect([[maybe_unused]] const sys_net_sockaddr& addr)
{
	sys_net.fatal("[RAW] connect() called on a RAW socket");
	return CELL_OK;
}

s32 lv2_socket_raw::connect_followup()
{
	sys_net.fatal("[RAW] connect_followup() called on a RAW socket");
	return CELL_OK;
}

std::pair<s32, sys_net_sockaddr> lv2_socket_raw::getpeername()
{
	LOG_ONCE(raw_getpeername, "[RAW] getpeername() called on a RAW socket");
	return {};
}

s32 lv2_socket_raw::listen([[maybe_unused]] s32 backlog)
{
	LOG_ONCE(raw_listen, "[RAW] listen() called on a RAW socket");
	return {};
}

s32 lv2_socket_raw::bind([[maybe_unused]] const sys_net_sockaddr& addr)
{
	LOG_ONCE(raw_bind, "lv2_socket_raw::bind");
	return {};
}

std::pair<s32, sys_net_sockaddr> lv2_socket_raw::getsockname()
{
	LOG_ONCE(raw_getsockname, "lv2_socket_raw::getsockname");
	return {};
}

std::tuple<s32, lv2_socket::sockopt_data, u32> lv2_socket_raw::getsockopt([[maybe_unused]] s32 level, [[maybe_unused]] s32 optname, [[maybe_unused]] u32 len)
{
	LOG_ONCE(raw_getsockopt, "lv2_socket_raw::getsockopt");
	return {};
}

s32 lv2_socket_raw::setsockopt(s32 level, s32 optname, const std::vector<u8>& optval)
{
	LOG_ONCE(raw_setsockopt, "lv2_socket_raw::setsockopt");

	// TODO
	int native_int = *reinterpret_cast<const be_t<s32>*>(optval.data());

	if (level == SYS_NET_SOL_SOCKET && optname == SYS_NET_SO_NBIO)
	{
		so_nbio = native_int;
	}

	return {};
}

  std::optional<std::tuple<s32, std::vector<u8>, sys_net_sockaddr>> lv2_socket_raw::recvfrom(s32 flags, u32 len,
  [[maybe_unused]] bool is_lock)
  {
      // AF_ROUTE socket: inject a fake RTM_IFINFO once to signal eth0 is up.
      // Layout from sys_net_rt_ifmsg (lv2_446):
      //   +0x00: be_u16 ifm_msglen  = 0x98
      //   +0x02: u8     ifm_version = 5
      //   +0x03: u8     ifm_type    = 0xf (RTM_IFINFO)
      //   +0x04: be_s32 ifm_addrs   = 0
      //   +0x08: be_s32 ifm_flags   = IFF_UP | IFF_DRV_RUNNING (0x41)
      //   +0x0c: be_u16 ifm_index   = 1 (eth0)
      //   +0x0e: u16    _pad        = 0
      //   +0x10: u8[0x88] ifm_data  = zeroes
      if (family == SYS_NET_AF_ROUTE)
      {
          if (route_msg_sent.exchange(true))
          {
              // already delivered — socket will be closed and reopened by netctl,
              // but we don't want to spam it on every reopen either
              if (so_nbio || (flags & SYS_NET_MSG_DONTWAIT))
                  return {{-SYS_NET_EWOULDBLOCK, {}, {}}};
              return {};
          }

          constexpr u32 msg_len = 0x98;
          std::vector<u8> msg(msg_len, 0);

          // ifm_msglen
          *reinterpret_cast<be_t<u16>*>(&msg[0x00]) = msg_len;
          // ifm_version = 3 (confirmed: rt_msg2 @ 0x141f30 does stb r0,0x2(r23) where r0=3)
          msg[0x02] = 3;
          // ifm_type = RTM_IFINFO
          msg[0x03] = 0xf;
          // ifm_addrs = 0 (already zeroed)
          // ifm_flags = IFF_UP | IFF_DRV_RUNNING
          *reinterpret_cast<be_t<s32>*>(&msg[0x08]) = 0x41;
          // ifm_index = 1 (eth0)
          *reinterpret_cast<be_t<u16>*>(&msg[0x0c]) = 1;
          // ifm_data[0x88] at +0x10 — zeroes fine for link-up notification

          sys_net.notice("lv2_socket_raw AF_ROUTE: injecting RTM_IFINFO (eth0 up)");
          return {{static_cast<s32>(msg_len), std::move(msg), {}}};
      }

      LOG_ONCE(raw_recvfrom, "lv2_socket_raw::recvfrom");

      if (so_nbio || (flags & SYS_NET_MSG_DONTWAIT))
          return {{-SYS_NET_EWOULDBLOCK, {}, {}}};

      return {};
  }

std::optional<s32> lv2_socket_raw::sendto([[maybe_unused]] s32 flags, [[maybe_unused]] const std::vector<u8>& buf, [[maybe_unused]] std::optional<sys_net_sockaddr> opt_sn_addr, [[maybe_unused]] bool is_lock)
{
	LOG_ONCE(raw_sendto, "lv2_socket_raw::sendto");
	return ::size32(buf);
}

std::optional<s32> lv2_socket_raw::sendmsg([[maybe_unused]] s32 flags, [[maybe_unused]] const sys_net_msghdr& msg, [[maybe_unused]] bool is_lock)
{
	LOG_ONCE(raw_sendmsg, "lv2_socket_raw::sendmsg");
	return {};
}

void lv2_socket_raw::close()
{
	LOG_ONCE(raw_close, "lv2_socket_raw::close");
}

s32 lv2_socket_raw::shutdown([[maybe_unused]] s32 how)
{
	LOG_ONCE(raw_shutdown, "lv2_socket_raw::shutdown");
	return {};
}

void lv2_socket_raw::poll(sys_net_pollfd& sn_pfd, [[maybe_unused]] pollfd& native_pfd)
{
	if (family == SYS_NET_AF_ROUTE)
	{
		if (!route_msg_sent && (sn_pfd.events & SYS_NET_POLLIN))
			sn_pfd.revents |= SYS_NET_POLLIN;
		return;
	}

	LOG_ONCE(raw_poll, "lv2_socket_raw::poll");
}

std::tuple<bool, bool, bool> lv2_socket_raw::select(bs_t<lv2_socket::poll_t> selected, [[maybe_unused]] pollfd&
native_pfd)
{
	if (family == SYS_NET_AF_ROUTE)
	{
		// Signal readable until we've delivered the RTM_IFINFO message.
		// This causes the caller's poll/select to return immediately,
		// triggering the recvfrom where we inject the message.
		const bool readable = !route_msg_sent;
		return {readable, false, false};
	}

	LOG_ONCE(raw_select, "lv2_socket_raw::select");
	return {};
}

