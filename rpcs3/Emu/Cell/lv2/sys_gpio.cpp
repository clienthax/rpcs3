#include "stdafx.h"
#include "Emu/Memory/Memory.h"
#include "Emu/System.h"
#include "Emu/IdManager.h"

#include "Emu/Cell/ErrorCodes.h"

#include "Emu/Cell/PPUThread.h"

#include "Emu/Cell/lv2/sys_event.h"
#include "Emu/Cell/lv2/sys_timer.h"
#include "Emu/Io/PadHandler.h"
#include "Emu/Cell/Modules/cellPad.h"

#include "sys_gpio.h"

#include "sys_tty.h"
#include "pad_thread.h"

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
	sys_sm.todo("sys_sm_get_params(a=*0x%x, b=*0x%x, c=*0x%x, d=*0x%x)", a, b, c, d);
	*a = 0;
	*b = 0;
	*c = 0x200;
	*d = 7;
	return CELL_OK;
}

error_code sys_sm_get_ext_event2(vm::ptr<u64> a1, vm::ptr<u64> a2, vm::ptr<u64> a3, u64 a4)
{
	sys_sm.trace("sys_sm_get_ext_event2(a1=*0x%x, a2=*0x%x, a3=*0x%x, a4=*0x%x, a4=0x%xll", a1, a2, a3, a4);
	if (a4 != 0 && a4 != 1)
		return CELL_EINVAL;

	// a1 == 7 - 'console too hot, restart'
	// a2 looks to be used if a1 is either 5 or 3?
	// a3 looks to be ignored in vsh

	*a1 = 0;
	*a2 = 0;
	*a3 = 0;

	// eagain for no event
	return not_an_error(CELL_EAGAIN);
}

error_code sys_sm_shutdown(u16 op, vm::ptr<void> param, u64 size)
{
	sys_sm.todo("sys_sm_shutdown(op=0x%x, param=*0x%x, size=0x%x", op, param, size);
	return CELL_OK;
}

error_code sys_console_write(ppu_thread& ppu, vm::cptr<char> buf, u32 len)
{
	sys_sm.todo("sys_console_write: buf=%s, len=0x%x", buf, len);
	// to make this easier to spot, also piping to tty
	std::string tmp(buf.get_ptr(), len);
	tmp = "CONSOLE: " + tmp;
	auto tty = vm::make_str(tmp);
	auto out = vm::var<u32>();
	sys_tty_write(0, tty, tmp.size(), out);
	return CELL_OK;
}

error_code sys_sm_control_led(u8 led, u8 action) {
	sys_sm.todo("sys_sm_control_led(led=0x%x, action=0x%x)", led, action);
	return CELL_OK;
}

