#include "stdafx.h"
#include "Emu/System.h"
#include "Emu/Cell/PPUModule.h"

#include "cellGame.h"
#include "cellSysutil.h"
#include "cellNetCtl.h"

#include "Utilities/StrUtil.h"

logs::channel cellNetCtl("cellNetCtl");
logs::channel netctl_main("netctl_main");

template <>
void fmt_class_string<CellNetCtlError>::format(std::string& out, u64 arg)
{
	format_enum(out, arg, [](auto error)
	{
		switch (error)
		{
			STR_CASE(CELL_NET_CTL_ERROR_NOT_INITIALIZED);
			STR_CASE(CELL_NET_CTL_ERROR_NOT_TERMINATED);
			STR_CASE(CELL_NET_CTL_ERROR_HANDLER_MAX);
			STR_CASE(CELL_NET_CTL_ERROR_ID_NOT_FOUND);
			STR_CASE(CELL_NET_CTL_ERROR_INVALID_ID);
			STR_CASE(CELL_NET_CTL_ERROR_INVALID_CODE);
			STR_CASE(CELL_NET_CTL_ERROR_INVALID_ADDR);
			STR_CASE(CELL_NET_CTL_ERROR_NOT_CONNECTED);
			STR_CASE(CELL_NET_CTL_ERROR_NOT_AVAIL);
			STR_CASE(CELL_NET_CTL_ERROR_INVALID_TYPE);
			STR_CASE(CELL_NET_CTL_ERROR_INVALID_SIZE);
			STR_CASE(CELL_NET_CTL_ERROR_NET_DISABLED);
			STR_CASE(CELL_NET_CTL_ERROR_NET_NOT_CONNECTED);
			STR_CASE(CELL_NET_CTL_ERROR_NP_NO_ACCOUNT);
			STR_CASE(CELL_NET_CTL_ERROR_NP_RESERVED1);
			STR_CASE(CELL_NET_CTL_ERROR_NP_RESERVED2);
			STR_CASE(CELL_NET_CTL_ERROR_NET_CABLE_NOT_CONNECTED);
			STR_CASE(CELL_NET_CTL_ERROR_DIALOG_CANCELED);
			STR_CASE(CELL_NET_CTL_ERROR_DIALOG_ABORTED);

			STR_CASE(CELL_NET_CTL_ERROR_WLAN_DEAUTHED);
			STR_CASE(CELL_NET_CTL_ERROR_WLAN_KEYINFO_EXCHNAGE_TIMEOUT);
			STR_CASE(CELL_NET_CTL_ERROR_WLAN_ASSOC_FAILED);
			STR_CASE(CELL_NET_CTL_ERROR_WLAN_AP_DISAPPEARED);
			STR_CASE(CELL_NET_CTL_ERROR_PPPOE_SESSION_INIT);
			STR_CASE(CELL_NET_CTL_ERROR_PPPOE_SESSION_NO_PADO);
			STR_CASE(CELL_NET_CTL_ERROR_PPPOE_SESSION_NO_PADS);
			STR_CASE(CELL_NET_CTL_ERROR_PPPOE_SESSION_GET_PADT);
			STR_CASE(CELL_NET_CTL_ERROR_PPPOE_SESSION_SERVICE_NAME);
			STR_CASE(CELL_NET_CTL_ERROR_PPPOE_SESSION_AC_SYSTEM);
			STR_CASE(CELL_NET_CTL_ERROR_PPPOE_SESSION_GENERIC);
			STR_CASE(CELL_NET_CTL_ERROR_PPPOE_STATUS_AUTH);
			STR_CASE(CELL_NET_CTL_ERROR_PPPOE_STATUS_NETWORK);
			STR_CASE(CELL_NET_CTL_ERROR_PPPOE_STATUS_TERMINATE);
			STR_CASE(CELL_NET_CTL_ERROR_DHCP_LEASE_TIME);

			STR_CASE(CELL_GAMEUPDATE_ERROR_NOT_INITIALIZED);
			STR_CASE(CELL_GAMEUPDATE_ERROR_ALREADY_INITIALIZED);
			STR_CASE(CELL_GAMEUPDATE_ERROR_INVALID_ADDR);
			STR_CASE(CELL_GAMEUPDATE_ERROR_INVALID_SIZE);
			STR_CASE(CELL_GAMEUPDATE_ERROR_INVALID_MEMORY_CONTAINER);
			STR_CASE(CELL_GAMEUPDATE_ERROR_INSUFFICIENT_MEMORY_CONTAINER);
			STR_CASE(CELL_GAMEUPDATE_ERROR_BUSY);
			STR_CASE(CELL_GAMEUPDATE_ERROR_NOT_START);
			STR_CASE(CELL_GAMEUPDATE_ERROR_LOAD_FAILED);
		}

		return unknown;
	});
}

