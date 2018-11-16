#pragma once

struct lv2_config
{
	static const u32 id_base = 0x41000000;
	static const u32 id_step = 1;
	static const u32 id_count = 2048;

	std::weak_ptr<lv2_event_queue> queue;
};

struct sys_config_extended_device_info {
	be_t<u16> hid_device_type; // 0x0 
	be_t<u16> unk; // 0x2
	be_t<u16> unk2; // 0x4
	be_t<u16> unk3; // 0x6
	be_t<u16> unk4; // 0x8
	be_t<u16> vid; // 0xa vid
	be_t<u16> pid; // 0xb pid
	be_t<u16> bdaddr1;
	be_t<u16> bdaddr2;
	be_t<u16> bdaddr3;
	be_t<u16> bdaddr4;
};

struct sys_config_service_event_available {
	be_t<u32> service_listener_handle;
	be_t<u32> unk2;
	be_t<u64> logical_port; // logical port, 0 - 255 for kb/io/mouse?
	be_t<u64> device_no;         // or possibly called port_no
	be_t<u64> unk5;
	be_t<u64> has_dev_info; // 0x20,
	sys_config_extended_device_info hid_info; // 0x28
};

// syscalls
error_code sys_config_open(u32 equeue_id, vm::ptr<u32> config_id);
error_code sys_config_close(u32 equeue_id);
error_code sys_config_register_service(ppu_thread& ppu, u32 config_id, s64 b, u32 c, u32 d, vm::ptr<u32> data, u32 size, vm::ptr<u32> output);
error_code sys_config_add_service_listener(u32 config_id, s64 id, u32 c, u32 d, u32 unk, u32 f, vm::ptr<u32> service_listener_handle);
error_code sys_config_get_service_event(u32 config_id, u32 event_id, vm::ptr<void> event, u64 size);
