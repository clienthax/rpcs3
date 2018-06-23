#include "stdafx.h"
#include "Emu/Memory/Memory.h"
#include "Emu/System.h"
#include "Emu/IdManager.h"

#include "Emu/Cell/ErrorCodes.h"

#include "sys_gpio.h"

error_code sys_gpio_get(u64 device_id, vm::ptr<u64> value)
{
	if (device_id != SYS_GPIO_LED_DEVICE_ID && device_id != SYS_GPIO_DIP_SWITCH_DEVICE_ID)
	{
		return CELL_ESRCH;
	}

	if (!vm::check_addr(value.addr(), sizeof(u64), vm::page_allocated | vm::page_writable))
	{
		return CELL_EFAULT;
	}

	// Retail consoles dont have LEDs or DIPs switches, hence always sets 0 in paramenter
	*value = 0;

	return CELL_OK;
}

error_code sys_gpio_set(u64 device_id, u64 mask, u64 value)
{
	// Retail consoles dont have LEDs or DIPs switches, hence the syscall can't modify devices's value
	switch (device_id)
	{
	case SYS_GPIO_LED_DEVICE_ID: return CELL_OK;
	case SYS_GPIO_DIP_SWITCH_DEVICE_ID: return CELL_EINVAL;
	}

	return CELL_ESRCH;
}

logs::channel sys_sm("sys_sm");

error_code sys_sm_get_params(vm::ptr<u8> a, vm::ptr<u8> b, vm::ptr<u32> c, vm::ptr<u64> d)
{
	sys_sm.todo("sys_sm_get_params(0x%x, 0x%x, 0x%x, 0x%x)", a, b, c, d);
	*a = 0;
	*b = 0;
	*c = 0x200;
	*d = 7;
	return CELL_OK;
}

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
		*config_id    = id;

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

error_code sys_config_register_service(u32 config_id, s32 b, u32 c, u32 d, vm::ptr<u32> data, u32 size, vm::ptr<u32> output)
{
	static u32 next_output = 0xcafebabe;

	// `size` is the length of `data`
	sys_config.todo("sys_config_register_service(config_id=0x%x, 0x%x, 0x%x, 0x%x, data=0x%x, size=0x%x, output=0x%x) -> 0x%x", config_id, b, c, d, data, size, output, next_output);

	if (b < 0)
	{
		return CELL_EINVAL;
	}

	*output = next_output++;

	return CELL_OK;
}

error_code sys_config_add_service_listener(u32 config_id, s64 id, u32 c, u32 d, vm::ptr<ServiceListenerCallback[2]> funcs, u32 f, u32 g)
{
	sys_config.todo("sys_config_add_service_listener(config_id=0x%x, id=0x%x, 0x%x, 0x%x, funcs=0x%x, 0x%x, 0x%x)", config_id, id, c, d, funcs, f, g);
	//	VSH, @61CE68 - add_service_listener is called with 2 `funcs`, first being an add controller functions, second being a remove controller

	auto start_func = *funcs[0];
	auto end_func   = *funcs[1];

	return CELL_OK;
}

logs::channel sys_rsxaudio("sys_rsxaudio");

error_code sys_rsxaudio_initialize(vm::ptr<u32> handle)
{
	sys_rsxaudio.todo("sys_rsxaudio_initialize()");
	*handle = 0xcacad0d0;
	return CELL_OK;
}

error_code sys_rsxaudio_import_shared_memory(u32 handle, vm::ptr<u64> b)
{
	sys_rsxaudio.todo("sys_rsxaudio_import_shared_memory(handle=0x%x, *0x%x)", handle, b);
	b[0] = vm::alloc(0x40000, vm::main);
	return CELL_OK;
}

logs::channel sys_storage("sys_storage");

/* Devices */
#define ATA_HDD 0x101000000000007
#define BDVD_DRIVE 0x101000000000006
#define PATA0_HDD_DRIVE 0x101000000000008
#define PATA0_BDVD_DRIVE BDVD_DRIVE
#define PATA1_HDD_DRIVE ATA_HDD
#define BUILTIN_FLASH 0x100000000000001
#define NAND_FLASH BUILTIN_FLASH
#define NOR_FLASH 0x100000000000004
#define MEMORY_STICK 0x103000000000010
#define SD_CARD 0x103000100000010
#define COMPACT_FLASH 0x103000200000010
#define USB_MASS_STORAGE_1(n) (0x10300000000000A + n)       /* For 0-5 */
#define USB_MASS_STORAGE_2(n) (0x10300000000001F + (n - 6)) /* For 6-127 */

s32 sys_storage_open(u64 device, u32 mode, vm::ptr<u32> fd, u32 flags)
{
	sys_storage.todo("sys_storage_open(device=0x%x, mode=0x%x, fd=*0x%x, 0x%x)", device, mode, fd, flags);

	u64 storage = device & 0xFFFFF00FFFFFFFF;
	if (storage != ATA_HDD && storage != BDVD_DRIVE)
		fmt::throw_exception("unexpected device");
	static u32 dumbctr = 0xdadad0d0;
	*fd                = dumbctr++; // Poison value
	return CELL_OK;
}

