#pragma once

#include "../Modules/cellPad.h"

s32 _sys_config_add_service_listener(u32 a, u32 b, u32 c, u32 d, u32 e, u32 f);
s32 sys_config_open(u32 equeue_id, vm::ptr<u32> conf_id);
s32 sys_hid_manager_check_focus();
s32 sys_hid_manager_514(u32 port_no, vm::ptr<CellPadData> data, vm::ptr<u32> device_type);