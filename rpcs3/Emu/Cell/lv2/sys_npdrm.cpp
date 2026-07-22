#include "stdafx.h"
#include "Emu/System.h"

#include "Emu/Cell/ErrorCodes.h"
#include "Emu/Cell/PPUThread.h"
#include "Emu/Cell/lv2/sys_process.h"

#include "sys_npdrm.h"


LOG_CHANNEL(sys_npdrm);

error_code sys_npdrm_check_ekc(u32 pid, vm::ptr<np_drm_info_t> info)
{
	sys_npdrm.todo("sys_npdrm_check_ekc(pid=0x%x, np_drm_info_t=*0x%x)", pid, info);

	const auto& d = *info;
	sys_npdrm.todo(
		"np_drm_info: magic=0x%x version=0x%x drm_type=0x%x type=0x%x content_id=%.48s", d.magic, d.version, d.drm_type, d.type_, d.content_id
	);

	// Doesn't seem to copy any data back to userland
	// 1 = good
	return not_an_error(1);
}

error_code sys_npdrm_regist_ekc(u32 pid, vm::ptr<char> titleId, vm::ptr<void> klicensee, vm::ptr<u8> actdat, vm::ptr<u8> rif, s32 licenseType, vm::ptr<u8> magicVersion)
{
	sys_npdrm.todo("sys_npdrm_regist_ekc(pid=0x%x, titleId=*0x%x, klicensee=*0x%x, actdat=*0x%x, rif=*0x%x, licenseType=0x%x, magicVersion=*0x%x)", pid, titleId, klicensee, actdat, rif, licenseType, magicVersion);

	// Just returns 80010002 in 446 lv2
	return 0x80010002;
}

error_code sys_npdrm_regist_ekc2(u32 pid, vm::ptr<np_drm_info_t> info, vm::ptr<void> klicensee, vm::ptr<u8> actdat, vm::ptr<u8> rif, u8 magicVersion)
{
	sys_npdrm.todo("sys_npdrm_regist_ekc2(pid=0x%x, info=*0x%x, klicensee=*0x%x, actdat=*0x%x, rif=*0x%x, magicVersion=0x%x)", pid, info, klicensee, actdat, rif, magicVersion);
	const auto& d = *info;
	sys_npdrm.todo(
		"np_drm_info: magic=0x%x version=0x%x drm_type=0x%x type=0x%x content_id=%.48s", d.magic, d.version, d.drm_type, d.type_, d.content_id
	);

	// Doesn't seem to copy any data back to userland
	return CELL_OK;
}

// 0x1dc
error_code sys_npdrm_476(int type, vm::ptr<char> title_id, vm::ptr<char> user_str, u64 str_len)
{
	sys_npdrm.todo("sys_npdrm_476(type=%d, title_id=%s, user_str=*0x%x, str_len=0x%x)", type, title_id, user_str, str_len);
	Emu.Pause();

	// TODO process root check

	if (!g_ps3_process_info.has_root_perm())
	{
		return CELL_ENOSYS;
	}

	if (type == 1)
	{
		// TODO
		return CELL_OK;
	}

	if (type == 0)
	{
		if (!title_id)
		{
			return CELL_EINVAL;
		}

		// return _game_start(g_game_ctx, title_id, 0, null, 0)
		return CELL_OK;
	}

	if (type == 3)
	{
		if (!user_str || str_len == 0)
		{
			return CELL_EINVAL;
		}

		// return _game_start(g_game_ctx, title_id, 0, null, 0)
		return CELL_OK;
	}

	return CELL_ENOSYS;
}