error_code sys_sm_ring_buzzer(u64 packet, u64 a1, u64 a2) {
	sys_sm.todo("sys_sm_ring_buzzer(packet=0x%x, a1=0x%x, a2=0x%x)", packet, a1, a2);
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

u32 padlistenderhandle = 0;
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
#define NAND_UNK 0x100000000000003
#define NOR_FLASH 0x100000000000004
#define MEMORY_STICK 0x103000000000010
#define SD_CARD 0x103000100000010
#define COMPACT_FLASH 0x103000200000010
#define USB_MASS_STORAGE_1(n) (0x10300000000000A + n)       /* For 0-5 */
#define USB_MASS_STORAGE_2(n) (0x10300000000001F + (n - 6)) /* For 6-127 */

s32 sys_storage_open(u64 device, u64 mode, vm::ptr<u32> fd, u64 flags)
{
	sys_storage.todo("sys_storage_open(device=0x%x, mode=0x%x, fd=*0x%x, flags=0x%x)", device, mode, fd, flags);
	if (device == 0)
		return CELL_ENOENT;

	u64 storage_id = device & 0xFFFFF00FFFFFFFF;
	fs::file file;

	if (device == 0x100000200000004) {
		file = fs::file("G:/gitrepos/rpcs3/bin/imagedump/extnorarea.dat", fs::read);
		if (!file)
			sys_storage.fatal("couldnt find nor region file");
	}

	auto storage = std::make_shared<lv2_storage>(device, std::move(file), mode, flags);

	if (const u32 id = idm::import_existing<lv2_storage>(std::move(storage))) {

		*fd = id;
		return CELL_OK;
	}

	// idk error
	return CELL_EFAULT;
}

s32 sys_storage_close(u32 fd)
{
	sys_storage.todo("sys_storage_close(fd=0x%x)", fd);

	idm::remove<lv2_storage>(fd);

	return CELL_OK;
}

s32 sys_storage_read(u32 fd, u32 mode, u32 start_sector, u32 num_sectors, vm::ptr<void> bounce_buf, vm::ptr<u32> sectors_read, u64 flags)
{
	sys_storage.todo("sys_storage_read(fd=0x%x, mode=0x%x, start_sector=0x%x, num_sectors=0x%x, bounce_buf=*0x%x, sectors_read=*0x%x, flags=0x%x)", fd, mode, start_sector, num_sectors, bounce_buf,
	    sectors_read, flags);
	
	memset(bounce_buf.get_ptr(), 0, num_sectors * 0x200);

	auto handle = idm::get<lv2_storage>(fd);
	if (!handle)
		return CELL_ESRCH; // idk

	if (handle->device_id == 0x100000200000004 && handle->file) {
		handle->file.seek(start_sector * 0x200);
		u64 size = num_sectors * 0x200;
		const u64 result = handle->file.read(bounce_buf.get_ptr(), size);
		if (result != size) // woof
			fmt::throw_exception("didnt read expected");
	}

	*sectors_read = num_sectors;
	return CELL_OK;
}

s32 sys_storage_write(u32 fd, u32 mode, u32 start_sector, u32 num_sectors, vm::ptr<void> data, vm::ptr<u32> sectors_wrote, u64 flags)
{
	sys_storage.todo("sys_storage_write(fd=0x%x, mode=0x%x, start_sector=0x%x, num_sectors=0x%x, data=*=0x%x, sectors_wrote=*0x%x, flags=0x%llx)", fd, mode, start_sector, num_sectors, data, sectors_wrote, flags);
	*sectors_wrote = num_sectors;
	return CELL_OK;
}

s32 sys_storage_send_device_command(u32 dev_handle, u64 cmd, vm::ptr<void> in, u64 inlen, vm::ptr<void> out, u64 outlen)
{
	sys_storage.todo("sys_storage_send_device_command(dev_handle=0x%x, cmd=0x%x, in=*0x%, inlen=0x%x, out=*0x%x, outlen=0x%x)", dev_handle, cmd, in, inlen, out, outlen);
	return CELL_OK;
}

u32 asyncequeue;
s32 sys_storage_async_configure(u32 fd, u32 io_buf, u32 equeue_id, u32 unk)
{
	sys_storage.todo("sys_storage_async_configure(fd=0x%x, io_buf=0x%x, equeue_id=0x%x, unk=*0x%x)", fd, io_buf, equeue_id, unk);
	asyncequeue = equeue_id;
	return CELL_OK;
}

s32 sys_storage_async_send_device_command(u32 dev_handle, u64 cmd, vm::ptr<void> in, u64 inlen, vm::ptr<void> out, u64 outlen, u64 unk)
{
	sys_storage.todo("sys_storage_async_send_device_command(dev_handle=0x%x, cmd=0x%x, in=*0x%x, inlen=0x%x, out=*0x%x, outlen=0x%x, unk=0x%x)", dev_handle, cmd, in, inlen, out, outlen, unk);
	if (auto q = idm::get<lv2_obj, lv2_event_queue>(asyncequeue))
	{
		q->send(0, unk, unk, unk);
	}
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
	else if (storage == USB_MASS_STORAGE_1(0))
	{
		if (dev_num > 0)
			return -5;
		std::string u = "unnamed";
		memcpy(buffer->name, u.c_str(), u.size());
		/*buffer->sector_count = 0x4D955;*/
		buffer->sector_size  = 0x200;
		buffer->one          = 1;
		buffer->flags[1]     = 0;
		buffer->flags[2]     = 1;
		buffer->flags[7]     = 1;
	}
	else if (storage == NAND_FLASH)
	{
		if (dev_num > 6)
			return -5;

		std::string u = "unnamed";
		memcpy(buffer->name, u.c_str(), u.size());
		buffer->sector_size = 0x200;
		buffer->one         = 1;
		buffer->flags[1]    = 1;
		buffer->flags[2]    = 1;
		buffer->flags[7]    = 1;

		// see ata_hdd for explanation
		switch (dev_num)
		{
		case 0: buffer->sector_count = 0x80000;
		case 1: buffer->sector_count = 0x75F8;
		case 2: buffer->sector_count = 0x63E00;
		case 3: buffer->sector_count = 0x8000;
		case 4: buffer->sector_count = 0x400;
		case 5: buffer->sector_count = 0x2000;
		case 6: buffer->sector_count = 0x200;
		}
	}
	else if (storage == NOR_FLASH)
	{
		if (dev_num > 3)
			return -5;

		std::string u = "unnamed";
		memcpy(buffer->name, u.c_str(), u.size());
		buffer->sector_size = 0x200;
		buffer->one         = 1;
		buffer->flags[1]    = 0;
		buffer->flags[2]    = 1;
		buffer->flags[7]    = 1;

		// see ata_hdd for explanation
		switch (dev_num)
		{
		case 0: buffer->sector_count = 0x8000;
		case 1: buffer->sector_count = 0x77F8;
		case 2: buffer->sector_count = 0x100; // offset, 0x20000
		case 3: buffer->sector_count = 0x400;
		}
	}
	else if (storage == NAND_UNK)
	{
		if (dev_num > 1)
			return -5;

		std::string u = "unnamed";
		memcpy(buffer->name, u.c_str(), u.size());
		buffer->sector_size = 0x800;
		buffer->one         = 1;
		buffer->flags[1]    = 0;
		buffer->flags[2]    = 1;
		buffer->flags[7]    = 1;

		// see ata_hdd for explanation
		switch (dev_num)
		{
		case 0: buffer->sector_count = 0x7FFFFFFF;
		}
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

	//	cmd == 2 is get device info,
	// databuf, first byte 0 == status ok?
	// byte 1, if < 0 , not ata device
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

s32 sys_storage_get_region_offset()
{
	sys_storage.todo("sys_storage_get_region_offset()");
	return CELL_OK;
}

s32 sys_storage_set_emulated_speed()
{
	sys_storage.todo("sys_storage_set_emulated_speed()");
	// todo: only debug kernel has this
	return CELL_ENOSYS;
}

#define PS3AV_VERSION 0x205 /* version of ps3av command */

#define PS3AV_CID_AV_INIT 0x00000001
#define PS3AV_CID_AV_FIN 0x00000002
#define PS3AV_CID_AV_GET_HW_CONF 0x00000003
#define PS3AV_CID_AV_GET_MONITOR_INFO 0x00000004
#define PS3AV_CID_AV_GET_KSV_LIST_SIZE 0x00000005
#define PS3AV_CID_AV_ENABLE_EVENT 0x00000006
#define PS3AV_CID_AV_DISABLE_EVENT 0x00000007
#define PS3AV_CID_AV_GET_ALL_EDID 0x00000008
#define PS3AV_CID_AV_TV_MUTE 0x0000000a

#define PS3AV_CID_AV_VIDEO_CS 0x00010001
#define PS3AV_CID_AV_VIDEO_MUTE 0x00010002
#define PS3AV_CID_AV_VIDEO_DISABLE_SIG 0x00010003
#define PS3AV_CID_AV_VIDEO_UNK4 0x00010004
#define PS3AV_CID_AV_AUDIO_PARAM 0x00020001
#define PS3AV_CID_AV_AUDIO_MUTE 0x00020002
#define PS3AV_CID_AV_ACP_CTRL 0x00020003
#define PS3AV_CID_AV_SET_ACP_PACKET 0x00020004
#define PS3AV_CID_AV_UNK 0x0030001
#define PS3AV_CID_AV_UNK2 0x0030003
#define PS3AV_CID_AV_HDMI_MODE 0x00040001

#define PS3AV_CID_VIDEO_INIT 0x01000001
#define PS3AV_CID_VIDEO_MODE 0x01000002
#define PS3AV_CID_VIDEO_ROUTE 0x01000003 // 'route and color' ?
#define PS3AV_CID_VIDEO_FORMAT 0x01000004
#define PS3AV_CID_VIDEO_PITCH 0x01000005
#define PS3AV_CID_VIDEO_GET_HW_CONF 0x01000006
#define PS3AV_CID_VIDEO_REGVAL 0x01000008

#define PS3AV_CID_AUDIO_INIT 0x02000001
#define PS3AV_CID_AUDIO_MODE 0x02000002
#define PS3AV_CID_AUDIO_MUTE 0x02000003
#define PS3AV_CID_AUDIO_ACTIVE 0x02000004
#define PS3AV_CID_AUDIO_INACTIVE 0x02000005
#define PS3AV_CID_AUDIO_SPDIF_BIT 0x02000006
#define PS3AV_CID_AUDIO_CTRL 0x02000007

#define PS3AV_CID_EVENT_UNPLUGGED 0x10000001
#define PS3AV_CID_EVENT_PLUGGED 0x10000002
#define PS3AV_CID_EVENT_HDCP_DONE 0x10000003
#define PS3AV_CID_EVENT_HDCP_FAIL 0x10000004
#define PS3AV_CID_EVENT_HDCP_AUTH 0x10000005
#define PS3AV_CID_EVENT_HDCP_ERROR 0x10000006

#define PS3AV_CID_AVB_PARAM 0x04000001 // 'super packet'

#define PS3AV_REPLY_BIT 0x80000000

#define PS3AV_RESBIT_720x480P 0x0003 /* 0x0001 | 0x0002 */
#define PS3AV_RESBIT_720x576P 0x0003 /* 0x0001 | 0x0002 */
#define PS3AV_RESBIT_1280x720P 0x0004
#define PS3AV_RESBIT_1920x1080I 0x0008
#define PS3AV_RESBIT_1920x1080P 0x4000

#define PS3AV_MONITOR_TYPE_HDMI 1 /* HDMI */
#define PS3AV_MONITOR_TYPE_DVI 2  /* DVI */

logs::channel sys_uart("sys_uart");

error_code sys_uart_initialize()
{
	sys_uart.todo("sys_uart_initialize()");
	return CELL_OK;
}

struct uart_payload
{
	u32 version;
	std::vector<u32> data;
};
std::deque<uart_payload> payloads;
semaphore<> mutex;

// helper header for reply commands
struct ps3av_reply_cmd_hdr
{
	be_t<u16> version;
	be_t<u16> length;
	be_t<u32> cid;
	be_t<u32> status;
};

struct ps3av_header
{
	be_t<u16> version;
	be_t<u16> length;
};

struct ps3av_info_resolution
{
	be_t<u32> res_bits;
	be_t<u32> native;
};

struct ps3av_info_cs
{
	u8 rgb;
	u8 yuv444;
	u8 yuv422;
	u8 reserved;
};

struct ps3av_info_color
{
	be_t<u16> red_x;
	be_t<u16> red_y;
	be_t<u16> green_x;
	be_t<u16> green_y;
	be_t<u16> blue_x;
	be_t<u16> blue_y;
	be_t<u16> white_x;
	be_t<u16> white_y;
	be_t<u32> gamma;
};

struct ps3av_info_audio
{
	u8 type;
	u8 max_num_of_ch;
	u8 fs;
	u8 sbit;
};

#pragma pack(push, 1)
struct ps3av_info_monitor
{
	u8 avport;                       // 0
	u8 monitor_id[0xA];              // 0x1
	u8 monitor_type;                 // 0xB
	u8 monitor_name[0x10];           // 0xC
	ps3av_info_resolution res_60;    // 0x1C
	ps3av_info_resolution res_50;    // 0x24
	ps3av_info_resolution res_other; // 0x2C
	ps3av_info_resolution res_vesa;  // 0x34
	ps3av_info_cs cs;                // 0x3C
	ps3av_info_color color;          // 0x40
	u8 supported_ai;                 // 0x54
	u8 speaker_info;                 // 0x55
	u8 num_of_audio_block;           // 0x56
	//	struct ps3av_info_audio audio[0]; /* 0 or more audio blocks */
	u8 reserved[0xA9];
};
#pragma pack(pop)

static_assert(sizeof(ps3av_info_monitor) == 0x100, "ps3av_info_monitortest");

struct ps3av_evnt_plugged
{
	ps3av_header hdr;
	be_t<u32> cid;
	ps3av_info_monitor info;
};

struct ps3av_get_hw_info
{
	ps3av_header hdr;
	be_t<u32> cid;
	be_t<u32> status;
	be_t<u16> num_of_hdmi;
	be_t<u16> num_of_avmulti;
	be_t<u16> num_of_spdif;
	be_t<u16> resv;
};

struct ps3av_get_monitor_info
{
	ps3av_header hdr;
	be_t<u32> cid;
	be_t<u32> status;
	ps3av_info_monitor info;
};

error_code sys_uart_receive(ppu_thread& ppu, vm::ptr<void> buffer, u64 size, u32 unk)
{
	sys_uart.todo("sys_uart_receive(buffer=*0x%x, size=0x%llx, unk=0x%x)", buffer, size, unk);

	// blocking this for 0.85, not sure if correct for newer kernels
	u32 rtnSize = 0;
	while (!Emu.IsStopped()) {
		sys_timer_usleep(ppu, 1000);

		semaphore_lock lock(mutex);
		if (rtnSize && payloads.size() == 0)
			break;
		//if (payloads.size() == 0)
		//	return CELL_OK;
		u32 addr = buffer.addr();

		//for (const auto payload : payloads)
		while (payloads.size())
		{
			const auto payload = payloads.front();
			if (payload.data.size())
			{
				u32 cid = payload.data[0];
				switch (cid)
				{
				case PS3AV_CID_VIDEO_INIT:
				case PS3AV_CID_AUDIO_INIT:
				case PS3AV_CID_AV_GET_KSV_LIST_SIZE:
				case PS3AV_CID_VIDEO_FORMAT:
				case PS3AV_CID_VIDEO_PITCH:
				case PS3AV_CID_VIDEO_ROUTE:
				case PS3AV_CID_AV_VIDEO_DISABLE_SIG:
				case PS3AV_CID_AV_TV_MUTE:
				case PS3AV_CID_AV_UNK:
				case PS3AV_CID_AV_UNK2:
				case PS3AV_CID_VIDEO_GET_HW_CONF:
				case PS3AV_CID_AV_HDMI_MODE:
				case PS3AV_CID_AV_SET_ACP_PACKET:
				case PS3AV_CID_AV_VIDEO_UNK4:
				case PS3AV_CID_AV_ENABLE_EVENT: // this one has another u32, mask of enabled events
				case PS3AV_CID_AV_AUDIO_MUTE: // this should have another u32 for mute/unmute
				case PS3AV_CID_AUDIO_MUTE:
				case PS3AV_CID_AUDIO_MODE:
				case PS3AV_CID_AUDIO_CTRL:
				case PS3AV_CID_AUDIO_ACTIVE:
				case PS3AV_CID_AUDIO_INACTIVE:
				case PS3AV_CID_AUDIO_SPDIF_BIT:
				case PS3AV_CID_AVB_PARAM:
				case PS3AV_CID_AV_INIT: // this one has another u32 in data, event_bits or something?
				case 0xa0002:           // unk
				{
					auto out = vm::ptr<ps3av_reply_cmd_hdr>::make(addr);
					out->cid = cid | PS3AV_REPLY_BIT;
					out->length = 8;
					out->status = 0;
					out->version = payload.version;
					addr += sizeof(ps3av_reply_cmd_hdr);
					rtnSize += sizeof(ps3av_reply_cmd_hdr);
					break;
				}
				case PS3AV_CID_AV_GET_HW_CONF:
				{
					auto out = vm::ptr<ps3av_get_hw_info>::make(addr);
					out->hdr.version = payload.version;
					out->hdr.length = sizeof(ps3av_get_hw_info) - sizeof(ps3av_header);
					out->cid = cid | PS3AV_REPLY_BIT;

					out->status = 0;
					out->num_of_hdmi = 1;
					out->num_of_avmulti = 0;
					out->num_of_spdif = 1;
					addr += sizeof(ps3av_get_hw_info);
					rtnSize += sizeof(ps3av_get_hw_info);
					break;
				}
				case PS3AV_CID_AV_GET_MONITOR_INFO:
				{
					auto evnt = vm::ptr<ps3av_get_monitor_info>::make(addr);
					evnt->hdr.version = payload.version;
					evnt->hdr.length = (sizeof(ps3av_get_monitor_info) - sizeof(ps3av_header));
					evnt->cid = cid | PS3AV_REPLY_BIT;
					evnt->status = 0;

					evnt->info.avport = 0; // this looks to be hardcoded check
					//	evnt->info.monitor_id =
					evnt->info.monitor_type = PS3AV_MONITOR_TYPE_HDMI;

					//	evnt->info.monitor_name;
					evnt->info.res_60.native = PS3AV_RESBIT_1280x720P;
					evnt->info.res_60.res_bits = 0xf;
					evnt->info.res_50.native = PS3AV_RESBIT_1280x720P;
					evnt->info.res_50.res_bits = 0xf;
					evnt->info.res_vesa.res_bits = 1;

					evnt->info.cs.rgb = 1; // full rbg?
					evnt->info.cs.yuv444 = 1;
					evnt->info.cs.yuv422 = 1;

					evnt->info.color.blue_x = 0xFFFF;
					evnt->info.color.blue_y = 0xFFFF;
					evnt->info.color.green_x = 0xFFFF;
					evnt->info.color.green_y = 0xFFFF;
					evnt->info.color.red_x = 0xFFFF;
					evnt->info.color.red_y = 0xFFFF;
					evnt->info.color.white_x = 0xFFFF;
					evnt->info.color.white_x = 0xFFFF;
					evnt->info.color.gamma = 100;

					evnt->info.supported_ai = 0; // ????
					evnt->info.speaker_info = 0; // ????
					evnt->info.num_of_audio_block = 0;

					addr += sizeof(ps3av_get_monitor_info);
					rtnSize += sizeof(ps3av_get_monitor_info);
					break;
				}
				default: fmt::throw_exception("unhandled packet 0x%x", cid); break;
				}
			}
			payloads.pop_front();
		}

		//payloads.clear();
	}

	return not_an_error(rtnSize);
}

error_code sys_uart_send(vm::cptr<void> buffer, u64 size, u64 flags)
{
	sys_uart.todo("sys_uart_send(buffer=0x%x, size=0x%llx, flags=0x%x)", buffer, size, flags);
	if ((flags & ~2ull) != 0)
		return CELL_EINVAL;

	u64 counter = size;
	if (flags == 0x2)
	{
		// avset
		if (counter < 4)
			fmt::throw_exception("unhandled packet size, no header?");

		semaphore_lock lock(mutex);
		vm::ptr<u32> data;
		u32 addr = buffer.addr();
		while (counter > 0)
		{
			auto hdr = vm::ptr<ps3av_header>::make(addr);
			//if (hdr->version != PS3AV_VERSION)
			//	fmt::throw_exception("invalid ps3av_version: 0x%x", hdr->version);

			u32 length = hdr->length;
			if (length == 0)
				fmt::throw_exception("empty packet?");

			data = vm::ptr<u32>::make(addr + sizeof(ps3av_header));
			uart_payload pl;
			for (int i = 0; i < length; i += 4)
			{
				pl.data.push_back(data[i / 4]);
			}

			for (const auto& d : pl.data)
				sys_uart.error("uart: 0x%x", d);
			sys_uart.error("end");
			pl.version = hdr->version;
			payloads.emplace_back(pl);

			counter -= (length + sizeof(ps3av_header));
			addr += sizeof(ps3av_header) + length;
		}
	}
	else
	{
		// two other types, dismatch manager and system manager?
	}
	return not_an_error(size);
}

error_code sys_uart_get_params(vm::ptr<char> buffer)
{
	// buffer's size should be 0x10
	sys_uart.todo("sys_uart_get_params(buffer=0x%x)", buffer);
	return CELL_OK;
}

logs::channel sys_hid("sys_hid");

error_code sys_hid_manager_open(u64 device_type, u64 port_no, vm::ptr<u32> handle) {
	sys_hid.todo("sys_hid_manager_open(device_type=0x%llx, port_no=0x%llx, handle=*0x%llx)", device_type, port_no, handle);
	//device type == 1 = pad, 2 = kb, 3 = mouse
	if (device_type > 3)
		return CELL_EINVAL;

	// 'handle' might actually be some sort of port number, but may not actually matter for the time being
	static u32 ctr = 0x13370000;
	*handle = ctr++;

	//const auto handler = fxm::import<pad_thread>(Emu.GetCallbacks().get_pad_handler);
	cellPadInit(7);
	cellPadSetPortSetting(0, 1 | 2);

	return CELL_OK;
}

error_code sys_hid_manager_ioctl(u32 hid_handle, u32 pkg_id, vm::ptr<void> buf, u64 buf_size) {
	sys_hid.todo("sys_hid_manager_ioctl(hid_handle=0x%x, pkg_id=0x%llx, buf=*0x%x, buf_size=0x%llx)", hid_handle, pkg_id, buf, buf_size);
	if (pkg_id == 5) {
		// set sensor mode? also getinfo?
		struct sys_hid_info {
			le_t<u16> vid;
			le_t<u16> pid;
			u8 status;
			// todo: more in this, not sure what tho
		};

		auto info = vm::static_ptr_cast<sys_hid_info>(buf);
		info->vid = 0x054C;
		info->pid = 0x0268;
		info->status = 2; // apparently 2 is correct, 0/1 doesnt cause vsh to call read
		// this is probly related to usbd, where the status is 'claimed' by kernel
	}
	else if (pkg_id == 2) {
		struct sys_hid_info {
			be_t<u32> unk1;
			be_t<u32> unk2;
			be_t<u32> unk3;
			be_t<u32> unk4;
			be_t<u32> unk5;
			u8 unk6;
		};

		auto data = vm::static_ptr_cast<sys_hid_info>(buf);
		// puting all of these to 0xff reports full hid device capabilities
		// todo: figure out what each one cooresponds to
		data->unk1 = 0xFF;
		data->unk2 = 0xFF;
		data->unk3 = 0xFF;
		data->unk4 = 0xFF;
		data->unk5 = 0xFF;
		data->unk6 = 0xFF;
	}
	// pkg_id == 6 == setpressmode?
	else if (pkg_id == 0x68) {
		struct sys_hid_ioctl_68 {
			u8 unk;
			u8 unk2;
		};

		auto info = vm::static_ptr_cast<sys_hid_ioctl_68>(buf);
		//info->unk2 = 0;
	}
	return CELL_OK;
}

error_code sys_hid_manager_check_focus()
{
	return not_an_error(1);
}
error_code sys_hid_manager_514(u32 pkg_id, vm::ptr<void> buf, u64 buf_size)
{
	if (pkg_id == 0xE)
		sys_hid.trace("sys_hid_manager_514(pkg_id=0x%x, buf=*0x%x, buf_size=0x%llx)", pkg_id, buf, buf_size);
	else
		sys_hid.todo("sys_hid_manager_514(pkg_id=0x%x, buf=*0x%x, buf_size=0x%llx)", pkg_id, buf, buf_size);
	if (pkg_id == 0xE) {
		// buf holds device_type
		//auto device_type = vm::static_ptr_cast<u8>(buf);

		// return 1 or 0? look like almost like another check_focus type check, returning 0 looks to keep system focus

	}
	else if (pkg_id == 0xd) {
		//unk
		struct sys_hid_manager_514_pkg_d {
			be_t<u32> unk1;
			u8 unk2;
		};
		auto inf = vm::static_ptr_cast<sys_hid_manager_514_pkg_d>(buf);
		sys_hid.todo("unk1: 0x%x, unk2:0x%x", inf->unk1, inf->unk2);
	}
	return CELL_OK;
}

error_code sys_hid_manager_is_process_permission_root() {
	return not_an_error(1);
}

error_code sys_hid_manager_add_hot_key_observer(u32 event_queue, vm::ptr<u32> unk) {
	sys_hid.todo("sys_hid_manager_add_hot_key_observer(event_queue=0x%x, unk=*0x%x)", event_queue, unk);
	return CELL_OK;
}

error_code sys_hid_manager_read(u32 handle, u32 pkg_id, vm::ptr<void> buf, u64 buf_size) {
	if (pkg_id == 2 || pkg_id == 0x81)
		sys_hid.trace("sys_hid_manager_read(handle=0x%x, pkg_id=0x%x, buf=*0x%x, buf_size=0x%llx)", handle, pkg_id, buf, buf_size);
	else
		sys_hid.todo("sys_hid_manager_read(handle=0x%x, pkg_id=0x%x, buf=*0x%x, buf_size=0x%llx)", handle, pkg_id, buf, buf_size);
	if (pkg_id == 2) {
		// cellPadGetData
		// it returns just button array from 'CellPadData'
		//auto data = vm::static_ptr_cast<u16[64]>(buf);
		// todo: use handle and dont call cellpad here
		vm::var<CellPadData> tmpData;
		if ((cellPadGetData(0, tmpData) == CELL_OK) && tmpData->len > 0) {
			u64 cpySize = std::min((u64)tmpData->len * 2, buf_size * 2);
			memcpy(buf.get_ptr(), &tmpData->button, cpySize);
			return not_an_error(cpySize);
		}
	}
	else if (pkg_id == 0x81) {
		// cellPadGetDataExtra?
		vm::var<CellPadData> tmpData;
		cellPadGetData(0, tmpData);
		u64 cpySize = std::min((u64)tmpData->len * 2, buf_size);
		memcpy(buf.get_ptr(), &tmpData->button, cpySize);
		return not_an_error(cpySize / 2);
	}
	else if (pkg_id == 0xFF) {
		
	}
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
		if (auto q = idm::get<lv2_obj, lv2_event_queue>(packet->equeue_id))
		{
			q->send(0, 0, 0xDEAD, 0);
		}
	}
	//else
		//fmt::throw_exception("unhandled btpckt");

	return CELL_OK;
}

logs::channel sys_bdemu("sys_bdemu");
error_code sys_bdemu_send_command(u64 cmd, u64 a2, u64 a3, vm::ptr<void> buf, u64 buf_len)
{
	sys_bdemu.todo("cmd=0%x, a2=0x%x, a3=0x%x, buf=0x%x, buf_len=0x%x", cmd, a2, a3, buf, buf_len);
	// todo: only debug kernel has this
	return CELL_ENOSYS;
	/*if (cmd == 0)
	{
		auto out = vm::static_ptr_cast<u64>(buf);
		*out     = 0x101000000000008;
	}
	return CELL_OK;*/
}

struct lv2_io_buf
{
	using id_type             = lv2_io_buf;
	static const u32 id_base  = 0x44000000;
	static const u32 id_step  = 1;
	static const u32 id_count = 2048;

	const u32 block_count;
	const u32 block_size;
	const u32 blocks;
	const u32 unk1;

	lv2_io_buf(u32 block_count, u32 block_size, u32 blocks, u32 unk1)
	    : block_count(block_count)
	    , block_size(block_size)
	    , blocks(blocks)
	    , unk1(unk1)
	{
	}
};

logs::channel sys_io2("sys_io2");
error_code sys_io_buffer_create(u32 block_count, u32 block_size, u32 blocks, u32 unk1, vm::ptr<u32> handle)
{
	// blocks arg might just be alignment, or 'sector' size?
	sys_io2.todo("sys_io_buffer_create(block_count=0x%x, block_size=0x%x, blocks=0x%x, unk1=0x%x, handle=*0x%x)", block_count, block_size, blocks, unk1, handle);
	if (auto io = idm::make<lv2_io_buf>(block_count, block_size, blocks, unk1))
	{
		*handle = io;
		return CELL_OK;
	}
	return -1;
}

error_code sys_io_buffer_destroy(u32 handle)
{
	sys_io2.todo("sys_io_buffer_destroy(handle=0x%x)", handle);
	idm::remove<lv2_io_buf>(handle);
	return CELL_OK;
}

error_code sys_io_buffer_allocate(u32 handle, vm::ptr<u32> block)
{
	sys_io2.todo("sys_io_buffer_allocate(handle=0x%x, block=*0x%x)", handle, block);
	if (auto io = idm::get<lv2_io_buf>(handle))
	{
		// no idea what we actually need to allocate
		if (u32 addr = vm::alloc(io->block_count * io->block_size, vm::main))
		{
			*block = addr;
			return CELL_OK;
		}
	}
	return -1;
}

error_code sys_io_buffer_free(u32 handle, u32 block)
{
	sys_io2.todo("sys_io_buffer_free(handle=0x%x, block=0x%x)", handle, block);
	vm::dealloc(block);
	return CELL_OK;
}