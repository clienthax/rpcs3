#include "stdafx.h"
#include "Emu/Cell/PPUModule.h"

logs::channel sys_io("sys_io");


extern void cellPad_init();
extern void cellKb_init();
extern void cellMouse_init();


s32 sys_config_start()
{
	fmt::throw_exception("Unimplemented" HERE);
}

s32 sys_config_stop()
{
	fmt::throw_exception("Unimplemented" HERE);
}

s32 sys_config_add_service_listener()
{
	fmt::throw_exception("Unimplemented" HERE);
}

s32 sys_config_remove_service_listener()
{
	fmt::throw_exception("Unimplemented" HERE);
}

s32 sys_config_register_io_error_handler()
{
	fmt::throw_exception("Unimplemented" HERE);
}

s32 sys_config_register_service()
{
	fmt::throw_exception("Unimplemented" HERE);
}

s32 sys_config_unregister_io_error_handler()
{
	fmt::throw_exception("Unimplemented" HERE);
}

s32 sys_config_unregister_service()
{
	fmt::throw_exception("Unimplemented" HERE);
}


s32 sys_io_10DA8DCC() {
	UNIMPLEMENTED_FUNC(sys_io);
	return CELL_OK;
}

s32 sys_io_13ED2BA4() {
	UNIMPLEMENTED_FUNC(sys_io);
	return CELL_OK;
}


s32 sys_io_5A2645AF() {
	UNIMPLEMENTED_FUNC(sys_io);
	return CELL_OK;
}

s32 sys_io_5C016E79() {
	UNIMPLEMENTED_FUNC(sys_io);
	return CELL_OK;
}

s32 sys_io_7009B738() {
	UNIMPLEMENTED_FUNC(sys_io);
	return CELL_OK;
}

s32 sys_io_73BB2D74() {
	UNIMPLEMENTED_FUNC(sys_io);
	return CELL_OK;
}

s32 sys_io_CCC785DF() {
	UNIMPLEMENTED_FUNC(sys_io);
	return CELL_OK;
}


DECLARE(ppu_module_manager::sys_io)("sys_io", []()
{
	cellPad_init();
	cellKb_init();
	cellMouse_init();

	REG_FNID(sys_io, 0x10DA8DCC, sys_io_10DA8DCC);
	REG_FNID(sys_io, 0x13ED2BA4, sys_io_13ED2BA4);
	REG_FNID(sys_io, 0x5A2645AF, sys_io_5A2645AF);
	REG_FNID(sys_io, 0x5C016E79, sys_io_5C016E79);
	REG_FNID(sys_io, 0x7009B738, sys_io_7009B738);
	REG_FNID(sys_io, 0x73BB2D74, sys_io_73BB2D74);
	REG_FNID(sys_io, 0xCCC785DF, sys_io_CCC785DF);

	REG_FUNC(sys_io, sys_config_start);
	REG_FUNC(sys_io, sys_config_stop);
	REG_FUNC(sys_io, sys_config_add_service_listener);
	REG_FUNC(sys_io, sys_config_remove_service_listener);
	REG_FUNC(sys_io, sys_config_register_io_error_handler);
	REG_FUNC(sys_io, sys_config_register_service);
	REG_FUNC(sys_io, sys_config_unregister_io_error_handler);
	REG_FUNC(sys_io, sys_config_unregister_service);
});
