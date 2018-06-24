#include "stdafx.h"
#include "sys_tty.h"

logs::channel sys_tty("sys_tty");

extern fs::file g_tty;
extern atomic_t<s64> g_tty_size;

error_code sys_tty_read(s32 ch, vm::ptr<char> buf, u32 len, vm::ptr<u32> preadlen)
{
	sys_tty.fatal("sys_tty_read(ch=%d, buf=*0x%x, len=%d, preadlen=*0x%x)", ch, buf, len, preadlen);

	// We currently do not support reading from the Console
	fmt::throw_exception("Unimplemented" HERE);
}

error_code sys_tty_write(s32 ch, vm::cptr<char> buf, u32 len, vm::ptr<u32> pwritelen)
{
	sys_tty.notice("sys_tty_write(ch=%d, buf=*0x%x, len=%d, pwritelen=*0x%x)", ch, buf, len, pwritelen);

	if (ch > 15)
	{
		return CELL_EINVAL;
	}

	const u32 written_len = static_cast<s32>(len) > 0 ? len : 0;

	if (written_len > 0 && g_tty)
	{
		// Lock size by making it negative
		g_tty_size -= (1ll << 48);
		g_tty.write(buf.get_ptr(), len);
		g_tty_size += (1ll << 48) + len;
	}

	if (!pwritelen)
	{
		return CELL_EFAULT;
	}

	*pwritelen = written_len;

	return CELL_OK;
}

error_code sys_console_write(vm::cptr<char> buf, u32 len)
{
	sys_tty.notice("sys_console_write(buf=*0x%x, len=%d)", buf, len);

	const u32 written_len = static_cast<s32>(len) > 0 ? len : 0;

	if (written_len > 0 && g_tty)
	{
		// Lock size by making it negative
		g_tty_size -= (1ll << 48);
		g_tty.write(buf.get_ptr(), len);
		g_tty_size += (1ll << 48) + len;
	}

	return CELL_OK;
}