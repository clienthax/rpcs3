#pragma once

#include "Emu/Memory/vm_ptr.h"
#include "Emu/Cell/Modules/cellRtc.h"
#include "Emu/Cell/ErrorCodes.h"

struct np_drm_info_t {
	be_t<u32> magic;
	be_t<u32> version;
	be_t<u32> drm_type;
	be_t<u32> type_;
	char content_id[0x30];
	CellRtcTick validity_start;
	CellRtcTick validity_end;
	CellRtcTick current_tick;
	CellRtcTick current_secure_tick;
};

// SysCalls

error_code sys_npdrm_check_ekc(u32 pid, vm::ptr<np_drm_info_t> info);
error_code sys_npdrm_regist_ekc(u32 pid, vm::ptr<char> titleId, vm::ptr<void> klicensee, vm::ptr<u8> actdat, vm::ptr<u8> rif, s32 licenseType, vm::ptr<u8> magicVersion);
error_code sys_npdrm_regist_ekc2(u32 pid, vm::ptr<np_drm_info_t> info, vm::ptr<void> klicensee, vm::ptr<u8> actdat, vm::ptr<u8> rif, u8 magicVersion);
error_code sys_npdrm_476(int type, vm::ptr<char> title_id, vm::ptr<char> user_str, u64 str_len);

