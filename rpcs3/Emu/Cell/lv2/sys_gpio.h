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
error_code sys_config_register_service(ppu_thread& ppu, u32 config_id, s64 b, u32 c, u32 d, vm::ptr<u32> data, u32 size, vm::ptr<u32> output);
error_code sys_config_add_service_listener(u32 a, s64 b, u32 c, u32 d, vm::ptr<ServiceListenerCallback[2]> funcs, u32 f, u32 g);

error_code sys_rsxaudio_initialize(vm::ptr<u32>);
error_code sys_rsxaudio_import_shared_memory(u32, vm::ptr<u64>);

s32 sys_storage_open(u64 device, u32 b, vm::ptr<u32> fd, u32 d);
s32 sys_storage_close(u32 fd);
s32 sys_storage_read(u32 fd, u32 mode, u32 start_sector, u32 num_sectors, vm::ptr<u8> bounce_buf, vm::ptr<u32> sectors_read, u64 flags);
s32 sys_storage_write();
s32 sys_storage_send_device_command(u32 dev_handle, u64 cmd, vm::ptr<void> in, u64 inlen, vm::ptr<void> out, u64 outlen);
s32 sys_storage_async_configure(u32 fd, u32 io_buf, u32 equeue_id, u32 unk);
s32 sys_storage_async_read();
s32 sys_storage_async_write();
s32 sys_storage_async_cancel();

#pragma pack(push, 1)
struct StorageDeviceInfo
{
	u8 name[0x20];    // 0x0
	be_t<u32> zero; // 0x20
	be_t<u32> zero2;  // 0x24 
	be_t<u64> sector_count; // 0x28
	be_t<u32> sector_size; // 0x30
	be_t<u32> one; // 0x34
	u8 flags[8]; // 0x38
};
#pragma pack(pop)

static_assert(sizeof(StorageDeviceInfo) == 0x40, "StorageDeviceInfoSizeTest");

s32 sys_storage_get_device_info(u64 device, vm::ptr<StorageDeviceInfo> buffer);
s32 sys_storage_get_device_config(vm::ptr<u32> storages, vm::ptr<u32> devices);
s32 sys_storage_report_devices(u32 storages, u32 start, u32 devices, vm::ptr<u64> device_ids);
s32 sys_storage_configure_medium_event(u32 fd, u32 equeue_id, u32 c);
s32 sys_storage_set_medium_polling_interval();
s32 sys_storage_create_region();
s32 sys_storage_delete_region();
s32 sys_storage_execute_device_command(u32 fd, u64 cmd, vm::ptr<char> cmdbuf, u64 cmdbuf_size, vm::ptr<char> databuf, u64 databuf_size, vm::ptr<u32> driver_status);
s32 sys_storage_check_region_acl();
s32 sys_storage_set_region_acl();
s32 sys_storage_async_send_device_command(u32 dev_handle, u64 cmd, vm::ptr<void> in, u64 inlen, vm::ptr<void> out, u64 outlen, u64 unk);
s32 sys_storage_get_region_offset();
s32 sys_storage_set_emulated_speed();

struct uart_packet
{
	u8 magic;
	u8 size;
	u8 cmd;
	u8 data[1];
};

// SysCalls
error_code sys_uart_initialize();
error_code sys_uart_receive(vm::ptr<void> buffer, u64 size, u32 unk);
error_code sys_uart_send(vm::cptr<void> buffer, u64 size, u64 flags);
error_code sys_uart_get_params(vm::ptr<char> buffer);

error_code sys_console_write(ppu_thread& ppu, vm::cptr<char> buf, u32 len);

error_code sys_hid_manager_510();
error_code sys_hid_manager_514();

error_code sys_sm_get_ext_event2(vm::ptr<u64> a1, vm::ptr<u64> a2, vm::ptr<u64> a3, u64 a4);
error_code sys_sm_shutdown(u16 op, vm::ptr<void> param, u64 size);
error_code sys_sm_get_params(vm::ptr<u8> a, vm::ptr<u8> b, vm::ptr<u32> c, vm::ptr<u64> d);
error_code sys_sm_control_led(u8 led, u8 action);

error_code sys_btsetting_if(u64 cmd, vm::ptr<void> msg);
error_code sys_bdemu_send_command(u64 cmd, u64 a2, u64 a3, vm::ptr<void> buf, u64 buf_len);

error_code sys_io_buffer_create(u32 block_count, u32 block_size, u32 blocks, u32 unk1, vm::ptr<u32> handle);
error_code sys_io_buffer_destroy(u32 handle);
error_code sys_io_buffer_allocate(u32 handle, vm::ptr<u32> block);
error_code sys_io_buffer_free(u32 handle, u32 block);