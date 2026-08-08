#include "stdafx.h"
#include "Emu/Cell/ErrorCodes.h"

#include "sys_console.h"

LOG_CHANNEL(sys_console);

extern fs::file g_tty;
extern atomic_t<s64> g_tty_size;

error_code sys_console_write(vm::cptr<char> buf, u32 len)
{
	sys_console.notice("sys_console_write(buf=*0x%x, len=0x%x)", buf, len);

	if (static_cast<s32>(len) <= 0)
	{
		return CELL_OK;
	}

	std::string msg;

	if (vm::check_addr(buf.addr(), vm::page_readable, len))
	{
		msg.resize(len);

		if (!vm::try_access(buf.addr(), msg.data(), len, false))
		{
			msg.clear();
		}
	}

	if (msg.empty())
	{
		return {CELL_EFAULT, buf.addr()};
	}

	if (msg.ends_with("\n"))
	{
		const std::string_view msg_clear = std::string_view(msg).substr(0, msg.find_last_not_of('\n') + 1);
		sys_console.notice(u8"sys_console_write(): \"%s\" << endl", msg_clear);
	}
	else
	{
		sys_console.notice(u8"sys_console_write(): \"%s\"", msg);
	}

	if (g_tty)
	{
		g_tty_size -= (1ll << 48);
		g_tty.write(msg);
		g_tty_size += (1ll << 48) + len;
	}

	return CELL_OK;
}
