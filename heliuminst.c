/*
 * HeliumInst - An EFI-based HeliumOS installation program.
 * Launched by HeliumBoot (see boot.c)
 */

#include <efi.h>
#include <efilib.h>

CHAR16 buf[100] = {0};
BOOLEAN sysvr2_flag = FALSE;
BOOLEAN sysvr3_flag = FALSE;

EFI_STATUS EFIAPI
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
	EFI_STATUS Status;

	InitializeLib(ImageHandle, SystemTable);

	Print(L"\nHeliumOS -- Initial Load: Installation Media to Drive\n\n");
	Print(L"The type of disk drive on which the Root file system will reside,\n");
	Print(L"as well as the type of media that will be used for restoring\n");
	Print(L"the rest of the operating system must be specified below.\n\n");

	Print(L"Answer the questions below with a 'y' or 'n', and then\n");
	Print(L"press the Enter key to confirm your answer.\n");
	Print(L"There is no type-ahead -- wait for the question to complete.\n");
	Print(L"Use the Backspace key to erase the last character typed.\n\n");

	Print(L"Note that if you have Secure Boot enabled, reboot your system,\n");
	Print(L"enter your UEFI BIOS settings and disable it before beginning\n");
	Print(L"the installation, otherwise you will not be able to boot HeliumOS.\n");
	Print(L"For more information on how to enter your UEFI BIOS settings,\n");
	Print(L"contact your manufacturer.\n\n");

	// HeliumInst main loop
	for (;;) {
		// Question 1
		for (;;) {
			Print(L"Are you installing SVR2 or SVR3.2?\n");
			Print(L"Type '2' for SVR2 or '3.2' for SVR3.2: ");
			Input(L"", buf, sizeof(buf) / sizeof(CHAR16));
			Print(L"\n");

			if (StrCmp(buf, L"2") == 0) {
				sysvr2_flag = TRUE;
				break;
			} else if (StrCmp(buf, L"3.2") == 0) {
				sysvr3_flag = TRUE;
				break;
			} else
				Print(L"Invalid input. Please try again.\n");
		}

		break;
	}

	Print(L"\nThe HeliumOS miniroot has been successfully copied to your drive.\n");
	Print(L"To boot HeliumOS in order to finish restoring the system,\n");
	Print(L"type 'reboot' at the HeliumBoot prompt, and boot from the\n");
	Print(L"HeliumOS drive where the miniroot was installed. At the\n");
	Print(L"HeliumBoot prompt, type:\n\n");
	Print(L"\t\tboot hd(X,1)unix\n\n");
	Print(L"where X corresponds to the hard disk where you installed the\n");
	Print(L"HeliumOS miniroot. Then press Enter. HeliumOS will start up.\n\n");

	Print(L"Note that HeliumOS will start in single-user mode.\n");
	Print(L"Once you reach the shell prompt, type the command 'heliumrest'\n");
	Print(L"to finish restoring the full HeliumOS system.\n\n");

	Print(L"Good Luck!\n\n");
	Print(L"The program will now exit.\n\n");

	return EFI_SUCCESS;
}
