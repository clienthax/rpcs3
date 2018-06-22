#pragma once

#include "Emu/Memory/Memory.h"
#include "Emu/Cell/ErrorCodes.h"

// SysCalls
s32 sys_hid_manager_check_focus();
error_code sys_hid_514(u32 a, vm::ptr<u32> b, u32 c);