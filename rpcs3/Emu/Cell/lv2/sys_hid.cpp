#include "stdafx.h"
#include "sys_tty.h"
#include "sys_hid.h"
#include "../Modules/cellPad.h"

#include "sys_ppu_thread.h"

logs::channel sys_hid("sys_hid");


//	sys_event.warning("sys_event_queue_create(equeue_id=*0x%x, attr=*0x%x, event_queue_key=0x%llx, size=%d)", equeue_id, attr, event_queue_key, size);

s32 _sys_config_add_service_listener(u32 conf_id, u32 b, u32 c, u32 d, u32 e, u32 f)
{
	sys_hid.error("sys_config_add_service_listener(conf_id=0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x)", conf_id, b, c, d, e, f);
	return 0;
}

s32 sys_config_open(u32 equeue_id, vm::ptr<u32> conf_id)
{
	sys_hid.error("sys_config_open(equeue_id=0x%x, conf_id=*0x%x)", equeue_id, conf_id);
	//Seems this function is meant to generate a conf id

	*conf_id = 0xDEADCAFE;
	return 0;
}

s32 sys_hid_manager_check_focus()
{
	sys_hid.todo("sys_hid_manager_check_focus()");
	//Seems to just return 0 if focused?
	return 0;
}

s32 sys_hid_manager_514(u32 port_no, vm::ptr<CellPadData> data, vm::ptr<u32> device_type)//device type might be data fetch type
{
	// Seems to be  extradata
	sys_hid.warning("sys_hid_514(port_no=%d, data=*0x%x, device_type=*0x%x)", port_no, data, device_type);
	return cellPadGetDataExtra(port_no, device_type, data);
}

