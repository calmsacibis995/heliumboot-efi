#include <efi.h>
#include <efilib.h>

#include "boot.h"

struct boot_command_tab cmd_tab[] = {
	//{ L"boot", boot, CMD_REQUIRED_ARGS, L"boot: DEVICE " },
	{ L"echo", echo, CMD_REQUIRED_ARGS, L"echo: STRING" },
	{ L"ls", ls, CMD_OPTIONAL_ARGS, L"ls: [DIRECTORY] [DEVICE]" },
	{ L"version", print_version, CMD_NO_ARGS, L"version" },
	{ NULL, NULL, CMD_NO_ARGS, NULL }
};