template <>
void fmt_class_string<CellNetCtlState>::format(std::string& out, u64 arg)
{
	format_enum(out, arg, [](CellNetCtlState value)
	{
		switch (value)
		{
		case CELL_NET_CTL_STATE_Disconnected: return "Disconnected";
		case CELL_NET_CTL_STATE_Connecting: return "Connecting";
		case CELL_NET_CTL_STATE_IPObtaining: return "Obtaining IP";
		case CELL_NET_CTL_STATE_IPObtained: return "IP Obtained";
		}

		return unknown;
	});
}

error_code cellNetCtlInit()
{
	cellNetCtl.warning("cellNetCtlInit()");

	return CELL_OK;
}

error_code cellNetCtlTerm()
{
	cellNetCtl.warning("cellNetCtlTerm()");

	return CELL_OK;
}

error_code cellNetCtlGetState(vm::ptr<u32> state)
{
	cellNetCtl.trace("cellNetCtlGetState(state=*0x%x)", state);

	*state = g_cfg.net.net_status;
	return CELL_OK;
}

error_code cellNetCtlAddHandler(vm::ptr<cellNetCtlHandler> handler, vm::ptr<void> arg, vm::ptr<s32> hid)
{
	cellNetCtl.todo("cellNetCtlAddHandler(handler=*0x%x, arg=*0x%x, hid=*0x%x)", handler, arg, hid);

	return CELL_OK;
}

error_code cellNetCtlDelHandler(s32 hid)
{
	cellNetCtl.todo("cellNetCtlDelHandler(hid=0x%x)", hid);

	return CELL_OK;
}

error_code cellNetCtlGetInfo(s32 code, vm::ptr<CellNetCtlInfo> info)
{
	cellNetCtl.todo("cellNetCtlGetInfo(code=0x%x (%s), info=*0x%x)", code, InfoCodeToName(code), info);

	if (code == CELL_NET_CTL_INFO_DEVICE)
	{//Called by vsh while doing connection tests
		info->device = CELL_NET_CTL_DEVICE_WIRELESS;
	}

	if (code == CELL_NET_CTL_INFO_ETHER_ADDR)
	{
		// dummy values set
		std::memset(info->ether_addr.data, 0xFF, sizeof(info->ether_addr.data));
		return CELL_OK;
	}

	if (code == CELL_NET_CTL_INFO_MTU)
	{
		info->mtu = 1500;
	}
	else if (code == CELL_NET_CTL_INFO_LINK)
	{
		info->link = CELL_NET_CTL_LINK_CONNECTED;
	}
	else if (code == CELL_NET_CTL_INFO_IP_ADDRESS)
	{
		if (g_cfg.net.net_status != CELL_NET_CTL_STATE_IPObtained)
		{
			// 0.0.0.0 seems to be the default address when no ethernet cables are connected to the PS3
			strcpy_trunc(info->ip_address, "0.0.0.0");
		}
		else
		{
			strcpy_trunc(info->ip_address, "192.168.1.75");
		}
	}
	else if (code == CELL_NET_CTL_INFO_NETMASK)
	{
		strcpy_trunc(info->netmask, "255.255.255.255");
	}
	else if (code == CELL_NET_CTL_INFO_HTTP_PROXY_CONFIG)
	{
		info->http_proxy_config = 0;
	}

	return CELL_OK;
}

error_code cellNetCtlNetStartDialogLoadAsync(vm::ptr<CellNetCtlNetStartDialogParam> param)
{
	cellNetCtl.error("cellNetCtlNetStartDialogLoadAsync(param=*0x%x)", param);

	// TODO: Actually sign into PSN or an emulated network similar to PSN (ESN)
	// TODO: Properly open the dialog prompt for sign in
	sysutil_send_system_cmd(CELL_SYSUTIL_NET_CTL_NETSTART_LOADED, 0);
	sysutil_send_system_cmd(CELL_SYSUTIL_NET_CTL_NETSTART_FINISHED, 0);

	return CELL_OK;
}

