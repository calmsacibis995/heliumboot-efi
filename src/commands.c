/*
 * HeliumBoot/EFI - A simple UEFI bootloader.
 *
 * Copyright (c) 2025, 2026 Stefanos Stefanidis.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <efi.h>
#include <efilib.h>

#include "boot.h"
#include "s5fs.h"

void
about(CHAR16 *args)
{
	PrintToScreen(L"HeliumBoot/EFI - The HeliumOS boot loader\n");
	PrintToScreen(L"Copyright (c) 2025, 2026 Stefanos Stefanidis.\n");
	PrintToScreen(L"All rights reserved.\n");
	PrintToScreen(L"Version %s\n", getbuildno());
	PrintToScreen(L"Internal revision %s, %s\n", getrevision(), getrevdate());
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
		PrintToScreen(L"boot: Failed to boot file (%r)\n", Status);
}

void
cls(CHAR16 *args)
{
	ClearScreen();
}

void
echo(CHAR16 *args)
{
	PrintToScreen(L"%s\n", args);
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

	PrintToScreen(L"The commands supported by HeliumBoot are:\n");
	for (cmd = cmd_tab; cmd->cmd_name != NULL; cmd++)
		PrintToScreen(L"  %s\n", cmd->cmd_usage);
	PrintToScreen(L"\n");
}

void
hinv(CHAR16 *args)
{
	UINT64 MemBytes = GetTotalMemoryBytes();
	CHAR16 *ScreenInfo = GetScreenInfo();

	PrintToScreen(L"\tHardware Inventory:\n\n");
	PrintToScreen(L"\tFirmware:               %s (%d.%d)\n", ST->FirmwareVendor,
		ST->FirmwareRevision >> 16, ST->FirmwareRevision & ((1 << 16) - 1));
	PrintToScreen(L"\tEFI Revision:           %d.%d\n", ST->Hdr.Revision >> 16,
		ST->Hdr.Revision & ((1 << 16) - 1));
#if defined(IA32_BLD)
	PrintToScreen(L"\tPlatform:               32-bit\n");
#elif defined(X86_64_BLD) || defined(AARCH64_BLD) || defined(RISCV64_BLD)
	PrintToScreen(L"\tPlatform:               64-bit\n");
#else
	PrintToScreen(L"\tPlatform:               unknown\n");
#endif
#if defined(X86_64_BLD)
	CHAR8 CpuNameA[49];
	CHAR16 CpuName[49];
	UINT32 *p = (UINT32 *)CpuNameA;
	UINTN i;

	AsmCpuid(0x80000002, 0, &p[0], &p[1], &p[2], &p[3]);
	AsmCpuid(0x80000003, 0, &p[4], &p[5], &p[6], &p[7]);
	AsmCpuid(0x80000004, 0, &p[8], &p[9], &p[10], &p[11]);

	CpuNameA[48] = '\0';
	for (i = 0; i < 49; i++)
		CpuName[i] = (CHAR16)CpuNameA[i];
	PrintToScreen(L"\tProcessor:              %s\n", CpuName);
#endif
	PrintToScreen(L"\tInstalled memory:       %d MB\n", MemBytes / (1024 * 1024));
	PrintToScreen(L"\tScreen Output:          %s\n", ScreenInfo);

	// GetScreenInfo() allocates memory. Free it.
	FreePool(ScreenInfo);
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
	VOID *sb = NULL;
	UINT32 SliceLBA;
	struct fs_tab_entry *fs_entry_ptr;
	VOID *mount_ctx = NULL;
	BOOLEAN detected_by_plugin = FALSE;
	struct mbr_partition *Partitions = NULL;

	// Process sd(x,y). We also support X:\, where X is a UEFI drive or partition number.
	// sd(x,y) should be used by disks with VTOC, X:\ by everything else.
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
			PrintToScreen(L"Invalid sd(X,Y) format\n");
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
init_simplefs:
		Status = uefi_call_wrapper(BS->HandleProtocol, 3, gImageHandle, &LoadedImageProtocol, (void **)&LoadedImage);
		if (EFI_ERROR(Status)) {
			PrintToScreen(L"Failed to get LoadedImage protocol: %r\n", Status);
			return;
		}

		Status = uefi_call_wrapper(BS->HandleProtocol, 3, LoadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid,
			(void **)&SimpleFileSystem);
		if (EFI_ERROR(Status)) {
			PrintToScreen(L"Failed to get SimpleFileSystem protocol: %r\n", Status);
			return;
		}

		goto open_volume;
	}

	Status = uefi_call_wrapper(BS->LocateHandleBuffer, 5, ByProtocol, &BlockIoProtocol, NULL, &HandleCount, &HandleBuffer);
	if (EFI_ERROR(Status)) {
		PrintToScreen(L"Failed to locate block I/O handles: %r\n", Status);
		return;
	}

	if (DriveIndex >= HandleCount) {
		PrintToScreen(L"Invalid drive index %u, max index is %u\n", DriveIndex, HandleCount - 1);
		goto cleanup;
	}

	Status = uefi_call_wrapper(BS->HandleProtocol, 3, HandleBuffer[DriveIndex], &BlockIoProtocol, (void **)&BlockIo);
	if (EFI_ERROR(Status)) {
		PrintToScreen(L"Failed to get Block I/O protocol: %r\n", Status);
		goto cleanup;
	}

	// Skip disk-specific stuff and use Simple File System protocol if we are booting from CD/DVD.
	if (BlockIo->Media->RemovableMedia && !BlockIo->Media->LogicalPartition)
		goto init_simplefs;

	Status = GetPartitionData(BlockIo, Partitions);
	if (EFI_ERROR(Status)) {
		PrintToScreen(L"Error: Cannot get partition data: %r\n", Status);
		goto init_simplefs;		// Fallback to native EFI API on failure.
	}

	Status = FindSysVPartition(Partitions, &PartitionStart);
	if (EFI_ERROR(Status)) {
		PrintToScreen(L"No System V partition detected.\n");
		goto init_simplefs;		// Fallback to native EFI API on failure.
	}

	Status = ReadVtoc(Vtoc, BlockIo, PartitionStart);
	if (!EFI_ERROR(Status)) {
		if (SliceIndex < Vtoc->v_nparts) {
			SliceLBA = Vtoc->v_part[SliceIndex].p_start;
			PrintToScreen(L"VTOC slice %u start LBA: %u\n", SliceIndex, SliceLBA);
		} else {
			PrintToScreen(L"Invalid slice index %u\n", SliceIndex);
			goto cleanup;
		}
	} else {
		PrintToScreen(L"Failed to read VTOC: %r\n", Status);
		goto cleanup;
	}

	// Run through all supported filesystems to detect one.
	for (fs_entry_ptr = fs_tab; fs_entry_ptr && fs_entry_ptr->fs_name != NULL; fs_entry_ptr++) {
		if (fs_entry_ptr->sb_size == 0 || fs_entry_ptr->detect_fs == NULL)
			continue;

		sb = AllocateZeroPool(fs_entry_ptr->sb_size);
		if (!sb) {
			PrintToScreen(L"Failed to allocate memory for superblock\n");
			continue;
		}

		Status = fs_entry_ptr->detect_fs(BlockIo, SliceLBA, sb);
		if (!EFI_ERROR(Status)) {
			PrintToScreen(L"Detected filesystem: %s\n", fs_entry_ptr->fs_name);
			if (fs_entry_ptr->mount_fs) {
				Status = fs_entry_ptr->mount_fs(BlockIo, SliceLBA, sb, &mount_ctx);
				if (EFI_ERROR(Status)) {
					PrintToScreen(L"Failed to mount %s: %r\n", fs_entry_ptr->fs_name, Status);
					FreePool(sb);
					sb = NULL;
					mount_ctx = NULL;
					continue;
				}
			}

			if (fs_entry_ptr->list_dir && mount_ctx != NULL) {
				if (!Path || StrLen(Path) == 0)
					Path = L"\\";
				Status = fs_entry_ptr->list_dir(mount_ctx, Path);
				if (EFI_ERROR(Status)) {
					PrintToScreen(L"Failed to list directory %s: %r\n", Path, Status);
					FreePool(sb);
					sb = NULL;
					mount_ctx = NULL;
					continue;
				}
				detected_by_plugin = TRUE;
			} else {
				detected_by_plugin = FALSE;
			}

			break;
		}

		FreePool(sb);
		sb = NULL;
	}

	// A filesystem was detected and listed by a plugin. Clean up and exit.
	if (detected_by_plugin)
		goto cleanup;

open_volume:
	Status = uefi_call_wrapper(SimpleFileSystem->OpenVolume, 2, SimpleFileSystem, &Root);
	if (EFI_ERROR(Status)) {
		PrintToScreen(L"Failed to open volume: %r\n", Status);
		goto cleanup;
	}

	if (!Path || StrLen(Path) == 0)
		Path = L"\\";

	Status = uefi_call_wrapper(Root->Open, 5, Root, &Dir, Path, EFI_FILE_MODE_READ, 0);
	if (EFI_ERROR(Status)) {
		PrintToScreen(L"Failed to open directory '%s': %r\n", Path, Status);
		goto cleanup;
	}

	BufferSize = SIZE_OF_EFI_FILE_INFO + 512;
	FileInfo = AllocatePool(BufferSize);
	if (!FileInfo) {
		PrintToScreen(L"Failed to allocate memory to list directory\n");
		goto cleanup;
	}

	PrintToScreen(L"Listing directory: %s\n", Path);
	uefi_call_wrapper(Dir->SetPosition, 2, Dir, 0);

	while (TRUE) {
		BufferSize = SIZE_OF_EFI_FILE_INFO + 512;
		Status = uefi_call_wrapper(Dir->Read, 3, Dir, &BufferSize, FileInfo);
		if (EFI_ERROR(Status) || BufferSize == 0)
			break;
		if (StrCmp(FileInfo->FileName, L".") == 0 || StrCmp(FileInfo->FileName, L"..") == 0)
			continue;
		if (FileInfo->Attribute & EFI_FILE_DIRECTORY)
			PrintToScreen(L"  <DIR>  %s\n", FileInfo->FileName);
		else
			PrintToScreen(L" <FILE>  %s  %ld bytes\n", FileInfo->FileName, FileInfo->FileSize);
	}

cleanup:
	if (sb)
		FreePool(sb);

	if (FileInfo)
		FreePool(FileInfo);

	if (Dir)
		uefi_call_wrapper(Dir->Close, 1, Dir);

	if (Root)
		uefi_call_wrapper(Root->Close, 1, Root);

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
		PrintToScreen(L"Failed to locate block I/O handles: %r\n", Status);
		return;
	}

	PrintToScreen(L"Block devices found: %d\n", HandleCount);
	for (UINTN i = 0; i < HandleCount; i++) {
		Status = uefi_call_wrapper(BS->HandleProtocol, 3, HandleBuffer[i], &BlockIoProtocol, (void**)&BlockIo);
		if (EFI_ERROR(Status)) {
			PrintToScreen(L"Failed to get Block I/O protocol for handle %d: %r\n", i, Status);
			continue;
		}

		Status = uefi_call_wrapper(BS->HandleProtocol, 3, HandleBuffer[i], &gEfiDevicePathProtocolGuid,
			(void **)&DevicePath);
		if (EFI_ERROR(Status))
			DevicePath = NULL;
		PrintToScreen(L"[%u]: %llu MB %s %s\n", i, (BlockIo->Media->LastBlock + 1) * BlockIo->Media->BlockSize / (1024 * 1024),
			BlockIo->Media->RemovableMedia ? L"(Removable)" : L"(Fixed)", BlockIo->Media->LogicalPartition ? L"(Partition)" : L"(Whole Disk)");
		if (DevicePath) {
			CHAR16 *PathStr = DevicePathToStr(DevicePath);
			PrintToScreen(L"    Path: %s\n", PathStr);
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
		PrintToScreen(L"Reset failed: %r\n", Status);
}

void
print_revision(CHAR16 *args)
{
	PrintToScreen(L"Internal revision %s, %s\n", getrevision(), getrevdate());
}

void
print_version(CHAR16 *args)
{
	PrintToScreen(L"%s\n", getversion());
}
