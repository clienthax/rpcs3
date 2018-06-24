#include "stdafx.h"
#include "sys_tty.h"

#include "sys_uart.h"
#include "sys_ppu_thread.h"

logs::channel sys_uart("sys_uart");

s32 sys_uart_initialize()
{
	sys_uart.todo("sys_uart_initialize()");
	return CELL_OK;
}

s32 sys_uart_receive(ppu_thread& ppu, vm::ptr<char> buffer, u64 size, u32 unk)
{
	if (size != 0x800) return 0; // HACK: Unimplemented - we only send the 'stop waiting' packet, or whatever

	struct uart_required_packet
	{
		struct
		{
			be_t<u8> unk1;
			be_t<u8> unk2;
			be_t<u16> payload_size;
		} header;
		be_t<u32> type; //??
		be_t<u32> cmd; // < 0x13, in sub_5DA13C
		be_t<u32> cmd_size; // < cmd_size?
		u8 data_stuff[0x40];
	};
	auto packets = reinterpret_cast<uart_required_packet *>(buffer.get_ptr());

	be_t<u32> types[] { 0x80000004, 0x1000001, 0xffffffff };

	int i = 0;
	for (auto type : types)
	{
		uart_required_packet & packet = packets[i++];
		packet.header.payload_size = sizeof(packet) - sizeof(packet.header);
		packet.type = type;
		packet.cmd = 15; // 0 or 15 return 0
	}

	return sizeof(uart_required_packet) * sizeof(types) / sizeof(types[0]);
}

s32 sys_uart_send(ppu_thread& ppu, vm::cptr<u8> buffer, u64 size, u64 flags)
{
	sys_uart.todo("sys_uart_send(buffer=0x%x, size=0x%llx, flags=0x%x)", buffer, size, flags);
	return size;
}

s32 sys_uart_get_params(vm::ptr<char> buffer)
{
	// buffer's size should be 0x10
	sys_uart.todo("sys_uart_get_params(buffer=0x%x)", buffer);
	return CELL_OK;
}
