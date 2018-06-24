#pragma once
#include "Emu/Memory/Memory.h"
#include "Emu/Cell/ErrorCodes.h"

error_code sys_sm_get_ext_event2(vm::ptr<u32> stack1, vm::ptr<u32> stack2, vm::ptr<u32> stack3, u32 maybe_bool);
error_code sys_sm_get_params(vm::ptr<u64> a, vm::ptr<u64> b, vm::ptr<u64> c, vm::ptr<u64> d); 