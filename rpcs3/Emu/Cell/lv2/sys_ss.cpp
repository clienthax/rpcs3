#include "stdafx.h"
#include "Emu/Cell/PPUThread.h"

#include "sys_ss.h"

extern u64 get_timebased_time();

#ifdef _WIN32
#include <Windows.h>
#include <wincrypt.h>

const HCRYPTPROV s_crypto_provider = []() -> HCRYPTPROV {
	HCRYPTPROV result;

	if (!CryptAcquireContextW(&result, nullptr, nullptr, PROV_RSA_FULL, 0) && !CryptAcquireContextW(&result, nullptr, nullptr, PROV_RSA_FULL, CRYPT_NEWKEYSET))
	{
		return 0;
	}

	::atexit([]() {
		if (s_crypto_provider)
		{
			CryptReleaseContext(s_crypto_provider, 0);
		}
	});

	return result;
}();
#endif

logs::channel sys_ss("sys_ss");

error_code sys_ss_random_number_generator(u32 arg1, vm::ptr<void> buf, u64 size)
{
	sys_ss.warning("sys_ss_random_number_generator(arg1=%u, buf=*0x%x, size=0x%x)", arg1, buf, size);

	if (arg1 != 2)
	{
		return 0x80010509;
	}

	if (size > 0x10000000)
	{
		return 0x80010501;
	}

#ifdef _WIN32
	if (!s_crypto_provider || !CryptGenRandom(s_crypto_provider, size, (BYTE*)buf.get_ptr()))
	{
		return CELL_EABORT;
	}
#else
	fs::file rnd{"/dev/urandom"};

	if (!rnd || rnd.read(buf.get_ptr(), size) != size)
	{
		return CELL_EABORT;
	}
#endif

	return CELL_OK;
}

s32 sys_ss_get_console_id(vm::ptr<u8> buf)
{
	sys_ss.todo("sys_ss_get_console_id(buf=*0x%x)", buf);

	// TODO: Return some actual IDPS?
	*buf = 0;

	return CELL_OK;
}

s32 sys_ss_get_open_psid(vm::ptr<CellSsOpenPSID> psid)
{
	sys_ss.warning("sys_ss_get_open_psid(psid=*0x%x)", psid);

	psid->high = 0;
	psid->low  = 0;

	return CELL_OK;
}

s32 sys_ss_appliance_info_manager(u32 code, vm::ptr<u8> buffer)
{
	switch (code)
	{
	case 0x19002:
	{
		//Called by devtoolupdater
		//U {PPU[0x1000000] Thread (main_thread) [0x00042b88]} sys_ss TODO: sys_ss_appliance_info_manager(code=0x19002, buffer=*0xd0100b60)
		//U {PPU[0x1000000] Thread (main_thread) [0x00010c98]} sys_sm TODO: sys_console_write: buf=“Error : get product -2147290717
		sys_ss.warning("sys_ss_appliance_info_manager(code=0x%x (DEVICE_TYPE), buffer=*0x%x)", code, buffer);
		u8 idps[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x85};//http://www.psdevwiki.com/ps3/Target_ID
		memcpy(buffer.get_ptr(), idps, 16);
		break;
	}
	case 0x19003:
	{
		sys_ss.warning("sys_ss_appliance_info_manager(code=0x%x (IDPS), buffer=*0x%x)", code, buffer);
		u8 idps[] = { 0x00, 0x00, 0x00, 0x01, 0x00, 0x89, 0x00, 0x0B, 0x14, 0x00, 0xEF, 0xDD, 0xCA, 0x25, 0x52, 0x66 };
		memcpy(buffer.get_ptr(), idps, 16);
		break;
	}
	case 0x19004:
	{
		sys_ss.warning("sys_ss_appliance_info_manager(code=0x%x (PSCODE), buffer=*0x%x)", code, buffer);
		u8 pscode[] = {0x00, 0x01, 0x00, 0x85, 0x00, 0x07, 0x00, 0x04};
		memcpy(buffer.get_ptr(), pscode, 8);
		break;
	}
	default: sys_ss.todo("sys_ss_appliance_info_manager(code=0x%x, buffer=*0x%x)", code, buffer);
	}
	return CELL_OK;
}

s32 sys_ss_get_cache_of_product_mode(vm::ptr<u8> ptr)
{
	sys_ss.todo("UNIMPLEMENTED sys_ss_get_cache_of_product_mode(0x%x)", ptr);
	s32 pid = 1;

	if (false /*process == null*/)
	{
		return 0x80010003;
	}
	if (!ptr)
	{
		return 0x80010002;
	}
	// 0xff Happens when hypervisor call returns an error
	// 0 - disabled
	// 1 - enabled

	// except something segfaults when using 0, so error it is!
	*ptr = 0xFF;

	return CELL_OK;
}

