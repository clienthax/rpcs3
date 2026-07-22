#include "stdafx.h"
#include "sceNpInstaller.h"

#include "Emu/IdManager.h"
#include "Emu/Cell/PPUModule.h"

LOG_CHANNEL(sceNpInstaller);


template <>
void fmt_class_string<SceNpInstallerError>::format(std::string& out, u64 arg)
{
	format_enum(out, arg, [](auto error) {
		switch (error)
		{
			STR_CASE(SCE_NP_INSTALLER_ERROR_INVALID_ARGUMENT);
			STR_CASE(SCE_NP_INSTALLER_ERROR_INVALID_DATA);
			STR_CASE(SCE_NP_INSTALLER_ERROR_BUSY_OR_IN_PROGRESS);
			STR_CASE(SCE_NP_INSTALLER_ERROR_NOT_INITIALIZED);
			STR_CASE(SCE_NP_INSTALLER_ERROR_ALREADY_INITIALIZED);
		}

		return unknown;
	});
}

error_code sceNpInstallerInit()
{
	sceNpInstaller.todo("sceNpInstallerInit()");

	auto& manager = g_fxo->get<sce_np_installer_manager>();

	if (manager.is_initialized)
	{
		return SCE_NP_INSTALLER_ERROR_ALREADY_INITIALIZED;
	}

	manager.is_initialized = true;
	return CELL_OK;
}

error_code sceNpInstallerTerm()
{
	sceNpInstaller.todo("sceNpInstallerTerm()");
	auto& manager = g_fxo->get<sce_np_installer_manager>();
	manager.is_initialized = false;
	return CELL_OK;
}

error_code sceNpInstallerGetNpEnv(vm::ptr<char> env)
{
	sceNpInstaller.todo("sceNpInstallerGetNpEnv(env=*0x%x)", env);

	if (!g_fxo->get<sce_np_installer_manager>().is_initialized)
	{
		return SCE_NP_INSTALLER_ERROR_NOT_INITIALIZED;
	}

	if (!env)
	{
		return SCE_NP_INSTALLER_ERROR_INVALID_ARGUMENT;
	}

	std::memcpy(env.get_ptr(), "np", 3);
	return CELL_OK;
}

error_code sceNpInstallerGetGuestSignIn(vm::ptr<sce_np_guest_acc_info> guest_acc_info)
{
	sceNpInstaller.todo("sceNpInstallerGetGuestSignIn(guest_acc_info=*0x%x)", guest_acc_info);

	if (!g_fxo->get<sce_np_installer_manager>().is_initialized)
	{
		return SCE_NP_INSTALLER_ERROR_NOT_INITIALIZED;
	}

	if (!guest_acc_info)
	{
		return SCE_NP_INSTALLER_ERROR_INVALID_ARGUMENT;
	}

	// TODO confirm
	memccpy(guest_acc_info->country, "us", 3, 3);
	memccpy(guest_acc_info->language, "english", 8, 8);
	guest_acc_info->birthDate.day = 1;
	guest_acc_info->birthDate.month = 1;
	guest_acc_info->birthDate.year = 1990;

	return CELL_OK;
}

error_code sceNpInstallerGetFacebookParam(vm::ptr<void> param)
{
	sceNpInstaller.todo("sceNpInstallerGetFacebookParam(param=*0x%x)");

	if (!g_fxo->get<sce_np_installer_manager>().is_initialized)
	{
		return SCE_NP_INSTALLER_ERROR_NOT_INITIALIZED;
	}

	if (!param)
	{
		return SCE_NP_INSTALLER_ERROR_INVALID_ARGUMENT;
	}

	// TODO
	return not_an_error(-1);
}

error_code sceNpInstallerGetFacebookParam2(vm::ptr<void> param)
{
	sceNpInstaller.todo("sceNpInstallerGetFacebookParam(param=*0x%x)");

	if (!g_fxo->get<sce_np_installer_manager>().is_initialized)
	{
		return SCE_NP_INSTALLER_ERROR_NOT_INITIALIZED;
	}

	if (!param)
	{
		return SCE_NP_INSTALLER_ERROR_INVALID_ARGUMENT;
	}

	// TODO
	return not_an_error(-1);
}

DECLARE(ppu_module_manager::sceNpInstaller)("sceNpInstaller", []()
{
	REG_FUNC(sceNpInstaller, sceNpInstallerInit);
	REG_FUNC(sceNpInstaller, sceNpInstallerTerm);
	REG_FUNC(sceNpInstaller, sceNpInstallerGetNpEnv);
	REG_FUNC(sceNpInstaller, sceNpInstallerGetGuestSignIn);
	REG_FUNC(sceNpInstaller, sceNpInstallerGetFacebookParam);
	REG_FUNC(sceNpInstaller, sceNpInstallerGetFacebookParam2);
});
