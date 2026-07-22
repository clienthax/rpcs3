#include "stdafx.h"
#include "Emu/Cell/PPUModule.h"

#include "Emu/Cell/lv2/sys_mutex.h"
#include "Emu/Cell/lv2/sys_process.h"
#include "vshnet.h"

LOG_CHANNEL(vshnet);

// Does some shit with act.dat :<
error_code vshnet_0xEFB67F8E()
{

	return CELL_OK;
}

/*
DECLARE(ppu_module_manager::vshnet)("vshnet", []
{
	REG_VNID(vshnet, 0xEFB67F8E, vshnet_0xEFB67F8E);
});
*/