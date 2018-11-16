#include "stdafx.h"
#include "Emu/System.h"
#include "Emu/Memory/vm.h"
#include "Emu/IdManager.h"

#include "Emu/Cell/lv2/sys_event.h"
#include "Emu/Cell/ErrorCodes.h"

#include "sys_config.h"


logs::channel sys_config("sys_config");

error_code sys_config_open(u32 equeue_id, vm::ptr<u32> config_id)
{
	sys_config.warning("sys_config_open(equeue_id=0x%x, config_id=*0x%x)", equeue_id, config_id);

	const auto queue = idm::get<lv2_obj, lv2_event_queue>(equeue_id);

	if (!queue)
	{
		return CELL_ESRCH;
	}

	auto config = std::make_shared<lv2_config>();
	if (const u32 id = idm::import_existing<lv2_config>(std::move(config)))
	{
		config->queue = std::move(queue);
		*config_id = id;

		return CELL_OK;
	}

	return CELL_EAGAIN;
}

error_code sys_config_close(u32 config_id)
{
	sys_config.warning("sys_config_close(config_id=0x%x)", config_id);

	if (!idm::remove<lv2_config>(config_id))
	{
		return CELL_ESRCH;
	}

	return CELL_OK;
}

error_code sys_config_register_service(ppu_thread& ppu, u32 config_id, s64 b, u32 c, u32 d, vm::ptr<u32> data, u32 size, vm::ptr<u32> output)
{
	static u32 next_output = 0xcafebabe;

	// `size` is the length of `data`
	sys_config.todo("sys_config_register_service(config_id=0x%x, 0x%x, 0x%x, 0x%x, data=0x%x, size=0x%x, output=0x%x) -> 0x%x", config_id, b, c, d, data, size, output, next_output);

	if (b >= 0)
	{
		return CELL_EINVAL;
	}

	*output = next_output++;

	return CELL_OK;
}

error_code sys_config_add_service_listener(u32 config_id, s64 id, u32 c, u32 d, u32 unk, u32 f, vm::ptr<u32> service_listener_handle)
{
	sys_config.todo("sys_config_add_service_listener(config_id=0x%x, id=0x%x, 0x%x, 0x%x, funcs=0x%x, 0x%x, service_listener_handle=*0x%x)", config_id, id, c, d, unk, f, service_listener_handle);

	// id's, 0x8000000000000001 == libpad
	// 0x8000000000000002 == libkb
	// 0x8000000000000003 == libmouse

	static u32 listener_handles = 0x42000001;

	*service_listener_handle = listener_handles;
	listener_handles += 0x100; // unknown how these are used/incremented, this seems to be pretty close tho

	// low 32 bits is event_id, 33rd bit, can be either 1 or 0
	// 0 looks to be 'unavaiable' flag?
	// 1 is 'available' flag
	static u64 event_id = 0x100000001;
	const auto cfg = idm::get<lv2_config>(config_id);
	if (cfg && id == 0x8000000000000001ll) {
		if (auto q = cfg->queue.lock())
		{
			padlistenderhandle = *service_listener_handle;
			// 'source' in this case looks to be config_event_type:
			// 1 for service event
			// 2 for io error event
			// invalid for any others
			// data3 looks to be size of event to write
			q->send(1, config_id, event_id, 0x68);
			++event_id;
		}
	}
	else if (cfg && id == 0x11) {
		// 0x11 == padmanager?
		if (auto q = cfg->queue.lock())
		{
			padlistenderhandle = *service_listener_handle;
			// 'source' in this case looks to be config_event_type:
			// 1 for service event
			// 2 for io event
			// invalid for any others
			// data3 looks to be size of event to write
			q->send(1, config_id, event_id, 0x68);
			++event_id;
		}
	}

	return CELL_OK;
}

error_code sys_config_get_service_event(u32 config_id, u32 event_id, vm::ptr<void> event, u64 size) {
	sys_config.todo("sys_config_get_service_event(config_id=0x%x, event_id=0x%llx, event=*0x%llx, size=0x%llx)", config_id, event_id, event, size);

	// args...0x8, 0x10, 0x18, 0x28..or 0 if unk6 == 0,  0x20

	auto& ev = vm::static_ptr_cast<sys_config_service_event_available>(event);
	ev->service_listener_handle = padlistenderhandle;
	ev->logical_port = 1; // it doesnt look like 0 is valid?
	ev->unk2 = 0;
	ev->device_no = 0;
	ev->unk5 = 0;
	ev->has_dev_info = 1;
	ev->hid_info.hid_device_type = 1;

	return CELL_OK;
}
