#include "stdafx.h"
#include "sys_tty.h"

#include "sys_sm.h"
#include "sys_ppu_thread.h"

logs::channel sys_sm("sys_sm");
error_code sys_sm_get_ext_event2(vm::ptr<u32> stack1, vm::ptr<u32> stack2, vm::ptr<u32> stack3, u32 maybe_bool)
{
	sys_sm.todo("sys_sm_get_ext_event2(*0x%x, *0x%x, *0x%x, 0x%x)", stack1, stack2, stack3, maybe_bool);
	// stack_1 should be a u32 with a 5, a 7 or a 3

	// Writing 7 to stack1 results in a message of "Console too hot, please restart"
	return CELL_OK;
}

error_code sys_sm_get_params(vm::ptr<u64> a, vm::ptr<u64> b, vm::ptr<u64> c, vm::ptr<u64> d)
{
	sys_sm.todo("sys_sm_get_params(0x%x, 0x%x, 0x%x, 0x%x)", a, b, c, d);
	*a = 0;
	*b = 0;
	*c = 0;
	*d = 0;
	return CELL_OK;
}