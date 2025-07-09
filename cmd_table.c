#include <efi.h>
#include <efilib.h>

#include "boot.h"

struct boot_command_tab cmd_tab[] = {
	{ L"?", help, CMD_NO_ARGS, L"?: help" },
	{ L"boot", boot, CMD_REQUIRED_ARGS, L"boot: DEVICE:PATH [ARGS]" },
	{ L"boot_efi", boot_efi, CMD_REQUIRED_ARGS, L"boot_efi: DEVICE:PATH [ARGS]" },
	{ L"echo", echo, CMD_REQUIRED_ARGS, L"echo: STRING" },
	{ L"exit", exit, CMD_NO_ARGS, L"exit: exit" },
	{ L"help", help, CMD_NO_ARGS, L"help: help" },
	{ L"ls", ls, CMD_OPTIONAL_ARGS, L"ls: [DIRECTORY] [DEVICE]" },
	{ L"reboot", reboot, CMD_NO_ARGS, L"reboot: reboot" },
	{ L"version", print_version, CMD_NO_ARGS, L"version: version" },
	{ NULL, NULL, CMD_NO_ARGS, NULL }
};
