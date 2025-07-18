#include <efi.h>
#include <efilib.h>

#include "boot.h"

void
boot(CHAR16 *args)
{
	Print(L"Not implemented yet!\n");
}

void
boot_efi(CHAR16 *args)
{
	EFI_STATUS Status;

	Print(L"Booting EFI file: %s\n", args);

	Status = LoadFileEFI(gImageHandle, gST, args);
	if (EFI_ERROR(Status))
		Print(L"boot_efi: Failed to boot file (%r)\n", Status);
}

void
cls(CHAR16 *args)
{
	Print(L"Not implemented yet!\n");
}

void
echo(CHAR16 *args)
{
	Print(L"%s\n", args);
}

void
exit(CHAR16 *args)
{
	exit_flag = TRUE;
}

void
help(CHAR16 *args)
{
	struct boot_command_tab *cmd;

	Print(L"The commands supported by HeliumBoot are:\n");
	for (cmd = cmd_tab; cmd->cmd_name != NULL; cmd++)
		Print(L"  %s\n", cmd->cmd_usage);
	Print(L"\n");
}

void
ls(CHAR16 *args)
{
	Print(L"Not implemented yet!\n");
}

void
reboot(CHAR16 *args)
{
	EFI_STATUS Status;

	Status = uefi_call_wrapper(RT->ResetSystem, 4, EfiResetCold, EFI_SUCCESS, 0, NULL);
	if (EFI_ERROR(Status))
		Print(L"Reset failed\n");
}

void
print_revision(CHAR16 *args)
{
	Print(L"Internal revision %s, %s\n", getrevision(), getrevdate());
}

void
print_version(CHAR16 *args)
{
	Print(L"%s\n", getversion());
}
