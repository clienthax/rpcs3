#pragma once

#include "Emu/Memory/vm.h"

// Error codes
enum 
{
	SYS_NET_ENOENT          = 2,
	SYS_NET_EINTR           = 4,
	SYS_NET_EBADF           = 9,
	SYS_NET_ENOMEM          = 12,
	SYS_NET_EACCES          = 13,
	SYS_NET_EFAULT          = 14,
	SYS_NET_EBUSY           = 16,
	SYS_NET_EINVAL          = 22,
	SYS_NET_EMFILE          = 24,
	SYS_NET_ENOSPC          = 28,
	SYS_NET_EPIPE           = 32,
	SYS_NET_EAGAIN          = 35,
	SYS_NET_EWOULDBLOCK     = SYS_NET_EAGAIN,
	SYS_NET_EINPROGRESS     = 36,
	SYS_NET_EALREADY        = 37,
	SYS_NET_EDESTADDRREQ    = 39,
	SYS_NET_EMSGSIZE        = 40,
	SYS_NET_EPROTOTYPE      = 41,
	SYS_NET_ENOPROTOOPT     = 42,
	SYS_NET_EPROTONOSUPPORT = 43,
	SYS_NET_EOPNOTSUPP      = 45,
	SYS_NET_EPFNOSUPPORT    = 46,
	SYS_NET_EAFNOSUPPORT    = 47,
	SYS_NET_EADDRINUSE      = 48,
	SYS_NET_EADDRNOTAVAIL   = 49,
	SYS_NET_ENETDOWN        = 50,
	SYS_NET_ENETUNREACH     = 51,
	SYS_NET_ECONNABORTED    = 53,
	SYS_NET_ECONNRESET      = 54,
	SYS_NET_ENOBUFS         = 55,
	SYS_NET_EISCONN         = 56,
	SYS_NET_ENOTCONN        = 57,
	SYS_NET_ESHUTDOWN       = 58,
	SYS_NET_ETOOMANYREFS    = 59,
	SYS_NET_ETIMEDOUT       = 60,
	SYS_NET_ECONNREFUSED    = 61,
	SYS_NET_EHOSTDOWN       = 64,
	SYS_NET_EHOSTUNREACH    = 65,
};

// Socket types (prefixed with SYS_NET_)
enum
{
	SYS_NET_SOCK_STREAM     = 1,
	SYS_NET_SOCK_DGRAM      = 2,
	SYS_NET_SOCK_RAW        = 3,
	SYS_NET_SOCK_DGRAM_P2P  = 6,
	SYS_NET_SOCK_STREAM_P2P = 10,
};

// Socket options (prefixed with SYS_NET_)
enum
{
	SYS_NET_SO_SNDBUF       = 0x1001,
	SYS_NET_SO_RCVBUF       = 0x1002,
	SYS_NET_SO_SNDLOWAT     = 0x1003,
	SYS_NET_SO_RCVLOWAT     = 0x1004,
	SYS_NET_SO_SNDTIMEO     = 0x1005,
	SYS_NET_SO_RCVTIMEO     = 0x1006,
	SYS_NET_SO_ERROR        = 0x1007,
	SYS_NET_SO_TYPE         = 0x1008,
	SYS_NET_SO_NBIO         = 0x1100, // Non-blocking IO
	SYS_NET_SO_TPPOLICY     = 0x1101,

	SYS_NET_SO_REUSEADDR    = 0x0004,
	SYS_NET_SO_KEEPALIVE    = 0x0008,
	SYS_NET_SO_BROADCAST    = 0x0020,
	SYS_NET_SO_LINGER       = 0x0080,
	SYS_NET_SO_OOBINLINE    = 0x0100,
	SYS_NET_SO_REUSEPORT    = 0x0200,
	SYS_NET_SO_ONESBCAST    = 0x0800,
	SYS_NET_SO_USECRYPTO    = 0x1000,
	SYS_NET_SO_USESIGNATURE = 0x2000,
};

// TCP options (prefixed with SYS_NET_)
enum
{
	SYS_NET_TCP_NODELAY          = 1,
	SYS_NET_TCP_MAXSEG           = 2,
	SYS_NET_TCP_MSS_TO_ADVERTISE = 3,
};

// IP protocols (prefixed with SYS_NET_)
enum
{
	SYS_NET_IPPROTO_IP     = 0,
	SYS_NET_IPPROTO_ICMP   = 1,
	SYS_NET_IPPROTO_IGMP   = 2,
	SYS_NET_IPPROTO_TCP    = 6,
	SYS_NET_IPPROTO_UDP    = 17,
	SYS_NET_IPPROTO_ICMPV6 = 58,
};

