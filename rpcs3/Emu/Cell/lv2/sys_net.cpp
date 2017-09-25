#include "stdafx.h"
#include "Emu/Memory/Memory.h"
#include "Emu/System.h"
#include "Emu/IdManager.h"

#include "sys_net.h"

#ifdef _WIN32
#include <winsock2.h>
#include <WS2tcpip.h>
#else
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

#include <fcntl.h>

logs::channel sys_net("sys_net");

// Error helper functions
static s32 get_last_error()
{
	// Convert the error code for socket functions to a one for sys_net
	s32 result;
	const char* name{};

#ifdef _WIN32
	switch (s32 code = WSAGetLastError())
#define ERROR_CASE(error) case WSA ## error: result = SYS_NET_ ## error; name = #error; break;
#else
	switch (s32 code = errno)
#define ERROR_CASE(error) case error: result = SYS_NET_ ## error; name = #error; break;
#endif
	{
		ERROR_CASE(EWOULDBLOCK);
	default: sys_net.error("Unknown/illegal socket error: %d" HERE, code);
	}

	if (name && result != SYS_NET_EWOULDBLOCK)
	{
		sys_net.error("Socket error %s", name);
	}

	return result;
#undef ERROR_CASE
}

s32 sys_net_bnet_accept(s32 s, vm::ps3::ptr<sys_net_sockaddr> addr, vm::ps3::ptr<u32> paddrlen)
{
	sys_net.todo(__func__);
	return 0;
}

s32 sys_net_bnet_bind(s32 s, vm::ps3::cptr<sys_net_sockaddr> addr, u32 addrlen)
{
	sys_net.todo(__func__);
	return 0;
}

s32 sys_net_bnet_connect(s32 s, vm::ps3::ptr<sys_net_sockaddr> addr, u32 addrlen)
{
	sys_net.todo(__func__);
	return 0;
}

s32 sys_net_bnet_getpeername(s32 s, vm::ps3::ptr<sys_net_sockaddr> addr, vm::ps3::ptr<u32> paddrlen)
{
	sys_net.todo(__func__);
	return 0;
}

s32 sys_net_bnet_getsockname(s32 s, vm::ps3::ptr<sys_net_sockaddr> addr, vm::ps3::ptr<u32> paddrlen)
{
	sys_net.todo(__func__);
	return 0;
}

s32 sys_net_bnet_getsockopt(s32 s, s32 level, s32 optname, vm::ps3::ptr<void> optval, vm::ps3::ptr<u32> optlen)
{
	sys_net.todo(__func__);
	return 0;
}

s32 sys_net_bnet_listen(s32 s, s32 backlog)
{
	sys_net.todo(__func__);
	return 0;
}

s32 sys_net_bnet_recvfrom(s32 s, vm::ps3::ptr<void> buf, u32 len, s32 flags, vm::ps3::ptr<sys_net_sockaddr> addr, vm::ps3::ptr<u32> paddrlen)
{
	sys_net.todo(__func__);
	return 0;
}

s32 sys_net_bnet_recvmsg(s32 s, vm::ps3::ptr<sys_net_msghdr> msg, s32 flags)
{
	sys_net.todo(__func__);
	return 0;
}

s32 sys_net_bnet_sendmsg(s32 s, vm::ps3::cptr<sys_net_msghdr> msg, s32 flags)
{
	sys_net.todo(__func__);
	return 0;
}

s32 sys_net_bnet_sendto(s32 s, vm::ps3::cptr<void> buf, u32 len, s32 flags, vm::ps3::cptr<sys_net_sockaddr> addr, u32 addrlen)
{
	sys_net.todo(__func__);
	return 0;
}

s32 sys_net_bnet_setsockopt(s32 s, s32 level, s32 optname, vm::ps3::cptr<void> optval, u32 optlen)
{
	sys_net.todo(__func__);
	return 0;
}

s32 sys_net_bnet_shutdown(s32 s, s32 how)
{
	sys_net.todo(__func__);
	return 0;
}

s32 sys_net_bnet_socket(s32 family, s32 type, s32 protocol)
{
	sys_net.todo(__func__);
	return 0;
}

s32 sys_net_bnet_close(s32 s)
{
	sys_net.todo(__func__);
	return 0;
}

s32 sys_net_bnet_poll(vm::ps3::ptr<sys_net_pollfd> fds, s32 nfds, s32 ms)
{
	sys_net.todo(__func__);
	return 0;
}

s32 sys_net_bnet_select(s32 nfds, vm::ps3::ptr<sys_net_fd_set> readfds, vm::ps3::ptr<sys_net_fd_set> writefds, vm::ps3::ptr<sys_net_fd_set> exceptfds, vm::ps3::ptr<sys_net_timeval> timeout)
{
	sys_net.todo(__func__);
	return 0;
}

s32 _sys_net_open_dump(s32 len, s32 flags)
{
	sys_net.todo(__func__);
	return 0;
}

s32 _sys_net_read_dump(s32 id, vm::ps3::ptr<void> buf, s32 len, vm::ps3::ptr<s32> pflags)
{
	sys_net.todo(__func__);
	return 0;
}

s32 _sys_net_close_dump(s32 id, vm::ps3::ptr<s32> pflags)
{
	sys_net.todo(__func__);
	return 0;
}

s32 _sys_net_write_dump(s32 id, vm::ps3::cptr<void> buf, s32 len, u32 unknown)
{
	sys_net.todo(__func__);
	return 0;
}

s32 sys_net_abort(s32 type, u64 arg, s32 flags)
{
	sys_net.todo(__func__);
	return 0;
}

s32 sys_net_infoctl(s32 cmd, vm::ps3::ptr<void> arg)
{
	sys_net.todo(__func__);
	return 0;
}

s32 sys_net_control(u32 arg1, s32 arg2, vm::ps3::ptr<void> arg3, s32 arg4)
{
	sys_net.todo(__func__);
	return 0;
}

s32 sys_net_bnet_ioctl(s32 arg1, u32 arg2, u32 arg3)
{
	sys_net.todo(__func__);
	return 0;
}

s32 sys_net_bnet_sysctl(u32 arg1, u32 arg2, u32 arg3, vm::ps3::ptr<void> arg4, u32 arg5, u32 arg6)
{
	sys_net.todo(__func__);
	return 0;
}

s32 sys_net_eurus_post_command(s32 arg1, u32 arg2, u32 arg3)
{
	sys_net.todo(__func__);
	return 0;
}
