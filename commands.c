#include <efi.h>
#include <efilib.h>

#include "boot.h"

void
about(CHAR16 *args)
{
	Print(L"HeliumBoot/EFI - The HeliumOS boot loader\n");
	Print(L"Copyright (c) 2025 Stefanos Stefanidis.\n");
	Print(L"All rights reserved.\n");
	Print(L"Version %s\n", getbuildno());
	Print(L"Internal revision %s, %s\n", getrevision(), getrevdate());
}

/*
 * Boot an exeutable file. Does not support EFI applications; see boot_efi().
 * Not fully implemented yet.
 */
void
boot(CHAR16 *args)
{
	EFI_STATUS Status;

	Status = LoadFile(args);
	if (EFI_ERROR(Status))
		Print(L"boot: Failed to boot file (%r)\n", Status);
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
	EFI_STATUS Status;

	// Reset console output.
	Status = uefi_call_wrapper(ST->ConOut->Reset, 2, ST->ConOut, TRUE);
	if (EFI_ERROR(Status))
		Print(L"Warning: Failed to reset console: %r\n", Status);

	// Clear the screen.
	Status = uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut);
	if (EFI_ERROR(Status))
		Print(L"Failed to clear screen: %r\n", Status);
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
	EFI_STATUS Status;
	EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *SimpleFileSystem;
	EFI_FILE_PROTOCOL *Root = NULL;
	EFI_FILE_PROTOCOL *Dir = NULL;
	EFI_FILE_INFO *FileInfo = NULL;
	UINTN BufferSize;
	CHAR16 *Path = NULL;

	Status = uefi_call_wrapper(BS->HandleProtocol, 3, gImageHandle,
		&LoadedImageProtocol, (void **)&LoadedImage);
	if (EFI_ERROR(Status)) {
		Print(L"Failed to get LoadedImage protocol: %r\n", Status);
		return;
	}

	Status = uefi_call_wrapper(BS->HandleProtocol, 3,
		LoadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid,
		(void **)&SimpleFileSystem);

	if (EFI_ERROR(Status)) {
		Print(L"Failed to get SimpleFileSystem protocol: %r\n", Status);
		return;
	}

	Status = uefi_call_wrapper(SimpleFileSystem->OpenVolume, 2,
		SimpleFileSystem, &Root);
	if (EFI_ERROR(Status)) {
		Print(L"Failed to open volume: %r\n", Status);
		return;
	}

	if (args == NULL || StrLen(args) == 0)
		Path = L".";
	else
		Path = args;

	Status = uefi_call_wrapper(Root->Open, 5, Root, &Dir, Path,
		EFI_FILE_MODE_READ, 0);
	if (EFI_ERROR(Status)) {
		Print(L"Failed to open directory '%s': %r\n", Path, Status);
		goto cleanup;
	}

	BufferSize = SIZE_OF_EFI_FILE_INFO + 512;
	FileInfo = AllocatePool(BufferSize);
	if (FileInfo == NULL) {
		Print(L"Failed to allocate memory to list directory\n");
		goto cleanup;
	}

	Status = uefi_call_wrapper(Dir->Read, 3, Dir, &BufferSize, FileInfo);
	if (EFI_ERROR(Status)) {
		Print(L"Failed to read from '%s': %r\n", Path, Status);
		goto cleanup;
	}

	if (!(FileInfo->Attribute & EFI_FILE_DIRECTORY)) {
		Print(L"<FILE>  %s  %ld bytes\n", Path, FileInfo->FileSize);
		goto cleanup;
	}

	Print(L"Listing directory: %s\n", Path);

	uefi_call_wrapper(Dir->SetPosition, 2, Dir, 0);

	while (TRUE) {
		BufferSize = SIZE_OF_EFI_FILE_INFO + 512;
		Status = uefi_call_wrapper(Dir->Read, 3, Dir, &BufferSize, FileInfo);


		if (EFI_ERROR(Status) || BufferSize == 0)
			break;

		if (StrCmp(FileInfo->FileName, L".") == 0 || StrCmp(FileInfo->FileName, L"..") == 0)
			continue;

		if (FileInfo->Attribute & EFI_FILE_DIRECTORY)
			Print(L"  <DIR>  %s\n", FileInfo->FileName);
		else
			Print(L" <FILE>  %s  %ld bytes\n", FileInfo->FileName, FileInfo->FileSize);
	}

cleanup:
	if (Dir != NULL) {
		while (TRUE) {
			BufferSize = 0;
			Status = uefi_call_wrapper(Dir->Read, 3, Dir, &BufferSize, NULL);
			if (Status == EFI_BUFFER_TOO_SMALL) {
				if (FileInfo != NULL) {
					FreePool(FileInfo);
					FileInfo = NULL;
				}
				FileInfo = AllocatePool(BufferSize);
				if (FileInfo == NULL) {
					Print(L"Failed to allocate buffer for draining\n");
					break;
				}

				Status = uefi_call_wrapper(Dir->Read, 3, Dir, &BufferSize, FileInfo);
				if (EFI_ERROR(Status) || BufferSize == 0)
					break;
			} else if (EFI_ERROR(Status) || BufferSize == 0)
				break;
		}

		if (Dir->Close != NULL) {
			Status = uefi_call_wrapper(Dir->Close, 1, Dir);
			if (EFI_ERROR(Status))
				Print(L"Warning: Failed to close directory pointer: %r\n", Status);
		}

		Dir = NULL;
	}

	if (FileInfo != NULL) {
		FreePool(FileInfo);
		FileInfo = NULL;
	}

	if (Root != NULL && Root != Dir)
		Root = NULL;
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
