#include "stdafx.h"
#include "Emu/System.h"

#include "Emu/Cell/ErrorCodes.h"
#include "Emu/Cell/PPUThread.h"
#include "Emu/Cell/lv2/sys_process.h"

#include "sys_sm.h"

#include "Emu/system_config.h"
#include "Emu/Cell/Modules/sceNp2.h"


LOG_CHANNEL(sys_sm);

error_code sys_sm_get_params(vm::ptr<u8> a, vm::ptr<u8> b, vm::ptr<u32> c, vm::ptr<u64> d)
{
	sys_sm.todo("sys_sm_get_params(a=*0x%x, b=*0x%x, c=*0x%x, d=*0x%x)", a, b, c, d);

	if (a) *a = 0; else return CELL_EFAULT;
	if (b) *b = 0; else return CELL_EFAULT;
	if (c) *c = 0x200; else return CELL_EFAULT;
	// Bitfield
	// d & 0x2 1 = 0x0D500000 memory container size in vsh.
	if (d) *d = 7; else return CELL_EFAULT;

	return CELL_OK;
}

error_code sys_sm_get_ext_event2(vm::ptr<u64> a1, vm::ptr<u64> a2, vm::ptr<u64> a3, u64 a4)
{
	// SHUT
	//sys_sm.todo("sys_sm_get_ext_event2(a1=*0x%x, a2=*0x%x, a3=*0x%x, a4=*0x%x, a4=0x%xll", a1, a2, a3, a4);

	if (a4 != 0 && a4 != 1)
	{
		return CELL_EINVAL;
	}

	// a1 == 7 - 'console too hot, restart'
	// a2 looks to be used if a1 is either 5 or 3?
	// a3 looks to be ignored in vsh

	if (a1) *a1 = 0; else return CELL_EFAULT;
	if (a2) *a2 = 0; else return CELL_EFAULT;
	if (a3) *a3 = 0; else return CELL_EFAULT;

	// eagain for no event
	return not_an_error(CELL_EAGAIN);
}

error_code sys_sm_shutdown(ppu_thread& ppu, u16 op, vm::ptr<void> param, u64 size)
{
	ppu.state += cpu_flag::wait;

	sys_sm.success("sys_sm_shutdown(op=0x%x, param=*0x%x, size=0x%x)", op, param, size);

	if (!g_ps3_process_info.has_root_perm())
	{
		return CELL_ENOSYS;
	}

	switch (op)
	{
	case 0x100:
	case 0x1100:
	{
		sys_sm.success("Received shutdown request from application");
		_sys_process_exit(ppu, 0, 0, 0);
		break;
	}
	case 0x200:
	case 0x1200:
	{
		sys_sm.success("Received reboot request from application");
		lv2_exitspawn(ppu, Emu.argv, Emu.envp, Emu.data);
		break;
	}
	case 0x8201:
	case 0x8202:
	case 0x8204:
	{
		sys_sm.warning("Unsupported LPAR operation: 0x%x", op);
		return CELL_ENOTSUP;
	}
	default: return CELL_EINVAL;
	}

	return CELL_OK;
}

error_code sys_sm_set_shop_mode(s32 mode)
{
	sys_sm.todo("sys_sm_set_shop_mode(mode=0x%x)", mode);

	return CELL_OK;
}

error_code sys_sm_control_led(u8 led, u8 action)
{
	sys_sm.todo("sys_sm_control_led(led=0x%x, action=0x%x)", led, action);

	return CELL_OK;
}

error_code sys_sm_ring_buzzer(u64 packet, u64 a1, u64 a2)
{
	sys_sm.todo("sys_sm_ring_buzzer(packet=0x%x, a1=0x%x, a2=0x%x)", packet, a1, a2);

	return CELL_OK;
}


 error_code sys_sm_get_hw_config(vm::ptr<u8> out_res, vm::ptr<u64> out_config)
  {
        sys_sm.warning("sys_sm_get_hw_config(out_res=*0x%x, out_config=*0x%x)", out_res, out_config);

        if (!g_ps3_process_info.has_root_perm())
        {
                return CELL_ENOSYS;
        }

        if (!out_res || !out_config)
        {
                return CELL_EFAULT;
        }

        // Flat 64-bit bitfield from LV1 repository node sys/hw/config.
        // out_res 0x00 = valid; 0xFF = LV1 node unset.
        //
        // bit  0 : card reader slot 0a type selector (0=type-A, 1=type-B); USB port 1 HS chirp enable
        // bit  1 : USB port 2 high-speed chirp/reset enable
        // bit  2 : USB port 1 internally reserved (internal card reader bus)
        // bit  3 : USB port 2 internally reserved
        // bit  4 : BD-ROM requires SCSI Get Event Status Notification (0x4A) + Start/Stop Unit (0x1B) at startup
        // bit 18 : Gelic on-die WLAN enabled; gated at LV1 (calls 195/196 fail when clear) — Fat CECHA/B only
        // bit 19 : Memory Stick card reader present (exposed as boolean by sys_cardreader_561_)
        // bit 63 : extra card reader physical slot bank (registers slots 0x..0e / 0x..0f)

        u64 hw_config = 0;

        const bool is_fat_with_gelic      = false; // CECHA/B (20/60 GB launch models with on-die WLAN)
        const bool is_fat_with_cardreader = false; // CECHA/B/C/E (launch Fat models with MS slot)
		const bool messabout = true;

        if (is_fat_with_gelic)
        {
	        hw_config |= (1ULL << 18); // Gelic WLAN present; LV1 will permit calls 195/196
        }

        if (is_fat_with_cardreader)
        {
                hw_config |= (1ULL << 2);  // USB port 1 reserved for internal MS reader bus
                hw_config |= (1ULL << 19); // Memory Stick card reader present
        }

		if (messabout)
		{
			hw_config |= (1ULL << 18);
		}

        *out_res    = 0x00;
        *out_config = hw_config;

        return CELL_OK;
  }
