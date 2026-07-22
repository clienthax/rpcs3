#include "stdafx.h"
#include "sys_ss.h"

#include "sys_process.h"
#include "Emu/IdManager.h"
#include "Emu/Cell/timers.hpp"
#include "Emu/system_config.h"
#include "util/sysinfo.hpp"
#include "Crypto/aes.h"
#include "Crypto/key_vault.h"

#include <array>
#include <charconv>
#include <shared_mutex>
#include <unordered_set>

#include "Emu/System.h"

#ifdef _WIN32
#include <Windows.h>
#include <bcrypt.h>
#endif

struct lv2_update_manager
{
	lv2_update_manager()
	{
		std::string version_str = utils::get_firmware_version();

		// For example, 4.90 should be converted to 0x4900000000000
		std::erase(version_str, '.');
		if (std::from_chars(version_str.data(), version_str.data() + version_str.size(), system_sw_version, 16).ec == std::errc{})
			system_sw_version <<= 40;
		else
			system_sw_version = 0;
	}

	lv2_update_manager(const lv2_update_manager&) = delete;
	lv2_update_manager& operator=(const lv2_update_manager&) = delete;
	~lv2_update_manager() = default;

	u64 system_sw_version;

	std::unordered_map<u32, u8> eeprom_map // offset, value
	{
		// system language
		// *i think* this gives english
		{0x48C18, 0x00},
		{0x48C19, 0x00},
		{0x48C1A, 0x00},
		{0x48C1B, 0x01},
		// system language end

		// vsh target (seems it can be 0xFFFFFFFE, 0xFFFFFFFF, 0x00000001 default: 0x00000000 / vsh sets it to 0x00000000 on boot if it isn't 0x00000000)
		{0x48C1C, 0x00},
		{0x48C1D, 0x00},
		{0x48C1E, 0x00},
		{0x48C1F, 0x00}
		// vsh target end
	};
	mutable std::shared_mutex eeprom_mutex;

	std::unordered_set<u32> malloc_set;
	mutable std::shared_mutex malloc_mutex;

	// VTRM slot storage: slot index → 64-byte data blob
	std::unordered_map<u32, std::array<u8, 0x40>> vtrm_slots;
	mutable std::shared_mutex vtrm_mutex;

	// return address
	u32 allocate(u32 size)
	{
		std::unique_lock unique_lock(malloc_mutex);

		if (const auto addr = vm::alloc(size, vm::main); addr)
		{
			malloc_set.emplace(addr);
			return addr;
		}

		return 0;
	}

	// return size
	u32 deallocate(u32 addr)
	{
		std::unique_lock unique_lock(malloc_mutex);

		if (malloc_set.count(addr))
		{
			malloc_set.erase(addr);
			return vm::dealloc(addr, vm::main);
		}

		return 0;
	}
};

template<>
void fmt_class_string<sys_ss_rng_error>::format(std::string& out, u64 arg)
{
	format_enum(out, arg, [](auto error)
	{
		switch (error)
		{
		STR_CASE(SYS_SS_RNG_ERROR_INVALID_PKG);
		STR_CASE(SYS_SS_RNG_ERROR_ENOMEM);
		STR_CASE(SYS_SS_RNG_ERROR_EAGAIN);
		STR_CASE(SYS_SS_RNG_ERROR_EFAULT);
		STR_CASE(SYS_SS_RTC_ERROR_UNK);
		}

		return unknown;
	});
}

LOG_CHANNEL(sys_ss);

error_code sys_ss_random_number_generator(u64 pkg_id, vm::ptr<void> buf, u64 size)
{
	sys_ss.warning("sys_ss_random_number_generator(pkg_id=%u, buf=*0x%x, size=0x%x)", pkg_id, buf, size);

	if (pkg_id != 2)
	{
		if (pkg_id == 1)
		{
			if (!g_ps3_process_info.has_root_perm())
			{
				return CELL_ENOSYS;
			}

			sys_ss.todo("sys_ss_random_number_generator(): pkg_id=1");
			std::memset(buf.get_ptr(), 0, 0x18);
			return CELL_OK;
		}

		return SYS_SS_RNG_ERROR_INVALID_PKG;
	}

	// TODO
	if (size > 0x10000000)
	{
		return SYS_SS_RNG_ERROR_ENOMEM;
	}

	std::unique_ptr<u8[]> temp(new u8[size]);

#ifdef _WIN32
	if (auto ret = BCryptGenRandom(nullptr, temp.get(), static_cast<ULONG>(size), BCRYPT_USE_SYSTEM_PREFERRED_RNG))
	{
		fmt::throw_exception("sys_ss_random_number_generator(): BCryptGenRandom failed (0x%08x)", ret);
	}
#else
	fs::file rnd{"/dev/urandom"};

	if (!rnd || rnd.read(temp.get(), size) != size)
	{
		fmt::throw_exception("sys_ss_random_number_generator(): Failed to generate pseudo-random numbers");
	}
#endif

	std::memcpy(buf.get_ptr(), temp.get(), size);
	return CELL_OK;
}

