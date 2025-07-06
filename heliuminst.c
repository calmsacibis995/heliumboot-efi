#include <efi.h>
#include <efilib.h>

EFI_STATUS EFIAPI
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
	EFI_STATUS Status;
	EFI_BLOCK_IO_PROTOCOL *BlockIo;

	InitializeLib(ImageHandle, SystemTable);

	Print(L"\nHeliumOS -- Initial Load: Installation media to Drive\n\n");
	Print(L"The type of disk drive on which the Root file system will reside,\n");
	Print(L"as well as the type of media that will be used for restoring\n");
	Print(L"the rest of the operating system must be specified below.\n\n");

	Print(L"Answer the questions below with a 'y' or 'n', and then\n");
	Print(L"press the Enter key to confirm your answer.\n");
	Print(L"There is no type-ahead -- wait for the question to complete.\n");
	Print(L"Use the Backspace key to erase the last character typed.\n\n");

	Print(L"Note that if you have Secure Boot enabled, reboot your system,\n");
	Print(L"enter your UEFI BIOS settings and disable it! For more information\n");
	Print(L"on how to enter your UEFI BIOS settings, contact your manufacturer.\n\n");

	Print(L"The HeliumOS miniroot has been successfully copied to your drive.\n");
	Print(L"To boot HeliumOS in order to finish restoring the system,\n");
	Print(L"reboot the system, and boot from the HeliumOS drive.\n");
	Print(L"Note that the HeliumOS will start in single-user mode.\n");
	Print(L"Once you reach the shell prompt, type the command 'heliumrest'\n");
	Print(L"to finish restoring the full HeliumOS system.\n\n");

	return EFI_SUCCESS;
}