// Poll events (prefixed with SYS_NET_)
enum
{
	SYS_NET_POLLIN         = 0x0001,
	SYS_NET_POLLPRI        = 0x0002,
	SYS_NET_POLLOUT        = 0x0004,
	SYS_NET_POLLERR        = 0x0008, /* revent only */
	SYS_NET_POLLHUP        = 0x0010, /* revent only */
	SYS_NET_POLLNVAL       = 0x0020, /* revent only */
	SYS_NET_POLLRDNORM     = 0x0040,
	SYS_NET_POLLWRNORM     = SYS_NET_POLLOUT,
	SYS_NET_POLLRDBAND     = 0x0080,
	SYS_NET_POLLWRBAND     = 0x0100,
};

// in_addr_t type prefixed with sys_net_
using sys_net_in_addr_t    = u32;

// in_port_t type prefixed with sys_net_
using sys_net_in_port_t    = u16;

// sa_family_t type prefixed with sys_net_
using sys_net_sa_family_t  = u8;

// socklen_t type prefixed with sys_net_
using sys_net_socklen_t    = u32;

// fd_set prefixed with sys_net_
struct sys_net_fd_set
{
	be_t<u32> fds_bits[32];
};

// hostent prefixed with sys_net_
struct sys_net_hostent
{
	vm::ps3::bptr<char> h_name;
	vm::ps3::bpptr<char> h_aliases;
	be_t<s32> h_addrtype;
	be_t<s32> h_length;
	vm::ps3::bpptr<char> h_addr_list;
};

// in_addr prefixed with sys_net_
struct sys_net_in_addr
{
	be_t<u32> _s_addr;
};

// iovec prefixed with sys_net_
struct sys_net_iovec
{
	be_t<s32> zero1;
	vm::ps3::bptr<void> iov_base;
	be_t<s32> zero2;
	be_t<u32> iov_len;
};

// ip_mreq prefixed with sys_net_
struct sys_net_ip_mreq
{
	be_t<u32> imr_multiaddr;
	be_t<u32> imr_interface;
};

// msghdr prefixed with sys_net_
struct sys_net_msghdr
{
	be_t<s32> zero1;
	vm::ps3::bptr<void> msg_name;
	be_t<u32> msg_namelen;
	be_t<s32> pad1;
	be_t<s32> zero2;
	vm::ps3::bptr<sys_net_iovec> msg_iov;
	be_t<s32> msg_iovlen;
	be_t<s32> pad2;
	be_t<s32> zero3;
	vm::ps3::bptr<void> msg_control;
	be_t<u32> msg_controllen;
	be_t<s32> msg_flags;
};

// pollfd prefixed with sys_net_
struct sys_net_pollfd
{
	be_t<s32> fd;
	be_t<s16> events;
	be_t<s16> revents;
};

// sockaddr prefixed with sys_net_
struct sys_net_sockaddr
{
	u8 sa_len;
	u8 sa_family;
	char sa_data[14];
};

// sockaddr_dl prefixed with sys_net_
struct sys_net_sockaddr_dl
{
	u8 sdl_len;
	u8 sdl_family;
	be_t<u16> sdl_index;
	u8 sdl_type;
	u8 sdl_nlen;
	u8 sdl_alen;
	u8 sdl_slen;
	char sdl_data[12];
};

// sockaddr_in prefixed with sys_net_
struct sys_net_sockaddr_in
{
	u8 sin_len;
	u8 sin_family;
	be_t<u16> sin_port;
	be_t<u32> sin_addr;
	char sin_zero[8];
};

// sockaddr_in_p2p prefixed with sys_net_
struct sys_net_sockaddr_in_p2p
{
	u8 sin_len;
	u8 sin_family;
	be_t<u16> sin_port;
	be_t<u32> sin_addr;
	be_t<u16> sin_vport;
	char sin_zero[6];
};

// timeval prefixed with sys_net_
struct sys_net_timeval
{
	be_t<s64> tv_sec;
	be_t<s64> tv_usec;
};

struct sys_net_sockinfo_t
{
	be_t<s32> s;
	be_t<s32> proto;
	be_t<s32> recv_queue_length;
	be_t<s32> send_queue_length;
	sys_net_in_addr local_adr;
	be_t<s32> local_port;
	sys_net_in_addr remote_adr;
	be_t<s32> remote_port;
	be_t<s32> state;
};

struct sys_net_initialize_parameter_t
{
	vm::ps3::bptr<void> memory;
	be_t<s32> memory_size;
	be_t<s32> flags;
};