error_code sys_ss_access_control_engine(u64 pkg_id, u64 a2, u64 a3)
{
	sys_ss.success("sys_ss_access_control_engine(pkg_id=0x%llx, a2=0x%llx, a3=0x%llx)", pkg_id, a2, a3);

	const u64 authid = g_ps3_process_info.self_info.valid ?
		g_ps3_process_info.self_info.prog_id_hdr.program_authority_id : 0;

	switch (pkg_id)
	{
	case 0x1:
	{
		if (!g_ps3_process_info.debug_or_root())
		{
			return not_an_error(CELL_ENOSYS);
		}

		if (!a2)
		{
			return CELL_ESRCH;
		}

		ensure(a2 == static_cast<u64>(process_getpid()));
		vm::write64(vm::cast(a3), authid);
		break;
	}
	case 0x2:
	{
		vm::write64(vm::cast(a2), authid);
		break;
	}
	case 0x3:
	{
		if (!g_ps3_process_info.debug_or_root())
		{
			return CELL_ENOSYS;
		}

		break;
	}
	default:
		return 0x8001051du;
	}

	return CELL_OK;
}

error_code sys_ss_get_console_id(vm::ptr<u8> buf)
{
	sys_ss.notice("sys_ss_get_console_id(buf=*0x%x)", buf);

	return sys_ss_appliance_info_manager(0x19003, buf);
}

error_code sys_ss_get_open_psid(vm::ptr<CellSsOpenPSID> psid)
{
	sys_ss.notice("sys_ss_get_open_psid(psid=*0x%x)", psid);

	const u128 configured_psid = g_cfg.sys.console_psid.get();

	psid->high = static_cast<u64>(configured_psid >> 64);
	psid->low = static_cast<u64>(configured_psid);

	return CELL_OK;
}

error_code sys_ss_appliance_info_manager(u32 code, vm::ptr<u8> buffer)
{
	sys_ss.notice("sys_ss_appliance_info_manager(code=0x%x, buffer=*0x%x)", code, buffer);

	if (!g_ps3_process_info.has_root_perm())
		return CELL_ENOSYS;

	if (!buffer)
		return CELL_EFAULT;

	switch (code)
	{
	case 0x19002:
	{
		// AIM_get_device_type
		constexpr u8 product_code[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x89 };
		std::memcpy(buffer.get_ptr(), product_code, 16);
		if (g_cfg.core.debug_console_mode)
			buffer[15] = 0x81; // DECR
		break;
	}
	case 0x19003:
	{
		// AIM_get_device_id
		constexpr u8 idps[] = { 0x00, 0x00, 0x00, 0x01, 0x00, 0x89, 0x00, 0x0B, 0x14, 0x00, 0xEF, 0xDD, 0xCA, 0x25, 0x52, 0x66 };

		std::memcpy(buffer.get_ptr(), idps, 16);
		if (g_cfg.core.debug_console_mode)
		{
			buffer[5] = 0x81; // DECR
			buffer[7] = 0x09; // DECR-1400
		}
		break;
	}
	case 0x19004:
	{
		// AIM_get_ps_code
		constexpr u8 pscode[] = { 0x00, 0x01, 0x00, 0x85 /* console type? */, 0x00, 0x07, 0x00, 0x04 };
		std::memcpy(buffer.get_ptr(), pscode, 8);
		break;
	}
	case 0x19005:
	{
		// AIM_get_open_ps_id
		const be_t<u128> psid = g_cfg.sys.console_psid.get();
		std::memcpy(buffer.get_ptr(), &psid, 16);
		break;
	}
	case 0x19006:
	{
		// qa values (dex only) ??
		[[fallthrough]];
	}
	default:
	{
		sys_ss.todo("sys_ss_appliance_info_manager(code=0x%x, buffer=*0x%x)", code, buffer);
		break;
	}
	}

	return CELL_OK;
}

error_code sys_ss_get_cache_of_product_mode(vm::ptr<u8> ptr)
{
	sys_ss.todo("sys_ss_get_cache_of_product_mode(ptr=*0x%x)", ptr);

	if (!ptr)
	{
		return CELL_EINVAL;
	}
	// 0xff Happens when hypervisor call returns an error
	// 0 - Factory/service mode.
	// 1 - DEX

	// except something segfaults when using 0, so error it is!
	// TODO really need to fix this eventually..
	*ptr = 0xff;

	return CELL_OK;
}

error_code sys_ss_secure_rtc(u64 cmd, u64 a2, u64 a3, u64 a4)
{
	sys_ss.todo("sys_ss_secure_rtc(cmd=0x%llx, a2=0x%x, a3=0x%llx, a4=0x%llx)", cmd, a2, a3, a4);
	if (cmd == 0x3001)
	{
		if (a3 != 0x20)
			return 0x80010500; // bad packet id

		return CELL_OK;
	}
	else if (cmd == 0x3002)
	{
		// Get time
		if (a2 > 1)
			return 0x80010500; // bad packet id

		// a3 is actual output, not 100% sure, but best guess is its tb val
		vm::write64(::narrow<u32>(a3), get_timebased_time());
		// a4 is a pointer to status, non 0 on error
		vm::write64(::narrow<u32>(a4), 0);
		return CELL_OK;
	}
	else if (cmd == 0x3003)
	{
		return CELL_OK;
	}

	return 0x80010500; // bad packet id
}

