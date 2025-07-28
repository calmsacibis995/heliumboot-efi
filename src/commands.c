/*
 * HeliumBoot/EFI - A simple UEFI bootloader.
 *
 * Copyright (c) 2025 Stefanos Stefanidis.
 * All rights reserved.
 *
 * This file is part of HeliumBoot/EFI.
 *
 * HeliumBoot/EFI is free software: you can redistribute it and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * HeliumBoot/EFI is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even
 * the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with HeliumBoot/EFI. If not, see
 * <https://www.gnu.org/licenses/>.
 */

#include <efi.h>
#include <efilib.h>

#include "boot.h"
#include "s5fs.h"

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
 * Boot an exeutable file. Not implemented yet.
 */
void
boot(CHAR16 *args)
{
	EFI_STATUS Status;

	Status = LoadFile(args);
	if (EFI_ERROR(Status))
		Print(L"boot: Failed to boot file (%r)\n", Status);
}

/*
 * Boot an EFI executable file.
 */
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
	EFI_BLOCK_IO_PROTOCOL *BlockIo = NULL;
	EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *SimpleFileSystem = NULL;
	EFI_LOADED_IMAGE *LoadedImage;
	EFI_FILE_PROTOCOL *Root = NULL;
	EFI_FILE_PROTOCOL *Dir = NULL;
	EFI_FILE_INFO *FileInfo = NULL;
	UINTN BufferSize;
	CHAR16 *Path = NULL;
	UINTN DriveIndex = 0, SliceIndex = 0;
	EFI_HANDLE *HandleBuffer = NULL;
	UINTN HandleCount;
	UINT32 PartitionStart = 0;
	struct svr4_vtoc *Vtoc = NULL;
	struct s5_superblock *sb;
	UINT32 SliceLBA;

	if (args && StrnCmp(args, L"sd(", 3) == 0) {
		CHAR16 *p = args + 3;
		UINTN X = 0, Y = 0;

		while (*p >= L'0' && *p <= L'9')
			X = X * 10 + (*p++ - L'0');

		if (*p == L',' || *p == L' ')
			p++;

		while (*p >= L'0' && *p <= L'9')
			Y = Y * 10 + (*p++ - L'0');

		if (*p != L')') {
			Print(L"Invalid sd(X,Y) format\n");
			return;
		}

		p++;
		DriveIndex = X;
		SliceIndex = Y;

		if (*p == L':' && *(p+1) == L'\\')
			Path = p + 2;
		else
			Path = L"\\";
	} else if (args && StrLen(args) > 2 && args[1] == L':' && args[2] == L'\\') {
		DriveIndex = args[0] - L'0';
		Path = &args[3];
	} else {
		Status = uefi_call_wrapper(BS->HandleProtocol, 3, gImageHandle, &LoadedImageProtocol, (void **)&LoadedImage);
		if (EFI_ERROR(Status)) {
			Print(L"Failed to get LoadedImage protocol: %r\n", Status);
			return;
		}

		Status = uefi_call_wrapper(BS->HandleProtocol, 3, LoadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid,
			(void **)&SimpleFileSystem);
		if (EFI_ERROR(Status)) {
			Print(L"Failed to get SimpleFileSystem protocol: %r\n", Status);
			return;
		}

		goto open_volume;
	}

	Status = uefi_call_wrapper(BS->LocateHandleBuffer, 5, ByProtocol, &BlockIoProtocol, NULL, &HandleCount, &HandleBuffer);
	if (EFI_ERROR(Status)) {
		Print(L"Failed to locate block IO handles: %r\n", Status);
		return;
	}

	if (DriveIndex >= HandleCount) {
		Print(L"Invalid drive index %u, max index is %u\n", DriveIndex, HandleCount - 1);
		goto cleanup;
	}

	Status = uefi_call_wrapper(BS->HandleProtocol, 3, HandleBuffer[DriveIndex], &BlockIoProtocol, (void **)&BlockIo);
	if (EFI_ERROR(Status)) {
		Print(L"Failed to get Block I/O protocol: %r\n", Status);
		goto cleanup;
	}

	Status = FindPartitionStart(BlockIo, &PartitionStart);
	if (EFI_ERROR(Status)) {
		Print(L"Error: Cannot find partition start: %r\n", Status);
		goto cleanup;
	}

	Status = ReadVtoc(Vtoc, BlockIo, PartitionStart);
	if (!EFI_ERROR(Status)) {
		if (SliceIndex < Vtoc->v_nparts) {
			SliceLBA = Vtoc->v_part[SliceIndex].p_start;
			Print(L"VTOC slice %u start LBA: %u\n", SliceIndex, SliceLBA);
		} else {
			Print(L"Invalid slice index %u\n", SliceIndex);
			goto cleanup;
		}
	} else {
		Print(L"Failed to open VTOC: %r\n", Status);
		goto cleanup;
	}

	Status = DetectS5(BlockIo, SliceLBA, sb);
	if (EFI_ERROR(Status)) {
		Print(L"Could not read s5 filesystem: %r\n", Status);
		goto cleanup;
	}