error_code cellNetCtlNetStartDialogAbortAsync()
{
	cellNetCtl.error("cellNetCtlNetStartDialogAbortAsync()");

	return CELL_OK;
}

error_code cellNetCtlNetStartDialogUnloadAsync(vm::ptr<CellNetCtlNetStartDialogResult> result)
{
	cellNetCtl.warning("cellNetCtlNetStartDialogUnloadAsync(result=*0x%x)", result);

	result->result = CELL_NET_CTL_ERROR_DIALOG_CANCELED;
	sysutil_send_system_cmd(CELL_SYSUTIL_NET_CTL_NETSTART_UNLOADED, 0);

	return CELL_OK;
}

error_code cellNetCtlGetNatInfo(vm::ptr<CellNetCtlNatInfo> natInfo)
{
	cellNetCtl.todo("cellNetCtlGetNatInfo(natInfo=*0x%x)", natInfo);

	if (natInfo->size == 0)
	{
		cellNetCtl.error("cellNetCtlGetNatInfo : CELL_NET_CTL_ERROR_INVALID_SIZE");
		return CELL_NET_CTL_ERROR_INVALID_SIZE;
	}

	return CELL_OK;
}

error_code cellNetCtlAddHandlerGameInt()
{
	cellNetCtl.todo("cellNetCtlAddHandlerGameInt()");
	return CELL_OK;
}

error_code cellNetCtlConnectGameInt()
{
	cellNetCtl.todo("cellNetCtlConnectGameInt()");
	return CELL_OK;
}

error_code cellNetCtlDelHandlerGameInt()
{
	cellNetCtl.todo("cellNetCtlDelHandlerGameInt()");
	return CELL_OK;
}

error_code cellNetCtlDisconnectGameInt()
{
	cellNetCtl.todo("cellNetCtlDisconnectGameInt()");
	return CELL_OK;
}

error_code cellNetCtlGetInfoGameInt()
{
	cellNetCtl.todo("cellNetCtlGetInfoGameInt()");
	return CELL_OK;
}

error_code cellNetCtlGetScanInfoGameInt()
{
	cellNetCtl.todo("cellNetCtlGetScanInfoGameInt()");
	return CELL_OK;
}

error_code cellNetCtlGetStateGameInt()
{
	cellNetCtl.todo("cellNetCtlGetStateGameInt()");
	return CELL_OK;
}

error_code cellNetCtlScanGameInt()
{
	cellNetCtl.todo("cellNetCtlScanGameInt()");
	return CELL_OK;
}

error_code cellGameUpdateInit()
{
	cellNetCtl.todo("cellGameUpdateInit()");
	return CELL_OK;
}

error_code cellGameUpdateTerm()
{
	cellNetCtl.todo("cellGameUpdateTerm()");
	return CELL_OK;
}

error_code cellGameUpdateCheckStartAsync(vm::cptr<CellGameUpdateParam> param, vm::ptr<CellGameUpdateCallback> cb_func, vm::ptr<void> userdata)
{
	cellNetCtl.todo("cellGameUpdateCheckStartAsync(param=*0x%x, cb_func=*0x%x, userdata=*0x%x)", param, cb_func, userdata);
	sysutil_register_cb([=](ppu_thread& ppu) -> s32
	{
		cb_func(ppu, CELL_GAMEUPDATE_RESULT_STATUS_NO_UPDATE, CELL_OK, userdata);
		return CELL_OK;
	});
	return CELL_OK;
}

error_code cellGameUpdateCheckFinishAsync(vm::ptr<CellGameUpdateCallback> cb_func, vm::ptr<void> userdata)
{
	cellNetCtl.todo("cellGameUpdateCheckFinishAsync(cb_func=*0x%x, userdata=*0x%x)", cb_func, userdata);
	sysutil_register_cb([=](ppu_thread& ppu) -> s32
	{
		cb_func(ppu, CELL_GAMEUPDATE_RESULT_STATUS_FINISHED, CELL_OK, userdata);
		return CELL_OK;
	});
	return CELL_OK;
}