error_code sys_ss_get_cache_of_flash_ext_flag(vm::ptr<u64> flag)
{
	sys_ss.todo("sys_ss_get_cache_of_flash_ext_flag(flag=*0x%x)", flag);

	if (!flag)
	{
		return CELL_EFAULT;
	}

	*flag = 0xFE; // nand vs nor from lsb

	// vsh seems to check bit 0 and 1

	return CELL_OK;
}

error_code sys_ss_get_boot_device(vm::ptr<u64> dev)
{
	sys_ss.todo("sys_ss_get_boot_device(dev=*0x%x)", dev);

	if (!dev)
	{
		return CELL_EINVAL;
	}

	*dev = 0x190;

	return CELL_OK;
}

error_code sys_ss_update_manager(u64 pkg_id, u64 a1, u64 a2, u64 a3, u64 a4, u64 a5, u64 a6)
{
	sys_ss.notice("sys_ss_update_manager(pkg=0x%x, a1=0x%x, a2=0x%x, a3=0x%x, a4=0x%x, a5=0x%x, a6=0x%x)", pkg_id, a1, a2, a3, a4, a5, a6);

	if (!g_ps3_process_info.has_root_perm())
		return CELL_ENOSYS;

	auto& update_manager = g_fxo->get<lv2_update_manager>();

	switch (pkg_id)
	{
	case 0x6001:
	{
		// update package async
		break;
	}
	case 0x6002:
	{
		// inspect package async
		break;
	}
	case 0x6003:
	{
		// get installed package info
		[[maybe_unused]] const auto type = ::narrow<u32>(a1);
		const auto info_ptr = ::narrow<u32>(a2);

		if (!info_ptr)
			return CELL_EFAULT;

		vm::write64(info_ptr, update_manager.system_sw_version);

		break;
	}
	case 0x6004:
	{
		// get fix instruction
		break;
	}
	case 0x6005:
	{
		// extract package async
		break;
	}
	case 0x6006:
	{
		// get extract package
		break;
	}
	case 0x6007:
	{
		// get flash initialized
		break;
	}
	case 0x6008:
	{
		// set flash initialized
		break;
	}
	case 0x6009:
	{
		// get seed token - cellSsUmGetTokenSeed
		// Used: A1/A2/A3/A4
		// Seems like two pairs of 0x50 len buffers, buf1, len1, buf2, len2
		break;
	}
	case 0x600A:
	{
		// set seed token
		break;
	}
	case 0x600B:
	{
		// read eeprom
		const auto offset = ::narrow<u32>(a1);
		const auto value_ptr = ::narrow<u32>(a2);

		if (!value_ptr)
			return CELL_EFAULT;

		std::shared_lock shared_lock(update_manager.eeprom_mutex);

		if (const auto iterator = update_manager.eeprom_map.find(offset); iterator != update_manager.eeprom_map.end())
			vm::write8(value_ptr, iterator->second);
		else
			vm::write8(value_ptr, 0xFF); // 0xFF if not set

		break;
	}
	case 0x600C:
	{
		// write eeprom
		const auto offset = ::narrow<u32>(a1);
		const auto value = ::narrow<u8>(a2);

		std::unique_lock unique_lock(update_manager.eeprom_mutex);

		if (value != 0xFF)
			update_manager.eeprom_map[offset] = value;
		else
			update_manager.eeprom_map.erase(offset); // 0xFF: unset

		break;
	}
	case 0x600D:
	{
		// get async status
		break;
	}
	case 0x600E:
	{
		// allocate buffer
		const auto size = ::narrow<u32>(a1);
		const auto addr_ptr = ::narrow<u32>(a2);

		if (!addr_ptr)
			return CELL_EFAULT;

		const auto addr = update_manager.allocate(size);

		if (!addr)
			return CELL_ENOMEM;

		vm::write32(addr_ptr, addr);

		break;
	}
	case 0x600F:
	{
		// release buffer
		const auto addr = ::narrow<u32>(a1);

		if (!update_manager.deallocate(addr))
			return CELL_ENOMEM;

		break;
	}
	case 0x6010:
	{
		// check integrity
		break;
	}
	case 0x6011:
	{
		// get applicable version
		const auto addr_ptr = ::narrow<u32>(a2);

		if (!addr_ptr)
			return CELL_EFAULT;

		vm::write64(addr_ptr, 0x30040ULL << 32); // 3.40

		break;
	}
	case 0x6012:
	{
		// allocate buffer from memory container
		[[maybe_unused]] const auto mem_ct = ::narrow<u32>(a1);
		const auto size = ::narrow<u32>(a2);
		const auto addr_ptr = ::narrow<u32>(a3);

		if (!addr_ptr)
			return CELL_EFAULT;

		const auto addr = update_manager.allocate(size);

		if (!addr)
			return CELL_ENOMEM;

		vm::write32(addr_ptr, addr);

		break;
	}
	case 0x6013:
	{
		// unknown
		break;
	}
	default:
	{
		sys_ss.error("sys_ss_update_manager(): invalid packet id 0x%x ", pkg_id);
		return CELL_EINVAL;
	}
	}

	return CELL_OK;
}

