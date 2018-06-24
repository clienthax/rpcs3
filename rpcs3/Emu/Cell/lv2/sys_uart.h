#pragma once

#include "Emu/Memory/Memory.h"
#include "Emu/Cell/ErrorCodes.h"

struct uart_packet
{
	u8 magic;
	u8 size;
	u8 cmd;
	u8 data[1];
};

// SysCalls
s32 sys_uart_initialize();
s32 sys_uart_receive(ppu_thread& ppu, vm::ptr<char> buffer, u64 size, u32 unk);
s32 sys_uart_send(ppu_thread& ppu, vm::cptr<u8> buffer, u64 size, u64 flags);
s32 sys_uart_get_params(vm::ptr<char> buffer);
