#include "stdafx.h"
#include "sys_tty.h"
#include "sys_hid.h"

#include "sys_uart.h"
#include "sys_ppu_thread.h"

logs::channel sys_hid("sys_hid");

s32 sys_hid_manager_check_focus()
{
	sys_hid.todo("sys_hid_manager_check_focus()");
	//Seems to just return 0 if the focus hasnt been stolen  ???
	return CELL_OK;
}

error_code sys_hid_514(u32 a, vm::ptr<u32> b, u32 c)
{
	sys_hid.todo("sys_hid_514(0x%x, 0x%x, 0x%x)", a, b, c);
	return CELL_OK;
}