error_code sys_ss_virtual_trm_manager(u64 cmd, u64 a1, u64 a2, u64 a3, u64 a4)
{
	sys_ss.todo("sys_ss_virtual_trm_manager(cmd=0x%llx, a1=0x%llx, a2=0x%llx, a3=0x%llx, a4=0x%llx)", cmd, a1, a2, a3, a4);

	auto& update_manager = g_fxo->get<lv2_update_manager>();

	// Derive 16-byte AES-128 key: base_key XOR (laid[8] || paid[8])  big-endian
	const auto sc_derive_key = [](const u8* base_key, u64 laid, u64 paid, u8* out_key)
	{
		for (int i = 0; i < 8; i++)
		{
			out_key[i]     = base_key[i]     ^ static_cast<u8>(laid >> (56 - 8 * i));
			out_key[8 + i] = base_key[8 + i] ^ static_cast<u8>(paid >> (56 - 8 * i));
		}
	};

	// Map cipher context 0-3 to (laid, paid) for non-portable VTRM ops
	// matches vtrm_get_laid_paid_from_type() in sc_crypto_all-1.py
	const auto vtrm_context_to_laid_paid = [](u32 ctx, u64& laid, u64& paid)
	{
		switch (ctx & 3)
		{
		case 0: laid = 0xFFFFFFFFFFFFFFFFULL; paid = 0xFFFFFFFFFFFFFFFFULL; break;
		case 1: laid = 0x1070000002000001ULL; paid = 0x1070000000000001ULL; break;
		case 2: laid = 0x1070000002000001ULL; paid = 0x0000000000000000ULL; break;
		case 3: laid = 0x1070000002000001ULL; paid = 0x10700003FF000001ULL; break;
		}
	};

	// AES-128-CBC encrypt or decrypt; iv is consumed (CBC state)
	const auto vtrm_aes_cbc = [](bool encrypt, const u8* key, const u8* iv_in, const u8* input, u8* output, u32 size) -> bool
	{
		aes_context aes{};
		u8 iv[16];
		std::memcpy(iv, iv_in, 16);
		if (encrypt)
		{
			aes_setkey_enc(&aes, key, 128);
			return aes_crypt_cbc(&aes, AES_ENCRYPT, size, iv, input, output) == 0;
		}
		else
		{
			aes_setkey_dec(&aes, key, 128);
			return aes_crypt_cbc(&aes, AES_DECRYPT, size, iv, input, output) == 0;
		}
	};

	// Shared body for ops 0x200A / 0x200B (standard) and 0x200C / 0x200D (portable)
	// r4=context, r5=key/IV ptr (16 bytes), r6=data ptr (64 bytes, in/out)
	const auto do_cipher_op = [&](bool encrypt, bool portable) -> error_code
	{
		const u32 context   = static_cast<u32>(a1);
		const u32 key_addr  = ::narrow<u32>(a2); // IV
		const u32 data_addr = ::narrow<u32>(a3); // plaintext/ciphertext

		if (!key_addr || !data_addr)
			return CELL_EFAULT;

		const u8* base_key;
		u64 laid, paid;

		if (!portable)
		{
			// Standard: sc_type 3 (SC_ISO_SERIES_KEY_2) XOR vtrm_laid_paid(context)
			base_key = SC_ISO_SERIES_KEY_2;
			vtrm_context_to_laid_paid(context, laid, paid);
		}
		else
		{
			// Portable: vtrm_portability_type_mapper maps context → sc_type
			// laid_paid is (0,0) per reference implementation
			static constexpr u32 port_map[4] = {1, 3, 2, 5};
			const u32 sc_type = port_map[context & 3];
			laid = 0;
			paid = 0;
			switch (sc_type)
			{
			case 2: base_key = SC_ISO_SERIES_KEY_1; break;
			case 3: base_key = SC_ISO_SERIES_KEY_2; break;
			default:
				sys_ss.todo("sys_ss_virtual_trm_manager: portability sc_type %u not implemented", sc_type);
				return CELL_OK;
			}
		}

		u8 derived_key[16], iv[16], data[0x40], result[0x40];
		sc_derive_key(base_key, laid, paid, derived_key);
		std::memcpy(iv,   vm::_ptr<u8>(key_addr),  16);
		std::memcpy(data, vm::_ptr<u8>(data_addr), 0x40);

		if (!vtrm_aes_cbc(encrypt, derived_key, iv, data, result, 0x40))
			return CELL_EINVAL;

		std::memcpy(vm::_ptr<u8>(data_addr), result, 0x40);
		return CELL_OK;
	};

	switch (cmd)
	{
	case 0x2001:
		// Init — no-op; real VTRM initialises its internal state
		break;

	case 0x2002:
	{
		// Status — return (0, 0, 0) to indicate VTRM ready
		const u32 sa = ::narrow<u32>(a1);
		const u32 sb = ::narrow<u32>(a2);
		const u32 sc = ::narrow<u32>(a3);
		if (!sa || !sb || !sc)
			return CELL_EFAULT;
		vm::write32(sa, 0);
		vm::write32(sb, 0);
		vm::write32(sc, 0);
		break;
	}

	case 0x2003:
	{
		// Store with TRM Update — writes 64-byte blob to slot 0
		const u32 data_addr = ::narrow<u32>(a1);
		if (!data_addr)
			return CELL_EFAULT;
		std::lock_guard lock(update_manager.vtrm_mutex);
		std::memcpy(update_manager.vtrm_slots[0].data(), vm::_ptr<u8>(data_addr), 0x40);
		break;
	}

	case 0x2004:
	{
		// Store — writes 64-byte blob to slot nth (r5)
		const u32 data_addr = ::narrow<u32>(a1);
		const u32 nth       = static_cast<u32>(a2);
		if (!data_addr)
			return CELL_EFAULT;
		std::lock_guard lock(update_manager.vtrm_mutex);
		std::memcpy(update_manager.vtrm_slots[nth].data(), vm::_ptr<u8>(data_addr), 0x40);
		break;
	}

	case 0x2005:
	{
		// Retrieve — reads 64-byte blob from slot nth (r5) into r4
		const u32 data_addr = ::narrow<u32>(a1);
		const u32 nth       = static_cast<u32>(a2);
		if (!data_addr)
			return CELL_EFAULT;
		std::shared_lock lock(update_manager.vtrm_mutex);
		const auto it = update_manager.vtrm_slots.find(nth);
		if (it == update_manager.vtrm_slots.end())
		{
			// Slot never stored — zero out caller's buffer and return VTRM error
			std::memset(vm::_ptr<u8>(data_addr), 0, 0x40);
			// return 0x80010501; // VTRM_SLOT_EMPTY (arbitrary, maps to 0x80010500 | 1)
			return CELL_OK; // IDK MAN
		}
		std::memcpy(vm::_ptr<u8>(data_addr), it->second.data(), 0x40);
		break;
	}

	case 0x2006:
	{
		// Free — removes slot nth (r4) from storage
		const u32 nth = static_cast<u32>(a1);
		std::lock_guard lock(update_manager.vtrm_mutex);
		update_manager.vtrm_slots.erase(nth);
		break;
	}

	// 0x2007–0x2009 not implemented in lv2 (fall through to default → 0x8001051d)

	case 0x200A: return do_cipher_op(true,  false); // Encrypt
	case 0x200B: return do_cipher_op(false, false); // Decrypt
	case 0x200C: return do_cipher_op(true,  true);  // Encrypt Portable
	case 0x200D: return do_cipher_op(false, true);  // Decrypt Portable

		/*
	case 0x200E:
	{
		// Decrypt Master
		// r4 = key/IV ptr (16 bytes), r5 = data ptr (64 bytes, ciphertext→plaintext in place)
		// Key is derived from the caller's LAID/PAID and one of three master key variants.
		const u32 key_addr  = ::narrow<u32>(a1); // IV
		const u32 data_addr = ::narrow<u32>(a2); // ciphertext / plaintext out

		if (!key_addr || !data_addr)
			return CELL_EFAULT;

		// Caller's PAID from SELF header; LAID is LAID_2 for GameOS/PS3_LPAR processes
		const u64 paid = g_ps3_process_info.self_info.valid ?
			g_ps3_process_info.self_info.prog_id_hdr.program_authority_id : 0ULL;
		const u64 laid = 0x1070000002000001ULL; // LAID_2

		u8 iv[16], ciphertext[0x40], plaintext[0x40], derived_key[16];
		std::memcpy(iv,         vm::_ptr<u8>(key_addr),  16);
		std::memcpy(ciphertext, vm::_ptr<u8>(data_addr), 0x40);

		// Three master key variants: sc_type 4.0 (SC_ISO_SERIES_INTERNAL_KEY_3, fw < 3.10),
		// 4.1 (SC_KEY_FOR_MASTER_1, fw 3.10–3.55), 4.2 (SC_KEY_FOR_MASTER_2, fw ≥ 3.56).
		// Real VTRM picks the variant recorded in EEPROM; default to the most common (4.2).
		sc_derive_key(SC_KEY_FOR_MASTER_2, laid, paid, derived_key);
		if (!vtrm_aes_cbc(false, derived_key, iv, ciphertext, plaintext, 0x40))
			return CELL_EINVAL;

		for (size_t i = 0; i < 0x40; i += 16)
		{
			sys_ss.todo(
				"%02x %02x %02x %02x %02x %02x %02x %02x "
				"%02x %02x %02x %02x %02x %02x %02x %02x",
				plaintext[i+0],  plaintext[i+1],  plaintext[i+2],  plaintext[i+3],
				plaintext[i+4],  plaintext[i+5],  plaintext[i+6],  plaintext[i+7],
				plaintext[i+8],  plaintext[i+9],  plaintext[i+10], plaintext[i+11],
				plaintext[i+12], plaintext[i+13], plaintext[i+14], plaintext[i+15]);
		}

		std::memcpy(vm::_ptr<u8>(data_addr), plaintext, 0x40);
		break;
	}*/


case 0x200E:
{
    // Decrypt Master
    // r4 = key/IV ptr (16 bytes), r5 = data ptr (64 bytes, ciphertext→plaintext in place)
    // Key is derived from the caller's LAID/PAID and one of three master key variants.
    const u32 key_addr  = ::narrow<u32>(a1); // IV
    const u32 data_addr = ::narrow<u32>(a2); // ciphertext / plaintext out

    if (!key_addr || !data_addr)
       return CELL_EFAULT;

    // Caller's PAID from SELF header; LAID is LAID_2 for GameOS/PS3_LPAR processes
    const u64 paid = g_ps3_process_info.self_info.valid ?
       g_ps3_process_info.self_info.prog_id_hdr.program_authority_id : 0ULL;
    const u64 laid = 0x1070000002000001ULL; // LAID_2

    u8 iv[16], ciphertext[0x40], plaintext[0x40], derived_key[16];
    std::memcpy(iv,         vm::_ptr<u8>(key_addr),  16);
    std::memcpy(ciphertext, vm::_ptr<u8>(data_addr), 0x40);

    /* ===== DEBUG: Input Parameters ===== */
    sys_ss.todo("[VTRM DECRYPT MASTER] Syscall 0x200E invoked");
    sys_ss.todo("[VTRM] IV address (key_addr):     0x%08x", key_addr);
    sys_ss.todo("[VTRM] Data address (data_addr):  0x%08x", data_addr);
    sys_ss.todo("[VTRM] PAID (Program Authority ID): 0x%016llx", paid);
    sys_ss.todo("[VTRM] LAID (License Authority ID): 0x%016llx", laid);

    /* ===== DEBUG: IV Buffer (16 bytes) ===== */
    sys_ss.todo("[VTRM] IV (Initialization Vector) [16 bytes]:");
    sys_ss.todo(
       "  %02x %02x %02x %02x %02x %02x %02x %02x "
       "%02x %02x %02x %02x %02x %02x %02x %02x",
       iv[0],  iv[1],  iv[2],  iv[3],
       iv[4],  iv[5],  iv[6],  iv[7],
       iv[8],  iv[9],  iv[10], iv[11],
       iv[12], iv[13], iv[14], iv[15]);

    /* ===== DEBUG: Ciphertext Buffer (64 bytes, PRE-DECRYPTION) ===== */
    sys_ss.todo("[VTRM] CIPHERTEXT (PRE-DECRYPTION) [64 bytes]:");
    for (size_t i = 0; i < 0x40; i += 16)
    {
       sys_ss.todo(
          "  [+%02zx] %02x %02x %02x %02x %02x %02x %02x %02x "
          "%02x %02x %02x %02x %02x %02x %02x %02x",
          i,
          ciphertext[i+0],  ciphertext[i+1],  ciphertext[i+2],  ciphertext[i+3],
          ciphertext[i+4],  ciphertext[i+5],  ciphertext[i+6],  ciphertext[i+7],
          ciphertext[i+8],  ciphertext[i+9],  ciphertext[i+10], ciphertext[i+11],
          ciphertext[i+12], ciphertext[i+13], ciphertext[i+14], ciphertext[i+15]);
    }

    // Three master key variants: sc_type 4.0 (SC_ISO_SERIES_INTERNAL_KEY_3, fw < 3.10),
    // 4.1 (SC_KEY_FOR_MASTER_1, fw 3.10–3.55), 4.2 (SC_KEY_FOR_MASTER_2, fw ≥ 3.56).
    // Real VTRM picks the variant recorded in EEPROM; default to the most common (4.2).
    sys_ss.todo("[VTRM] Master key derivation:");
    sys_ss.todo("[VTRM]   Variant: SC_KEY_FOR_MASTER_2 (firmware >= 3.56)");
    sys_ss.todo("[VTRM]   Input LAID: 0x%016llx", laid);
    sys_ss.todo("[VTRM]   Input PAID: 0x%016llx", paid);

    sc_derive_key(SC_KEY_FOR_MASTER_2, laid, paid, derived_key);

    /* ===== DEBUG: Derived Key (16 bytes, AES-128) ===== */
    sys_ss.todo("[VTRM] DERIVED KEY (AES-128) [16 bytes]:");
    sys_ss.todo(
       "  %02x %02x %02x %02x %02x %02x %02x %02x "
       "%02x %02x %02x %02x %02x %02x %02x %02x",
       derived_key[0],  derived_key[1],  derived_key[2],  derived_key[3],
       derived_key[4],  derived_key[5],  derived_key[6],  derived_key[7],
       derived_key[8],  derived_key[9],  derived_key[10], derived_key[11],
       derived_key[12], derived_key[13], derived_key[14], derived_key[15]);

    /* ===== DEBUG: Decryption Operation ===== */
    sys_ss.todo("[VTRM] Performing AES-128-CBC decryption...");
    sys_ss.todo("[VTRM]   Mode:       AES-CBC");
    sys_ss.todo("[VTRM]   Key size:   128 bits (16 bytes)");
    sys_ss.todo("[VTRM]   IV size:    128 bits (16 bytes)");
    sys_ss.todo("[VTRM]   Data size:  0x40 bytes (64 bytes, 4 blocks)");
    sys_ss.todo("[VTRM]   Direction:  Decrypt (false)");

    if (!vtrm_aes_cbc(false, derived_key, iv, ciphertext, plaintext, 0x40))
    {
       sys_ss.error("[VTRM] ✗ AES-CBC DECRYPTION FAILED!");
       sys_ss.error("[VTRM] Possible causes:");
       sys_ss.error("[VTRM]   - Invalid derived key");
       sys_ss.error("[VTRM]   - Corrupted ciphertext");
       sys_ss.error("[VTRM]   - Wrong IV");
       sys_ss.error("[VTRM]   - Hardware error");
       return CELL_EINVAL;
    }

    sys_ss.todo("[VTRM] ✓ AES-CBC decryption completed successfully");

    /* ===== DEBUG: Plaintext Buffer (64 bytes, POST-DECRYPTION) ===== */
    sys_ss.todo("[VTRM] PLAINTEXT (POST-DECRYPTION) [64 bytes]:");
    for (size_t i = 0; i < 0x40; i += 16)
    {
       sys_ss.todo(
          "  [+%02zx] %02x %02x %02x %02x %02x %02x %02x %02x "
          "%02x %02x %02x %02x %02x %02x %02x %02x",
          i,
          plaintext[i+0],  plaintext[i+1],  plaintext[i+2],  plaintext[i+3],
          plaintext[i+4],  plaintext[i+5],  plaintext[i+6],  plaintext[i+7],
          plaintext[i+8],  plaintext[i+9],  plaintext[i+10], plaintext[i+11],
          plaintext[i+12], plaintext[i+13], plaintext[i+14], plaintext[i+15]);
    }

    /* ===== DEBUG: Plaintext Analysis & Marker Detection ===== */
    sys_ss.todo("[VTRM] Plaintext structure analysis:");

    // First 4 bytes often contain key variant marker
    sys_ss.todo("[VTRM]   Bytes [0-3]:  %02x %02x %02x %02x (key marker candidate)",
       plaintext[0], plaintext[1], plaintext[2], plaintext[3]);

    // Check for known PSN decryption key marker "b7fe802b" (4 bytes)
    if (plaintext[0] == 0xb7 && plaintext[1] == 0xfe &&
        plaintext[2] == 0x80 && plaintext[3] == 0x2b)
    {
       sys_ss.todo("[VTRM] ✓✓✓ PLAINTEXT MARKER DETECTED: b7fe802b");
       sys_ss.todo("[VTRM] This is a valid PSN/platform key variant!");
    }
    else
    {
       sys_ss.todo("[VTRM] ⚠ Plaintext marker MISMATCH");
       sys_ss.todo("[VTRM]   Expected: b7 fe 80 2b");
       sys_ss.todo("[VTRM]   Got:      %02x %02x %02x %02x",
          plaintext[0], plaintext[1], plaintext[2], plaintext[3]);
       sys_ss.todo("[VTRM] This may be a different key variant or corrupted data");
    }

    // Second part (bytes 8-23) often contains the actual credential/passphrase
    sys_ss.todo("[VTRM]   Bytes [8-23]: (credential/passphrase section)");
    sys_ss.todo(
       "    %02x %02x %02x %02x %02x %02x %02x %02x "
       "%02x %02x %02x %02x %02x %02x %02x %02x",
       plaintext[8],  plaintext[9],  plaintext[10], plaintext[11],
       plaintext[12], plaintext[13], plaintext[14], plaintext[15],
       plaintext[16], plaintext[17], plaintext[18], plaintext[19],
       plaintext[20], plaintext[21], plaintext[22], plaintext[23]);

    // Remaining bytes
    sys_ss.todo("[VTRM]   Bytes [24-63]: (padding/additional data)");
    for (size_t i = 24; i < 0x40; i += 16)
    {
       if (i + 16 <= 0x40)
       {
          sys_ss.todo(
             "    %02x %02x %02x %02x %02x %02x %02x %02x "
             "%02x %02x %02x %02x %02x %02x %02x %02x",
             plaintext[i+0],  plaintext[i+1],  plaintext[i+2],  plaintext[i+3],
             plaintext[i+4],  plaintext[i+5],  plaintext[i+6],  plaintext[i+7],
             plaintext[i+8],  plaintext[i+9],  plaintext[i+10], plaintext[i+11],
             plaintext[i+12], plaintext[i+13], plaintext[i+14], plaintext[i+15]);
       }
    }

    // Check for null padding (common in PKCS#7 or custom padding)
    size_t non_null_bytes = 0;
    for (size_t i = 0; i < 0x40; i++)
    {
       if (plaintext[i] != 0x00)
          non_null_bytes++;
    }
    sys_ss.todo("[VTRM]   Non-null bytes: %zu / 64 (%.1f%% data)", non_null_bytes, (non_null_bytes / 64.0) * 100.0);

    /* ===== DEBUG: Write-back to VM memory ===== */
    sys_ss.todo("[VTRM] Writing plaintext back to VM memory...");
    sys_ss.todo("[VTRM]   Destination: 0x%08x", data_addr);
    sys_ss.todo("[VTRM]   Size:        0x40 bytes (64 bytes)");

    std::memcpy(vm::_ptr<u8>(data_addr), plaintext, 0x40);

    sys_ss.todo("[VTRM] Write-back complete");

    /* ===== DEBUG: Memory Verification ===== */
    // Read back and verify
    u8 verify_buffer[0x40];
    std::memcpy(verify_buffer, vm::_ptr<u8>(data_addr), 0x40);
    bool verify_ok = std::memcmp(verify_buffer, plaintext, 0x40) == 0;

    if (verify_ok)
    {
       sys_ss.todo("[VTRM] ✓ Memory verification passed (write-back confirmed)");
    }
    else
    {
       sys_ss.todo("[VTRM] ⚠ Memory verification FAILED");
       sys_ss.todo("[VTRM] Data mismatch detected after write-back!");
    }

    /* ===== DEBUG: Summary ===== */
    sys_ss.todo("[VTRM DECRYPT MASTER] ════════════════════════════════════════");
    sys_ss.todo("[VTRM] Operation completed successfully");
    sys_ss.todo("[VTRM] Input:  64-byte ciphertext + 16-byte IV");
    sys_ss.todo("[VTRM] Key:    Derived from LAID/PAID using SC_KEY_FOR_MASTER_2");
    sys_ss.todo("[VTRM] Output: 64-byte plaintext → 0x%08x", data_addr);
    sys_ss.todo("[VTRM] Status: %s", verify_ok ? "VERIFIED ✓" : "UNVERIFIED ⚠");
    sys_ss.todo("[VTRM] ════════════════════════════════════════════════════════");

    break;
}

	// 0x200F–0x2011 not implemented in lv2 (fall through to default → 0x8001051d)

	case 0x2012:
		// Backup Flash — reads VTRM EEPROM region into caller buffer; requires product mode flag
		sys_ss.todo("sys_ss_virtual_trm_manager(cmd=BACKUP_FLASH, pos=0x%llx, size=0x%llx, buf=0x%llx, nread=0x%llx)", a1, a2, a3, a4);
		break;

	case 0x2013:
		// Restore Flash — writes caller buffer into VTRM EEPROM region; requires product mode flag
		sys_ss.todo("sys_ss_virtual_trm_manager(cmd=RESTORE_FLASH, pos=0x%llx, size=0x%llx, buf=0x%llx, nwritten=0x%llx)", a1, a2, a3, a4);
		break;

	case 0x2014:
		// Backup SRK/SRH — copies 128-byte SRK/SRH from VTRM into caller buffer
		sys_ss.todo("sys_ss_virtual_trm_manager(cmd=BACKUP_SRK_SRH, size=0x%llx, buf=0x%llx)", a1, a2);
		break;

	case 0x2015:
		// Restore SRK/SRH — writes caller's 128 bytes back to VTRM
		sys_ss.todo("sys_ss_virtual_trm_manager(cmd=RESTORE_SRK_SRH, size=0x%llx, buf=0x%llx)", a1, a2);
		break;

	case 0x2016:
		// Flash Info — returns VTRM flash base address and size to caller
		sys_ss.todo("sys_ss_virtual_trm_manager(cmd=FLASH_INFO, addr_out=0x%llx, size_out=0x%llx)", a1, a2);
		break;

	case 0x2017:
		// Force Restart — tells VTRM to restart its firmware
		sys_ss.todo("sys_ss_virtual_trm_manager(cmd=FORCE_RESTART)");
		break;

	default:
		return 0x8001051d; // unknown op — no IPC sent
	}

	return CELL_OK;
}