s32 sys_storage_close(u32 fd)
{
	sys_storage.todo("sys_storage_close(fd=0x%x)", fd);
	return CELL_OK;
}

s32 sys_storage_read(u32 fd, u32 mode, u32 start_sector, u32 num_sectors, vm::ptr<u8> bounce_buf, vm::ptr<u32> sectors_read, u64 flags)
{
	sys_storage.todo("sys_storage_read(fd=0x%x, mode=0x%x, start_sector=0x%x, num_sectors=0x%x, bounce_buf=*0x%x, sectors_read=*0x%x, flags=0x%x)", fd, mode, start_sector, num_sectors, bounce_buf,
	    sectors_read, flags);
	*sectors_read = num_sectors; // LIESSSSS!
	memset(bounce_buf.get_ptr(), 0, num_sectors * 0x10000);
	return CELL_OK;
}

s32 sys_storage_write()
{
	sys_storage.todo("sys_storage_write()");
	return CELL_OK;
}

s32 sys_storage_send_device_command()
{
	sys_storage.todo("sys_storage_send_device_command()");
	return CELL_OK;
}

s32 sys_storage_async_configure()
{
	sys_storage.todo("sys_storage_async_configure()");
	return CELL_OK;
}

s32 sys_storage_async_read()
{
	sys_storage.todo("sys_storage_async_read()");
	return CELL_OK;
}

s32 sys_storage_async_write()
{
	sys_storage.todo("sys_storage_async_write()");
	return CELL_OK;
}

s32 sys_storage_async_cancel()
{
	sys_storage.todo("sys_storage_async_cancel()");
	return CELL_OK;
}

s32 sys_storage_get_device_info(u64 device, vm::ptr<StorageDeviceInfo> buffer)
{
	sys_storage.todo("sys_storage_get_device_info(device=0x%x, config=0x%x)", device, buffer);
	if (!buffer)
		fmt::throw_exception("rawr");

	memset(buffer.get_ptr(), 0, sizeof(StorageDeviceInfo));

	u64 storage = device & 0xFFFFF00FFFFFFFF;
	u32 dev_num = (device >> 32) & 0xFF;

	if (storage == ATA_HDD) // dev_hdd?
	{
		if (dev_num > 2)
			return -5;

		std::string u = "unnamed";
		memcpy(buffer->name, u.c_str(), u.size());
		buffer->sector_size = 0x200;
		buffer->one         = 1;
		buffer->flags[1]    = 1;
		buffer->flags[2]    = 1;
		buffer->flags[7]    = 1;

		// set partition size based on dev_num
		// stole these sizes from kernel dump, unknown if they are 100% correct
		// vsh reports only 2 partitions even though there is 3 sizes
		switch (dev_num)
		{
		case 0:
			buffer->sector_count = 0x2542EAB0; // possibly total size
		case 1:
			buffer->sector_count = 0x24FAEA98; // which makes this hdd0
		case 2:
			buffer->sector_count = 0x3FFFF8; // and this one hdd1
		}
	}
	else if (storage == BDVD_DRIVE) //	dev_bdvd?
	{
		if (dev_num > 0)
			return -5;
		std::string u = "unnamed";
		memcpy(buffer->name, u.c_str(), u.size());
		buffer->sector_count = 0x4D955;
		buffer->sector_size  = 0x800;
		buffer->one          = 1;
		buffer->flags[1]     = 0;
		buffer->flags[2]     = 1;
		buffer->flags[7]     = 1;
	}
	else
		fmt::throw_exception("rawr2");

	return CELL_OK;
}

s32 sys_storage_get_device_config(vm::ptr<u32> storages, vm::ptr<u32> devices)
{
	sys_storage.todo("sys_storage_get_device_config(storages=*0x%x, devices=*0x%x)", storages, devices);

	*storages = 6;
	*devices  = 17;

	return CELL_OK;
}

s32 sys_storage_report_devices(u32 storages, u32 start, u32 devices, vm::ptr<u64> device_ids)
{
	sys_storage.todo("sys_storage_report_devices(storages=0x%x, start=0x%x, devices=0x%x, device_ids=0x%x)", storages, start, devices, device_ids);

	std::array<u64, 0x11> all_devs;

	all_devs[0] = 0x10300000000000A;
	for (int i = 0; i < 7; ++i)
	{
		all_devs[i + 1] = 0x100000000000001 | ((u64)(i) << 32);
	}

	for (int i = 0; i < 3; ++i)
	{
		all_devs[i + 8] = 0x101000000000007 | ((u64)(i) << 32);
	}

	all_devs[11] = 0x101000000000006;

	for (int i = 0; i < 4; ++i)
	{
		all_devs[i + 12] = 0x100000000000004 | ((u64)(i) << 32);
	}

	all_devs[16] = 0x100000000000003;

	for (int i = 0; i < devices; ++i)
	{
		device_ids[i] = all_devs[start++];
	}

	return CELL_OK;
}

s32 sys_storage_configure_medium_event(u32 fd, u32 equeue_id, u32 c)
{
	sys_storage.todo("sys_storage_configure_medium_event(fd=0x%x, equeue_id=0x%x, 0x%x)", fd, equeue_id, c);
	return CELL_OK;
}

