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

s32 sys_storage_open(u64 device, u32 mode, vm::ptr<u32> fd, u32 flags)
{
	sys_storage.todo("sys_storage_open(device=0x%x, mode=0x%x, fd=*0x%x, 0x%x)", device, mode, fd, flags);
	*fd = 0xdadad0d0; // Poison value
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

s32 sys_storage_get_device_info(u64 device, vm::ptr<u8> buffer)
{
	sys_storage.todo("sys_storage_get_device_info(device=0x%x, config=0x%x)", device, buffer);
	//*reinterpret_cast<be_t<u64>*>(buffer.get_ptr() + 0x28) = 0x0909090909090909; // sector count?
	*reinterpret_cast<be_t<u32>*>(buffer.get_ptr() + 0x30) = 0x1; // Functions tries to allocate (this size + 0xfffffffff) >> 20
	buffer[0x35]                                           = 1;   // writable?
	buffer[0x3f]                                           = 1;
	buffer[0x39]                                           = 1;
	buffer[0x3a]                                           = 1;
	return CELL_OK;
}

s32 sys_storage_get_device_config(vm::ptr<u32> storages, vm::ptr<u32> devices)
{
	sys_storage.todo("sys_storage_get_device_config(storages=*0x%x, devices=*0x%x)", storages, devices);

	*storages = 6;
	*devices = 17;

	return CELL_OK;
}

s32 sys_storage_report_devices(u32 storages, u32 zero, u32 devices, vm::ptr<u64> device_ids)
{
	sys_storage.todo("sys_storage_report_devices(storages=0x%x, zero=0x%x, devices=0x%x, device_ids=0x%x)", storages, zero, devices, device_ids);
	*device_ids = 0x101000000000007;
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