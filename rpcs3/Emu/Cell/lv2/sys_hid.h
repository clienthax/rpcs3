#pragma once

#include "Emu/Memory/Memory.h"
#include "Emu/Cell/ErrorCodes.h"

// SysCalls
error_code sys_hid_510(vm::ps3::ptr<u32> arg);
error_code sys_hid_514(u32 a, vm::ps3::ptr<u32> b, u32 c);
error_code sys_game_set_system_sw_version(vm::ps3::ptr<u32> version);
u32 sys_game_get_system_sw_version();
error_code sys_ss_get_boot_device(vm::ps3::ptr<u64> buf);