s32 sys_storage_set_medium_polling_interval()
{
	sys_storage.todo("sys_storage_set_medium_polling_interval()");
	return CELL_OK;
}

s32 sys_storage_create_region()
{
	sys_storage.todo("sys_storage_create_region()");
	return CELL_OK;
}

s32 sys_storage_delete_region()
{
	sys_storage.todo("sys_storage_delete_region()");
	return CELL_OK;
}

s32 sys_storage_execute_device_command(u32 fd, u64 cmd, vm::ptr<char> cmdbuf, u64 cmdbuf_size, vm::ptr<char> databuf, u64 databuf_size, vm::ptr<u32> driver_status)
{
	sys_storage.todo("sys_storage_execute_device_command(fd=0x%x, cmd=0x%x, cmdbuf=*0x%x, cmdbuf_size=0x%x, databuf=*0x%x, databuf_size=0x%x, driver_status=*0x%x)", fd, cmd, cmdbuf, cmdbuf_size, databuf,
	    databuf_size, driver_status);

	//	cmd == 2 is device mount or something similar, called per device that it finds
	return CELL_OK;
}

s32 sys_storage_check_region_acl()
{
	sys_storage.todo("sys_storage_check_region_acl()");
	return CELL_OK;
}

s32 sys_storage_set_region_acl()
{
	sys_storage.todo("sys_storage_set_region_acl()");
	return CELL_OK;
}

s32 sys_storage_async_send_device_command()
{
	sys_storage.todo("sys_storage_async_send_device_command()");
	return CELL_OK;
}

s32 sys_storage_get_region_offset()
{
	sys_storage.todo("sys_storage_get_region_offset()");
	return CELL_OK;
}

s32 sys_storage_set_emulated_speed()
{
	sys_storage.todo("sys_storage_set_emulated_speed()");
	return CELL_OK;
}

logs::channel sys_uart("sys_uart");

error_code sys_uart_initialize()
{
	sys_uart.todo("sys_uart_initialize()");
	return CELL_OK;
}

error_code sys_uart_receive(ppu_thread& ppu, vm::ptr<char> buffer, u64 size, u32 unk)
{
	if (size != 0x800)
		return 0; // HACK: Unimplemented - we only send the 'stop waiting' packet, or whatever

	struct uart_required_packet
	{
		struct
		{
			be_t<u8> unk1;
			be_t<u8> unk2;
			be_t<u16> payload_size;
		} header;
		be_t<u32> type;     //??
		be_t<u32> cmd;      // < 0x13, in sub_5DA13C
		be_t<u32> cmd_size; // < cmd_size?
		u8 data_stuff[0x40];
	};
	auto packets = reinterpret_cast<uart_required_packet*>(buffer.get_ptr());

	be_t<u32> types[]{0x80000004, 0x1000001, 0xffffffff};

	int i = 0;
	for (auto type : types)
	{
		uart_required_packet& packet = packets[i++];
		packet.header.payload_size   = sizeof(packet) - sizeof(packet.header);
		packet.type                  = type;
		packet.cmd                   = 15; // 0 or 15 return 0
	}

	return sizeof(uart_required_packet) * sizeof(types) / sizeof(types[0]);
}

error_code sys_uart_send(ppu_thread& ppu, vm::cptr<u8> buffer, u64 size, u64 flags)
{
	sys_uart.todo("sys_uart_send(buffer=0x%x, size=0x%llx, flags=0x%x)", buffer, size, flags);
	return size;
}

error_code sys_uart_get_params(vm::ptr<char> buffer)
{
	// buffer's size should be 0x10
	sys_uart.todo("sys_uart_get_params(buffer=0x%x)", buffer);
	return CELL_OK;
}

error_code sys_console_write(vm::cptr<char> buf, u32 len)
{
	std::string tmp(buf.get_ptr(), len);
	sys_sm.todo("console: %s", tmp);
	return CELL_OK;
}

error_code sys_hid_manager_510()
{
	return CELL_OK;
}
error_code sys_hid_manager_514()
{
	return CELL_OK;
}

error_code sys_sm_get_ext_event2()
{
	return CELL_OK;
}

logs::channel sys_btsetting("sys_btsetting");
error_code sys_btsetting_if(u64 cmd, vm::ptr<void> msg)
{
	sys_btsetting.todo("sys_btsetting_if(cmd=0x%x, msg=*0%x)", cmd, msg);

	if (cmd == 0)
	{
		// init
		struct BtInitPacket
		{
			be_t<u32> equeue_id;
			be_t<u32> pad;
			be_t<u32> page_proc_addr;
			be_t<u32> pad_;
		};

		auto packet = vm::static_ptr_cast<BtInitPacket>(msg);

		sys_btsetting.todo("init page_proc_addr =0x%x", packet->page_proc_addr);
		// second arg controls message
		// lets try just disabling for now
		sys_event_port_send(packet->equeue_id, 0, 0xDEAD, 0);
	}
	else fmt::throw_exception("unhandled btpckt"); 

	return CELL_OK;
}