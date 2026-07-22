#include "stdafx.h"

#include "Emu/Cell/ErrorCodes.h"

#include "sys_crypto_engine.h"

LOG_CHANNEL(sys_crypto_engine);

error_code sys_crypto_engine_create(vm::ptr<u32> id)
{
	sys_crypto_engine.todo("sys_crypto_engine_create(id=*0x%x)", id);

	return CELL_OK;
}

error_code sys_crypto_engine_destroy(u32 id)
{
	sys_crypto_engine.todo("sys_crypto_engine_destroy(id=0x%x)", id);

	return CELL_OK;
}

error_code sys_crypto_engine_random_generate(vm::ptr<void> buffer, u64 buffer_size)
{
	sys_crypto_engine.todo("sys_crypto_engine_random_generate(buffer=*0x%x, buffer_size=0x%x", buffer, buffer_size);

	if (buffer_size < 16)
	{
		return CELL_EINVAL;
	}


	/*error_code rng_status = get_pseudo_random_number(random_bytes, sizeof(random_bytes));
	if (rng_status == 0) {
		return copy_to_user(random_bytes, buffer, sizeof(random_bytes));
	}*/

	/* Translate internal RNG error codes to standard errno values */
	/*switch (rng_status) {
		case 1:    return ENOMEM;
		case 3:    return EBUSY;
		case 9:    return EFAULT;
		case 0xF:  ;   // unrecoverable — does not return
		default:   return rng_status;
	}*/

	return CELL_OK;
}