open_volume:
	Status = uefi_call_wrapper(SimpleFileSystem->OpenVolume, 2, SimpleFileSystem, &Root);
	if (EFI_ERROR(Status)) {
		Print(L"Failed to open volume: %r\n", Status);
		goto cleanup;
	}

	if (!Path || StrLen(Path) == 0)
		Path = L"\\";

	Status = uefi_call_wrapper(Root->Open, 5, Root, &Dir, Path, EFI_FILE_MODE_READ, 0);
	if (EFI_ERROR(Status)) {
		Print(L"Failed to open directory '%s': %r\n", Path, Status);
		goto cleanup;
	}

	BufferSize = SIZE_OF_EFI_FILE_INFO + 512;
	FileInfo = AllocatePool(BufferSize);
	if (!FileInfo) {
		Print(L"Failed to allocate memory to list directory\n");
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
	if (FileInfo)
		FreePool(FileInfo);

	if (Dir)
		uefi_call_wrapper(Dir->Close, 1, Dir);

	if (HandleBuffer)
		FreePool(HandleBuffer);

	if (Vtoc)
		FreePool(Vtoc);
}

void
lsblk(CHAR16 *args)
{
	EFI_STATUS Status;
	EFI_HANDLE *HandleBuffer = NULL;
	UINTN HandleCount = 0;
	EFI_BLOCK_IO_PROTOCOL *BlockIo = NULL;
	EFI_DEVICE_PATH_PROTOCOL *DevicePath = NULL;
	EFI_GUID BlockIoProtocol = EFI_BLOCK_IO_PROTOCOL_GUID;

	Status = LibLocateHandle(ByProtocol, &BlockIoProtocol, NULL, &HandleCount, &HandleBuffer);
	if (EFI_ERROR(Status)) {
		Print(L"Failed to locate block I/O handles: %r\n", Status);
		return;
	}

	Print(L"Block devices found: %d\n", HandleCount);
	for (UINTN i = 0; i < HandleCount; i++) {
		Status = uefi_call_wrapper(BS->HandleProtocol, 3, HandleBuffer[i], &BlockIoProtocol, (void**)&BlockIo);
		if (EFI_ERROR(Status)) {
			Print(L"Failed to get Block I/O protocol for handle %d: %r\n", i, Status);
			continue;
		}

		Status = uefi_call_wrapper(BS->HandleProtocol, 3, HandleBuffer[i], &gEfiDevicePathProtocolGuid,
			(void **)&DevicePath);
		if (EFI_ERROR(Status))
			DevicePath = NULL;
		Print(L"[%u]: %llu MB %s %s\n", i, (BlockIo->Media->LastBlock + 1) * BlockIo->Media->BlockSize / (1024 * 1024),
			BlockIo->Media->RemovableMedia ? L"(Removable)" : L"(Fixed)", BlockIo->Media->LogicalPartition ? L"(Partition)" : L"(Whole Disk)");
		if (DevicePath) {
			CHAR16 *PathStr = DevicePathToStr(DevicePath);
			Print(L"    Path: %s\n", PathStr);
			FreePool(PathStr);
		}
	}

	uefi_call_wrapper(BS->FreePool, 1, HandleBuffer);
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
