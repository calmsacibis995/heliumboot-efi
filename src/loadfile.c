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

/*
 * Common executable file loading routines.
 */

#include <efi.h>
#include <efilib.h>

#include <elf.h>

#include "aout.h"
#include "boot.h"
#include "disk.h"
#include "fs.h"
#include "vtoc.h"

/*
 * Function:
 * LoadFile()
 *
 * Description:
 * Load a binary file to execute. It can be one of:
 *	- a.out
 *	- COFF
 *	- ELF
 *	- EFI PE/COFF
 *
 * The filesystem types supported are:
 *	- FAT32
 *	- The filesystems listed in 'fs_table.c'. View that file for details.
 *
 * Large portions of code are shared with the 'ls' command. In particular, the VTOC routines,
 * the filesystem handling code, and the block device enumeration routine.
 *
 * Arguments:
 * args: Arguments passed by the 'boot' command.
 *
 * Return value:
 * EFI_SUCCESS on success, any other code on failure.
 */
EFI_STATUS
LoadFile(CHAR16 *args)
{
	EFI_STATUS Status;
	EFI_FILE_HANDLE File, RootFS;
	EFI_LOADED_IMAGE *LoadedImage;
	EFI_BLOCK_IO_PROTOCOL *BlockIo = NULL;
	UINTN ReadSize;
	UINT8 Header[64];
	CHAR16 *Path;
	CHAR16 *ProgArgs = NULL;
	UINTN DriveIndex = 0;
	UINTN SliceIndex = 0;
	UINT32 PartitionStart = 0;
	UINT32 SliceLBA, SectorStart;
	struct svr4_vtoc *Vtoc = NULL;
	struct mbr_partition *Partitions = NULL;
	struct fs_tab_entry *fs_entry_ptr;
	VOID *sb = NULL;
	VOID *mount_ctx = NULL;
	BOOLEAN detected_by_plugin = FALSE;

	// Process sd(x,y) only.
	if (!args || StrnCmp(args, L"sd(", 3) != 0) {
		PrintToScreen(L"Only sd(X,Y)[path] syntax is supported\n");
		return EFI_INVALID_PARAMETER;
	}

	CHAR16 *p = args + 3;
	UINTN X = 0, Y = 0;
	BOOLEAN have_x = FALSE;
	BOOLEAN have_y = FALSE;

	while (*p >= L'0' && *p <= L'9') {
		have_x = TRUE;
		X = X * 10 + (*p++ - L'0');
	}

	if (!have_x || *p != L',') {
		PrintToScreen(L"Invalid sd(X,Y) format\n");
		return EFI_INVALID_PARAMETER;
	}

	p++;

	while (*p >= L'0' && *p <= L'9') {
		have_y = TRUE;
		Y = Y * 10 + (*p++ - L'0');
	}

	if (!have_y || *p != L')') {
		PrintToScreen(L"Invalid sd(X,Y) format\n");
		return EFI_INVALID_PARAMETER;
	}

	p++;

	DriveIndex = X;
	SliceIndex = Y;

	if (SliceIndex > V_NUMPAR - 1) {
		PrintToScreen(L"Invalid slice number %d\n", SliceIndex);
		return EFI_INVALID_PARAMETER;
	}

	/* Split path token and optional program arguments */
	if (*p == L'\0') {
		Path = L"\\";
		ProgArgs = NULL;
	} else {
		CHAR16 *q = p;
		/* find end of path token (space/tab separate args) */
		while (*q != L'\0' && *q != L' ' && *q != L'\t')
			q++;
		if (*q != L'\0') {
			*q++ = L'\0'; /* terminate path */
			while (*q == L' ' || *q == L'\t')
				q++;
			ProgArgs = (*q != L'\0') ? q : NULL;
		}

		if (*p == L'\\')
			Path = p;
		else {
			static CHAR16 PathBuf[256];
			PathBuf[0] = L'\\';
			StrnCpy(PathBuf + 1, p, ARRAY_SIZE(PathBuf) - 2);
			PathBuf[ARRAY_SIZE(PathBuf) - 1] = L'\0';
			Path = PathBuf;
		}
	}

	Status = uefi_call_wrapper(gBS->HandleProtocol, 3, gImageHandle, &gEfiLoadedImageProtocolGuid, (VOID **)&LoadedImage);
	if (EFI_ERROR(Status)) {
		PrintToScreen(L"Cannot get loaded image protocol: %r\n", Status);
		return Status;
	}

	Status = uefi_call_wrapper(gBS->HandleProtocol, 3, LoadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (VOID **)&RootFS);
	if (EFI_ERROR(Status)) {
		PrintToScreen(L"Cannot get Simple File System protocol: %r\n", Status);
		return Status;
	}

	Status = GetWholeDiskByIndex(DriveIndex, &BlockIo);
	if (EFI_ERROR(Status)) {
		PrintToScreen(L"Cannot get whole disk by index: %r\n", Status);
		return Status;
	}

	if (BlockIo->Media->RemovableMedia && !BlockIo->Media->LogicalPartition)
		goto open_volume;

	Partitions = AllocateZeroPool(sizeof(struct mbr_partition) * 4);
    if (!Partitions) {
        PrintToScreen(L"Failed to allocate memory for partition table\n");
        goto cleanup;
    }

	Status = GetPartitionData(BlockIo, Partitions);
	if (EFI_ERROR(Status)) {
		PrintToScreen(L"Error: Cannot get partition data: %r\n", Status);
		goto cleanup;
	}

	Status = FindSysVPartition(Partitions, &PartitionStart);
	if (EFI_ERROR(Status)) {
		PrintToScreen(L"No System V partition detected.\n");
		goto cleanup;
	}

	Vtoc = AllocateZeroPool(sizeof(struct svr4_vtoc));
    if (!Vtoc) {
        PrintToScreen(L"Failed to allocate memory for VTOC\n");
        goto cleanup;
    }

	Status = ReadVtoc(Vtoc, BlockIo, PartitionStart);
	if (!EFI_ERROR(Status)) {
		if (SliceIndex < Vtoc->v_nparts) {
			SectorStart = Vtoc->v_part[SliceIndex].p_start;
			SliceLBA = SectorStart;
			switch (Vtoc->v_part[SliceIndex].p_tag) {
				case V_NOSLICE:
					PrintToScreen(L"VTOC slice %u is unassigned.\n", SliceIndex);
					goto cleanup;
				case V_BOOT:
					PrintToScreen(L"VTOC slice %u, starting sector: %u (master boot record)\n", SliceIndex, SectorStart);
					goto cleanup;
				case V_ROOT:
					PrintToScreen(L"VTOC slice %u, starting sector: %u (rootfs)\n", SliceIndex, SectorStart);
					break;
				case V_SWAP:
					PrintToScreen(L"VTOC slice %u, starting sector: %u (swap space)\n", SliceIndex, SectorStart);
					goto cleanup;
				case V_USR:
					PrintToScreen(L"VTOC slice %u, starting sector: %u (/usr)\n", SliceIndex, SectorStart);
					break;
				case V_BACKUP:
					PrintToScreen(L"VTOC slice %u, starting sector: %u (backup)\n", SliceIndex, SectorStart);
					PrintToScreen(L"This is a placeholder for the entire drive.\n", SliceIndex, SectorStart);
					goto cleanup;
				case V_ALTS:
					PrintToScreen(L"VTOC slice %u, starting sector: %u (alternate sector space)\n", SliceIndex, SectorStart);
					goto cleanup;
				case V_OTHER:
					PrintToScreen(L"VTOC slice %u, starting sector: %u (Non-SysV)\n", SliceIndex, SectorStart);
					goto cleanup;
				case V_ALTTRK:
					PrintToScreen(L"VTOC slice %u, starting sector: %u (alternate track space)\n", SliceIndex, SectorStart);
					goto cleanup;
				case V_STAND:
					PrintToScreen(L"VTOC slice %u, starting sector: %u (/stand)\n", SliceIndex, SectorStart);
					break;
				case V_VAR:
					PrintToScreen(L"VTOC slice %u, starting sector: %u (/var)\n", SliceIndex, SectorStart);
					break;
				case V_HOME:
					PrintToScreen(L"VTOC slice %u, starting sector: %u (/home)\n", SliceIndex, SectorStart);
					break;
				case V_DUMP:
					PrintToScreen(L"VTOC slice %u, starting sector: %u (dump)\n", SliceIndex, SectorStart);
					goto cleanup;
				default:
					PrintToScreen(L"No valid VTOC slice %u (contains invalid slice tag %d)\n", SliceIndex, Vtoc->v_part[SliceIndex].p_tag);
					goto cleanup;
			}
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
		if (EFI_ERROR(Status)) {
			FreePool(sb);
			sb = NULL;
			continue;
		}

		PrintToScreen(L"Detected filesystem: %s\n", fs_entry_ptr->fs_name);

		if (fs_entry_ptr->mount_fs) {
			Status = fs_entry_ptr->mount_fs(BlockIo, SliceLBA, sb, &mount_ctx);
			if (EFI_ERROR(Status)) {
				PrintToScreen(L"Failed to mount %s: %r\n", fs_entry_ptr->fs_name, Status);
				FreePool(sb);
				sb = NULL;
				continue;
			}
		}

		FreePool(sb);
		sb = NULL;
		detected_by_plugin = TRUE;

		if (fs_entry_ptr->open && mount_ctx) {
			if (!Path) {
                PrintToScreen(L"No file path specified!\n");
                break;
            }
			/* fs_open_file_fn should return an EFI_FILE_HANDLE in file_out */
			Status = fs_entry_ptr->open(mount_ctx, Path, EFI_FILE_MODE_READ, (void **)&File);
			if (EFI_ERROR(Status)) {
				PrintToScreen(L"Failed to open file: %r\n", Status);
			} else {
				/* Read header for format detection like open_volume path */
				ReadSize = sizeof(Header);
				Status = uefi_call_wrapper(File->Read, 3, File, &ReadSize, Header);
				if (EFI_ERROR(Status)) {
					PrintToScreen(L"Cannot read file %s: %r\n", Path, Status);
					uefi_call_wrapper(File->Close, 1, File);
					File = NULL;
				} else {
					uefi_call_wrapper(File->SetPosition, 2, File, 0);
				}
			}
        }

		PrintToScreen(L"Loaded file: %s\n", Path);

		if (fs_entry_ptr->umount_fs && mount_ctx) {
			fs_entry_ptr->umount_fs(mount_ctx);
			mount_ctx = NULL;
		}

		if (detected_by_plugin)
			break;
	}

	// A filesystem was detected and the executable was loaded by a plugin.
	// Clean up and exit if there wasn't one.
	if (detected_by_plugin)
		goto check_exec;
	else {
		PrintToScreen(L"No supported filesystem found at sd(%d,%d)\n", DriveIndex, SliceIndex);
		goto cleanup;
	}

open_volume:
	Status = uefi_call_wrapper(RootFS->Open, 5, RootFS, &File, Path, EFI_FILE_MODE_READ, 0);
	if (EFI_ERROR(Status)) {
		PrintToScreen(L"Cannot open file %s: %r\n", Path, Status);
		return Status;
	}

	ReadSize = sizeof(Header);
	Status = uefi_call_wrapper(File->Read, 3, File, &ReadSize, Header);
	if (EFI_ERROR(Status)) {
		PrintToScreen(L"Cannot read file %s: %r\n", Path, Status);
		uefi_call_wrapper(File->Close, 1, File);
		return Status;
	}

	uefi_call_wrapper(File->SetPosition, 2, File, 0);

check_exec:
	if (IsAOut(Header)) {
		Status = LoadAOutBinary(File);
		if (EFI_ERROR(Status)) {
			PrintToScreen(L"Failed to load a.out file %s: %r\n", Status);
			return Status;
		}
	} else if (IsElf64(Header)) {
		Status = LoadElfBinary(gImageHandle, File);
		if (EFI_ERROR(Status)) {
			PrintToScreen(L"Failed to load ELF file %s: %r\n", Status);
			return Status;
		}
	} else if (IsEfiBinary(Header)) {
		/* Pass ProgArgs to the EFI loader so it can set LoadOptions */
		Status = LoadEfiBinary(Path, LoadedImage, LoadedImage->DeviceHandle, ProgArgs);
        if (EFI_ERROR(Status)) {
			PrintToScreen(L"Failed to load EFI binary %s: %r\n", Path, Status);
			return Status;
        }
     } else {
		PrintToScreen(L"Unknown binary format\n");
		return EFI_UNSUPPORTED;
	}

cleanup:
	if (File)
		uefi_call_wrapper(File->Close, 1, File);

	return EFI_SUCCESS;
}