error_code cellGameUpdateCheckStartWithoutDialogAsync(vm::ptr<CellGameUpdateCallback> cb_func, vm::ptr<void> userdata)
{
	cellNetCtl.todo("cellGameUpdateCheckStartWithoutDialogAsync(cb_func=*0x%x, userdata=*0x%x)", cb_func, userdata);
	sysutil_register_cb([=](ppu_thread& ppu) -> s32
	{
		cb_func(ppu, CELL_GAMEUPDATE_RESULT_STATUS_NO_UPDATE, CELL_OK, userdata);
		return CELL_OK;
	});
	return CELL_OK;
}

error_code cellGameUpdateCheckAbort()
{
	cellNetCtl.todo("cellGameUpdateCheckAbort()");
	return CELL_OK;
}

error_code cellGameUpdateCheckStartAsyncEx(vm::cptr<CellGameUpdateParam> param, vm::ptr<CellGameUpdateCallbackEx> cb_func, vm::ptr<void> userdata)
{
	cellNetCtl.todo("cellGameUpdateCheckStartAsyncEx(param=*0x%x, cb_func=*0x%x, userdata=*0x%x)", param, cb_func, userdata);
	sysutil_register_cb([=](ppu_thread& ppu) -> s32
	{
		cb_func(ppu, vm::make_var(CellGameUpdateResult{ CELL_GAMEUPDATE_RESULT_STATUS_NO_UPDATE, CELL_OK, 0x0, 0x0}), userdata);
		return CELL_OK;
	});
	return CELL_OK;
}

error_code cellGameUpdateCheckFinishAsyncEx(vm::ptr<CellGameUpdateCallbackEx> cb_func, vm::ptr<void> userdata)
{
	cellNetCtl.todo("cellGameUpdateCheckFinishAsyncEx(cb_func=*0x%x, userdata=*0x%x)", cb_func, userdata);
	const s32 PROCESSING_COMPLETE = 5;
	sysutil_register_cb([=](ppu_thread& ppu) -> s32
	{
		cb_func(ppu, vm::make_var(CellGameUpdateResult{ CELL_GAMEUPDATE_RESULT_STATUS_FINISHED, CELL_OK, 0x0, 0x0}), userdata);
		return CELL_OK;
	});
	return CELL_OK;
}

error_code cellGameUpdateCheckStartWithoutDialogAsyncEx(vm::ptr<CellGameUpdateCallbackEx> cb_func, vm::ptr<void> userdata)
{
	cellNetCtl.todo("cellGameUpdateCheckStartWithoutDialogAsyncEx(cb_func=*0x%x, userdata=*0x%x)", cb_func, userdata);
	sysutil_register_cb([=](ppu_thread& ppu) -> s32
	{
		cb_func(ppu, vm::make_var(CellGameUpdateResult{ CELL_GAMEUPDATE_RESULT_STATUS_NO_UPDATE, CELL_OK, 0x0, 0x0}), userdata);
		return CELL_OK;
	});
	return CELL_OK;
}


//Vsh functions

error_code sceNetApCtlConnectVsh()
{
	netctl_main.error("sceNetApCtlConnectVsh(???)");
	return CELL_OK;
}

error_code sceNetApCtlDisconnectVsh()
{
	netctl_main.error("sceNetApCtlDisconnectVsh(???)");
	return CELL_OK;
}

error_code sceNetApCtlGetInfoVsh()
{
	netctl_main.error("sceNetApCtlGetInfoVsh(???)");
	return CELL_OK;
}

error_code sceNetApCtlGetStateVsh()
{
	netctl_main.error("sceNetApCtlGetStateVsh(???)");
	return CELL_OK;
}

error_code sceNetApCtlInitVsh()
{
	netctl_main.error("sceNetApCtlInitVsh(???)");
	return CELL_OK;
}

error_code sceNetApCtlTermVsh()
{
	netctl_main.error("sceNetApCtlTermVsh(???)");
	return CELL_OK;
}

error_code sceNetCtlAddHandlerSysUtil()
{
	netctl_main.error("sceNetCtlAddHandlerSysUtil(???)");
	return CELL_OK;
}

error_code sceNetCtlAddHandlerVsh()
{
	netctl_main.error("sceNetCtlAddHandlerVsh(???)");
	return CELL_OK;
}

error_code sceNetCtlConnectVsh()
{
	netctl_main.error("sceNetCtlConnectVsh(???)");
	return CELL_OK;
}

