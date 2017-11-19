#include "stdafx.h"
#include "sys_tty.h"

#include "sys_uart.h"
#include "sys_ppu_thread.h"
#include "sys_hid.h"

namespace vm { using namespace ps3; }

logs::channel sys_hid("sys_hid");

error_code sys_hid_510(vm::ptr<u32> arg)
{
	sys_hid.todo("sys_hid_510(0x%x)",arg);
	return 0;
}

error_code sys_hid_514(u32 a, vm::ptr<u32> b, u32 c)
{
	sys_hid.todo("sys_hid_514(0x%x, 0x%x, 0x%x)", a, b, c);
	return CELL_OK;
}

error_code sys_game_set_system_sw_version(vm::ptr<u32> version)
{
	sys_hid.todo("sys_game_set_system_sw_version(%d)", version);
	return CELL_OK;

}

u32 sys_game_get_system_sw_version() {
	sys_hid.todo("sys_game_set_system_sw_version()");
	return 0xbbe4;//48100
}

error_code sys_ss_get_boot_device(vm::ps3::ptr<u64> buf) {
	sys_hid.todo("sys_ss_get_boot_device()");

	*buf = 0;
	return CELL_OK;
}
