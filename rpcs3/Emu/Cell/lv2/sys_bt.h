#pragma once

#include "Emu/Memory/vm_ptr.h"
#include "Emu/Cell/ErrorCodes.h"

// SysCalls

error_code sys_bluetooth_aud_serial_579();
error_code sys_bluetooth_638_if(s32 cmd, vm::ptr<void> arg_in, vm::ptr<void> buf_out);
error_code sys_bt_something_648();