error_code sceNetCtlConnectWithRetryVsh()
{
	netctl_main.error("sceNetCtlConnectWithRetryVsh(???)");
	return CELL_OK;
}

error_code sceNetCtlDelHandlerSysUtil()
{
	netctl_main.error("sceNetCtlDelHandlerSysUtil(???)");
	return CELL_OK;
}

error_code sceNetCtlDelHandlerVsh()
{
	netctl_main.error("sceNetCtlDelHandlerVsh(???)");
	return CELL_OK;
}

error_code sceNetCtlDisconnectVsh()
{
	netctl_main.error("sceNetCtlDisconnectVsh(???)");
	return CELL_OK;
}

//error_code sceNetCtlGetInfoVsh()
error_code sceNetCtlGetInfoVsh(s32 code, vm::ptr<CellNetCtlInfo> info)
{
	netctl_main.error("sceNetCtlGetInfoVsh(???), passing to cellNetCtlGetInfo");
	return cellNetCtlGetInfo(code, info);
}

error_code sceNetCtlGetScanInfoVsh()
{
	netctl_main.error("sceNetCtlGetScanInfoVsh(???)");
	return CELL_OK;
}

error_code sceNetCtlGetStateSysUtil()
{
	netctl_main.error("sceNetCtlGetStateSysUtil(???)");
	return CELL_OK;
}

error_code sceNetCtlGetStateVsh(vm::ptr<u32> state)
{
	netctl_main.error("sceNetCtlGetStateVsh(???) passing to cellNetCtlGetState");
	return cellNetCtlGetState(state);
}

error_code sceNetCtlInitVsh()
{
	netctl_main.error("sceNetCtlInitVsh(???)");
	return CELL_OK;
}

error_code sceNetCtlScanVsh()
{
	netctl_main.error("sceNetCtlScanVsh(???)");
	return CELL_OK;
}

error_code sceNetCtlTermVsh()
{
	netctl_main.error("sceNetCtlTermVsh(???)");
	return CELL_OK;
}

error_code func_1237870F()
{
	netctl_main.error("func_1237870F(???)");
	return CELL_OK;
}

error_code func_23C35ECE()
{
	netctl_main.error("func_23C35ECE(???)");
	return CELL_OK;
}

error_code func_35A1C363()
{
	netctl_main.error("func_35A1C363(???)");
	return CELL_OK;
}

error_code func_367EFAA8()
{
	netctl_main.error("func_367EFAA8(???)");
	return CELL_OK;
}

error_code func_3A5CB886()
{
	netctl_main.error("func_3A5CB886(???)");
	return CELL_OK;
}

error_code func_442F0E40()
{
	netctl_main.error("func_442F0E40(???)");
	return CELL_OK;
}

error_code func_4E4B734D()
{
	netctl_main.error("func_4E4B734D(???)");
	return CELL_OK;
}

error_code func_5101A052()
{
	netctl_main.error("func_5101A052(???)");
	return CELL_OK;
}

error_code func_55D2047A()
{
	netctl_main.error("func_55D2047A(???)");
	return CELL_OK;
}

error_code func_6F2D52F1()
{
	netctl_main.error("func_6F2D52F1(???)");
	return CELL_OK;
}

error_code func_C67D3DB3()
{
	netctl_main.error("func_C67D3DB3(???)");
	return CELL_OK;
}

