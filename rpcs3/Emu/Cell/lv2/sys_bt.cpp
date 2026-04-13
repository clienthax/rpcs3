#include "stdafx.h"

#include "Emu/Cell/ErrorCodes.h"

#include "sys_bt.h"

LOG_CHANNEL(sys_bt);

error_code sys_bluetooth_aud_serial_579()
{
	// Don't care about this for now, hush.
	return CELL_OK;
}

error_code sys_bluetooth_638_if(s32 cmd, vm::ptr<void> arg_in, vm::ptr<void> buf_out)
{
	sys_bt.todo("sys_bluetooth_638_if(cmd=%d, arg_in=*0x%x, buf_out=*0x%x)", arg_in, buf_out);
	return CELL_OK;
}


error_code sys_bt_something_648()
{
	// Deprecated.
	return CELL_ENOSYS;
}