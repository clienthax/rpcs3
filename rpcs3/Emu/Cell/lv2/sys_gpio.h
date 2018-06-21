#pragma once

#include "sys_event.h"

enum : u64
{
	SYS_GPIO_UNKNOWN_DEVICE_ID,
	SYS_GPIO_LED_DEVICE_ID,
	SYS_GPIO_DIP_SWITCH_DEVICE_ID,
};

error_code sys_gpio_get(u64 device_id, vm::ptr<u64> value);
error_code sys_gpio_set(u64 device_id, u64 mask, u64 value);

error_code sys_sm_get_params(vm::ptr<u8> a, vm::ptr<u8> b, vm::ptr<u32> c, vm::ptr<u64> d);

struct lv2_config
{
	static const u32 id_base  = 0x01000000; // TODO all of these are just random
	static const u32 id_step  = 1;
	static const u32 id_count = 2048;

	std::weak_ptr<lv2_event_queue> queue;
};

typedef vm::ptr<void()> ServiceListenerCallback;

// SysCalls
error_code sys_config_open(u32 equeue_id, vm::ptr<u32> config_id);
error_code sys_config_close(u32 equeue_id);
error_code sys_config_register_service(u32 config_id, s32 b, u32 c, u32 d, vm::ptr<u32> data, u32 size, vm::ptr<u32> output);
error_code sys_config_add_service_listener(u32 a, s64 b, u32 c, u32 d, vm::ptr<ServiceListenerCallback[2]> funcs, u32 f, u32 g);

error_code sys_rsxaudio_initialize(vm::ptr<u32>);
error_code sys_rsxaudio_import_shared_memory(u32, vm::ptr<u64>);

s32 sys_storage_open(u64 device, u32 b, vm::ptr<u32> fd, u32 d);
s32 sys_storage_close(u32 fd);
s32 sys_storage_read(u32 fd, u32 mode, u32 start_sector, u32 num_sectors, vm::ptr<u8> bounce_buf, vm::ptr<u32> sectors_read, u64 flags);
s32 sys_storage_write();
s32 sys_storage_send_device_command();
s32 sys_storage_async_configure();
s32 sys_storage_async_read();
s32 sys_storage_async_write();
s32 sys_storage_async_cancel();
s32 sys_storage_get_device_info(u64 device, vm::ptr<u8> buffer);
s32 sys_storage_get_device_config(vm::ptr<u32> storages, vm::ptr<u32> devices);
s32 sys_storage_report_devices(u32 storages, u32 zero, u32 devices, vm::ptr<u64> device_ids);
s32 sys_storage_configure_medium_event(u32 fd, u32 equeue_id, u32 c);
s32 sys_storage_set_medium_polling_interval();
s32 sys_storage_create_region();
s32 sys_storage_delete_region();
s32 sys_storage_execute_device_command(u32 fd, u64 cmd, vm::ptr<char> cmdbuf, u64 cmdbuf_size, vm::ptr<char> databuf, u64 databuf_size, vm::ptr<u32> driver_status);
s32 sys_storage_check_region_acl();
s32 sys_storage_set_region_acl();
s32 sys_storage_async_send_device_command();
s32 sys_storage_get_region_offset();
s32 sys_storage_set_emulated_speed();