DECLARE(ppu_module_manager::netctl_main)("netctl_main", []()
{
	REG_FUNC(netctl_main, sceNetApCtlConnectVsh);
	REG_FUNC(netctl_main, sceNetApCtlDisconnectVsh);
	REG_FUNC(netctl_main, sceNetApCtlGetInfoVsh).flag(MFF_FORCED_HLE);
	REG_FUNC(netctl_main, sceNetApCtlGetStateVsh).flag(MFF_FORCED_HLE);
	REG_FUNC(netctl_main, sceNetApCtlInitVsh);
	REG_FUNC(netctl_main, sceNetApCtlTermVsh);
	REG_FUNC(netctl_main, sceNetCtlAddHandlerSysUtil);
	REG_FUNC(netctl_main, sceNetCtlAddHandlerVsh);
	REG_FUNC(netctl_main, sceNetCtlConnectVsh);
	REG_FUNC(netctl_main, sceNetCtlConnectWithRetryVsh);
	REG_FUNC(netctl_main, sceNetCtlDelHandlerSysUtil);
	REG_FUNC(netctl_main, sceNetCtlDelHandlerVsh);
	REG_FUNC(netctl_main, sceNetCtlDisconnectVsh);
	REG_FUNC(netctl_main, sceNetCtlGetInfoVsh).flag(MFF_FORCED_HLE);
	REG_FUNC(netctl_main, sceNetCtlGetScanInfoVsh).flag(MFF_FORCED_HLE);
	REG_FUNC(netctl_main, sceNetCtlGetStateSysUtil).flag(MFF_FORCED_HLE);
	REG_FUNC(netctl_main, sceNetCtlGetStateVsh).flag(MFF_FORCED_HLE);
	REG_FUNC(netctl_main, sceNetCtlInitVsh);
	REG_FUNC(netctl_main, sceNetCtlScanVsh);
	REG_FUNC(netctl_main, sceNetCtlTermVsh);

	REG_FNID(netctl_main, 0x1237870F, func_1237870F);
	REG_FNID(netctl_main, 0x23C35ECE, func_23C35ECE);
	REG_FNID(netctl_main, 0x35A1C363, func_35A1C363);
	REG_FNID(netctl_main, 0x367EFAA8, func_367EFAA8);
	REG_FNID(netctl_main, 0x3A5CB886, func_3A5CB886);
	REG_FNID(netctl_main, 0x442F0E40, func_442F0E40);
	REG_FNID(netctl_main, 0x4E4B734D, func_4E4B734D);
	REG_FNID(netctl_main, 0x5101A052, func_5101A052);
	REG_FNID(netctl_main, 0x55D2047A, func_55D2047A);
	REG_FNID(netctl_main, 0x6F2D52F1, func_6F2D52F1);
	REG_FNID(netctl_main, 0xC67D3DB3, func_C67D3DB3);

});

DECLARE(ppu_module_manager::cellNetCtl)("cellNetCtl", []()
{
	REG_FUNC(cellNetCtl, cellNetCtlInit);
	REG_FUNC(cellNetCtl, cellNetCtlTerm);

	REG_FUNC(cellNetCtl, cellNetCtlGetState);
	REG_FUNC(cellNetCtl, cellNetCtlAddHandler);
	REG_FUNC(cellNetCtl, cellNetCtlDelHandler);

	REG_FUNC(cellNetCtl, cellNetCtlGetInfo);

	REG_FUNC(cellNetCtl, cellNetCtlNetStartDialogLoadAsync);
	REG_FUNC(cellNetCtl, cellNetCtlNetStartDialogAbortAsync);
	REG_FUNC(cellNetCtl, cellNetCtlNetStartDialogUnloadAsync);

	REG_FUNC(cellNetCtl, cellNetCtlGetNatInfo);

	REG_FUNC(cellNetCtl, cellNetCtlAddHandlerGameInt);
	REG_FUNC(cellNetCtl, cellNetCtlConnectGameInt);
	REG_FUNC(cellNetCtl, cellNetCtlDelHandlerGameInt);
	REG_FUNC(cellNetCtl, cellNetCtlDisconnectGameInt);
	REG_FUNC(cellNetCtl, cellNetCtlGetInfoGameInt);
	REG_FUNC(cellNetCtl, cellNetCtlGetScanInfoGameInt);
	REG_FUNC(cellNetCtl, cellNetCtlGetStateGameInt);
	REG_FUNC(cellNetCtl, cellNetCtlScanGameInt);

	REG_FUNC(cellNetCtl, cellGameUpdateInit);
	REG_FUNC(cellNetCtl, cellGameUpdateTerm);

	REG_FUNC(cellNetCtl, cellGameUpdateCheckStartAsync);
	REG_FUNC(cellNetCtl, cellGameUpdateCheckFinishAsync);
	REG_FUNC(cellNetCtl, cellGameUpdateCheckStartWithoutDialogAsync);
	REG_FUNC(cellNetCtl, cellGameUpdateCheckAbort);
	REG_FUNC(cellNetCtl, cellGameUpdateCheckStartAsyncEx);
	REG_FUNC(cellNetCtl, cellGameUpdateCheckFinishAsyncEx);
	REG_FUNC(cellNetCtl, cellGameUpdateCheckStartWithoutDialogAsyncEx);
});