// these arguments are odd and change type based on command packet
s32 sys_ss_secure_rtc(u64 cmd, u64 a2, u64 a3, u64 a4)
{
	sys_ss.todo("sys_ss_secure_rtc(cmd=0x%x, a2=0x%x, a3=0x%x, a4=0x%x)", cmd, a2, a3, a4);
	if (cmd == 0x3001)
	{
		if (a3 != 0x20)
			return 0x80010500;

		return CELL_OK;
	}
	else if (cmd == 0x3002)
	{
		// Get time
		if (a2 > 1)
			return 0x80010500;

		// a3 is actual output, not 100% sure, but best guess is its tb val
		*vm::ptr<u64>::make(a3) = get_timebased_time();
		// a4 is a pointer to status, non 0 on error
		*vm::ptr<u64>::make(a4) = 0;

		return CELL_OK;
	}
	else if (cmd == 0x3003)
	{
		return CELL_OK;
	}

	return 0x80010500;
}

s32 sys_ss_get_cache_of_flash_ext_flag(vm::ptr<u64> flag)
{
	sys_ss.todo("sys_ss_get_cache_of_flash_ext_flag(flag=*0x%x)", flag);
	*flag = 0xFE; // nand vs nor from lsb
	return CELL_OK;
}

s32 sys_ss_get_boot_device(vm::ptr<u64> dev)
{
	sys_ss.todo("sys_ss_get_boot_device(dev=*0x%x)", dev);
	*dev = 0x190;
	return CELL_OK;
}

s32 sys_ss_update_manager(u64 pkg_id, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6)
{
	sys_ss.todo("sys_ss_update_manager(pkg=0x%x, a1=0x%x, a2=0x%x, a3=0x%x, a4=0x%x, a5=0x%x, a6=0x%x)", pkg_id, a1, a2, a3, a4, a5, a6);
	if (pkg_id == 0x6003)
	{
		//Get package info
		// uint64_t Package_Type, uint64_t *out
		//sys_ss.error("Not implemented package_type=0x%x, out=*0x%x", a1, a2);

		if (a1 == 0x1)
		{
			//*vm::ptr<u64>::make(a2) = 0x0003004100000000;
			*vm::ptr<u64>::make(a2) = 0x0004008100000000;
		}
		else if (a1 == 0x2)
		{
			*vm::ptr<u64>::make(a2) = 0x0003004100000000;
		}
		else if (a1 == 0x3)
		{
			*vm::ptr<u64>::make(a2) = 0x0002003000000000;
		}
		else if (a1 == 0x4 || a1 == 0x5)
		{
			*vm::ptr<u64>::make(a2) = 0xDEADBEAFFACEBABE;
		}
		else if (a1 == 0x6)
		{
			*vm::ptr<u64>::make(a2) = 0x0003004000000000;
		}
		else
		{
			return CELL_EINVAL;
		}


	}
	else if (pkg_id == 0x600B)
	{
		//In a standard mostly untouched ps3 the common value for this flags is 0xFF wich means not active, anything else means active (e.g. 0xFE) 

		// read eeprom
		// a1 == offset
		// a2 == *value
		if (a1 == 0x48C06)
		{
			// FSELF Control Flag / toggles release mode (fself_ctrl) 
			*vm::ptr<u8>::make(a2) = 0xFF;
		}
		else if (a1 == 0x48C42)
		{
			// hddcopymode
			*vm::ptr<u8>::make(a2) = 0xFF;
		}
		else if (a1 >= 0x48C1C && a1 <= 0x48C1F)
		{
			// vsh target? (seems it can be 0xFFFFFFFE, 0xFFFFFFFF, 0x00000001 default: 0x00000000 /maybe QA,Debug,Retail,Kiosk?)
			*vm::ptr<u8>::make(a2) = a1 == 0x48C1F ? 0x1 : 0;
		}
		else if (a1 >= 0x48C18 && a1 <= 0x48C1B)
		{
			// system language
			// *i think* this gives english
			*vm::ptr<u8>::make(a2) = a1 == 0x48C1B ? 0x1 : 0;
		}
		else if (a1 == 0x48c60)
		{
			//Update status flag
			*vm::ptr<u8>::make(a2) = 0xFF;
		}
		else if (a1 == 0x48c61)
		{
			//Recovery mode flag
			*vm::ptr<u8>::make(a2) = 0xFF;
		}
		else
		{
			sys_ss.error("sys_ss_update_manager unhandled D:");
		}
		//devtool updater, U {PPU[0x1000000] Thread (main_thread) [0x00042844]} sys_ss TODO: sys_ss_update_manager(pkg=0x600b, a1=0x48c60, a2=0x1003bdf4, a3=0x0, a4=0x0, a5=0x0, a6=0x0)

		//http://www.psdevwiki.com/ps3/SC_EEPROM#EEPROM_Offset_Table_-_Flags_and_Tokens
	}
	else if (pkg_id == 0x600C)
	{
		// write eeprom
	}
	else if (pkg_id == 0x6009)
	{
		// get seed token
	}
	else if (pkg_id == 0x600A)
	{
		// set seed token
	}
	else
	{
		sys_ss.error("Not implemented");
	}

	return CELL_OK;
}

s32 sys_ss_virtual_trm_manager(u64 pkg_id, u64 a1, u64 a2, u64 a3, u64 a4) {
	sys_ss.todo("sys_ss_virtual_trm_manager(pkg=0x%x, a1=0x%llx, a2=0x%llx, a3=0x%llx, a4=0x%llx)", pkg_id, a1, a2, a3, a4);
	return CELL_OK;
}