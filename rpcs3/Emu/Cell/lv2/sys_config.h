#pragma once

#include "Emu/Memory/Memory.h"
#include "Emu/Cell/ErrorCodes.h"

#include "sys_event.h"

struct lv2_config
{
	static const u32 id_base = 0x01000000; // TODO all of these are just random
	static const u32 id_step = 1;
	static const u32 id_count = 2048;

	std::weak_ptr<lv2_event_queue> queue;
};

// SysCalls
error_code sys_config_open(u32 equeue_id, vm::ptr<u32> config_id);
error_code sys_config_close(u32 equeue_id);
error_code sys_config_register_service(u32 config_id, s32 b, u32 c, u32 d, vm::ptr<u32> data, u32 size, vm::ptr<u32> output);
error_code sys_config_add_service_listener(u32 config_id, s32 b, u32 c, u32 d, u32 e, u32 f, u32 g);