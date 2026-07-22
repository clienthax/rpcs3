#pragma once
#include "sceNp.h"

// Error codes
enum SceNpInstallerError : u32
{
	// NP Installer
	SCE_NP_INSTALLER_ERROR_INVALID_ARGUMENT    = 0x80025602,
	SCE_NP_INSTALLER_ERROR_INVALID_DATA        = 0x80025604,
	SCE_NP_INSTALLER_ERROR_BUSY_OR_IN_PROGRESS = 0x8002560a, // Waiting for sysutil callback.
	SCE_NP_INSTALLER_ERROR_NOT_INITIALIZED     = 0x8002560b,
	SCE_NP_INSTALLER_ERROR_ALREADY_INITIALIZED = 0x8002560d,
};

// fxm objects

struct sce_np_installer_manager
{
	atomic_t<bool> is_initialized = false;
};

// Data types

struct sce_np_guest_acc_info
{
	char country[4];
	char language[8];
	SceNpDate birthDate;
};