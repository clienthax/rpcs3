#pragma once

#include "Emu/Cell/Modules/sys_net_.h"

// Custom structure for sockets
// We map host sockets to sequential IDs to return as descriptors because syscalls expect socket IDs to be under 1024.
struct lv2_socket final
{
	static const u32 id_base = 0;
	static const u32 id_step = 1;
	static const u32 id_count = 1024;
};

// Syscalls

s32 sys_net_bnet_accept(s32 s, vm::ps3::ptr<sys_net_sockaddr> addr, vm::ps3::ptr<u32> paddrlen);
s32 sys_net_bnet_bind(s32 s, vm::ps3::cptr<sys_net_sockaddr> addr, u32 addrlen);
s32 sys_net_bnet_connect(s32 s, vm::ps3::ptr<sys_net_sockaddr> addr, u32 addrlen);
s32 sys_net_bnet_getpeername(s32 s, vm::ps3::ptr<sys_net_sockaddr> addr, vm::ps3::ptr<u32> paddrlen);
s32 sys_net_bnet_getsockname(s32 s, vm::ps3::ptr<sys_net_sockaddr> addr, vm::ps3::ptr<u32> paddrlen);
s32 sys_net_bnet_getsockopt(s32 s, s32 level, s32 optname, vm::ps3::ptr<void> optval, vm::ps3::ptr<u32> optlen);
s32 sys_net_bnet_listen(s32 s, s32 backlog);
s32 sys_net_bnet_recvfrom(s32 s, vm::ps3::ptr<void> buf, u32 len, s32 flags, vm::ps3::ptr<sys_net_sockaddr> addr, vm::ps3::ptr<u32> paddrlen);
s32 sys_net_bnet_recvmsg(s32 s, vm::ps3::ptr<sys_net_msghdr> msg, s32 flags);
s32 sys_net_bnet_sendmsg(s32 s, vm::ps3::cptr<sys_net_msghdr> msg, s32 flags);
s32 sys_net_bnet_sendto(s32 s, vm::ps3::cptr<void> buf, u32 len, s32 flags, vm::ps3::cptr<sys_net_sockaddr> addr, u32 addrlen);
s32 sys_net_bnet_setsockopt(s32 s, s32 level, s32 optname, vm::ps3::cptr<void> optval, u32 optlen);
s32 sys_net_bnet_shutdown(s32 s, s32 how);
s32 sys_net_bnet_socket(s32 family, s32 type, s32 protocol);
s32 sys_net_bnet_close(s32 s);
s32 sys_net_bnet_poll(vm::ps3::ptr<sys_net_pollfd> fds, s32 nfds, s32 ms);
s32 sys_net_bnet_select(s32 nfds, vm::ps3::ptr<sys_net_fd_set> readfds, vm::ps3::ptr<sys_net_fd_set> writefds, vm::ps3::ptr<sys_net_fd_set> exceptfds, vm::ps3::ptr<sys_net_timeval> timeout);
s32 _sys_net_open_dump(s32 len, s32 flags);
s32 _sys_net_read_dump(s32 id, vm::ps3::ptr<void> buf, s32 len, vm::ps3::ptr<s32> pflags);
s32 _sys_net_close_dump(s32 id, vm::ps3::ptr<s32> pflags);
s32 _sys_net_write_dump(s32 id, vm::ps3::cptr<void> buf, s32 len, u32 unknown);
s32 sys_net_abort(s32 type, u64 arg, s32 flags);
s32 sys_net_infoctl(s32 cmd, vm::ps3::ptr<void> arg);
s32 sys_net_control(u32 arg1, s32 arg2, vm::ps3::ptr<void> arg3, s32 arg4);
s32 sys_net_bnet_ioctl(s32 arg1, u32 arg2, u32 arg3);
s32 sys_net_bnet_sysctl(u32 arg1, u32 arg2, u32 arg3, vm::ps3::ptr<void> arg4, u32 arg5, u32 arg6);
s32 sys_net_eurus_post_command(s32 arg1, u32 arg2, u32 arg3);