error_code sys_ss_individual_info_manager(u64 pkg_id, u64 a2, vm::ptr<u64> out_size, u64 a4, u64 a5, u64 a6)
{
	sys_ss.todo("sys_ss_individual_info_manager(pkg=0x%llx, a2=0x%llx, out_size=*0x%llx, a4=0x%llx, a5=0x%llx, a6=0x%llx)", pkg_id, a2, out_size, a4, a5, a6);
	sys_ss.todo("FUCK");

	switch (pkg_id)
	{
	// Read EID
	case 0x17002:
	{
		// TODO
		vm::write<u64>(static_cast<u32>(a5), a4); // Write back size of buffer
		break;
	}
	// Get EID size
	case 0x17001: *out_size = 0x100; break;
	default: break;
	}

	return CELL_OK;
}

// storage_manager_if
error_code sys_ss_sec_hw_framework(u32 packet_id, vm::ptr<void> buf)
{
	sys_ss.todo("sys_ss_sec_hw_framework(packet_id=0x%llx, buf=*0x%x)", packet_id, buf);

	switch (packet_id)
	{
	case 0x5004:
		// Authenticate BD Drive (cellSsDrvAuthDrive)
		break;

	case 0x5007:
		// Authenticate PS3 Game (cellSsDrvAuthDiscPs3)
		break;

	case 0x5008:
		// HW mc
		break;

	case 0x5011:
		// Retrieve M1m for bdv (Bluray Disc Voucher)
		break;

	case 0x5012:
		// Retrieve "X-I-5-Passphrase" NPpp (Network Product passphrase)
		// Copy 0x10 from user
		// TODO, will cause pain and suffering
		// Need to load the isolated spu from firmware and use it for this.. *Screams*
		break;

	default:
		return 0x8001051d;
	}

	return CELL_OK;
}