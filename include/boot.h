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

#ifndef _BOOT_H_
#define _BOOT_H_

#include "vtoc.h"

enum arg_type {
    CMD_NO_ARGS,
    CMD_OPTIONAL_ARGS,
    CMD_REQUIRED_ARGS
};

struct boot_command_tab {
	CHAR16 *cmd_name;		// Command name.
	void (*cmd_func)(CHAR16 *args);		// Function pointer.
	enum arg_type cmd_arg_type;		// Command type
	CHAR16 *cmd_usage;	// Command usage.
};

typedef EFI_STATUS (*fs_detect_fn)(EFI_BLOCK_IO_PROTOCOL *bio, UINT32 slice_lba, void *sb_buffer);
typedef EFI_STATUS (*fs_mount_fn)(EFI_BLOCK_IO_PROTOCOL *bio, UINT32 slice_lba, void *sb_buffer, void **mount_out);
typedef EFI_STATUS (*fs_list_fn)(void *mount_ctx, const CHAR16 *path);

/*
 * Filesystem table entry structure.
 */
struct fs_tab_entry {
	CHAR16 *fs_name;		// Filesystem name.
	fs_detect_fn detect_fs;	// Filesystem detection function.
	UINTN sb_size;			// Filesystem superblock size.
	fs_mount_fn mount_fs;	// Filesystem mount function.
	fs_list_fn list_dir;	// Filesystem directory listing function.
};

enum FileFormats {
	NONE,
	ELF,
	COFF,
	AOUT,
	PECOFF
};

typedef enum FileFormats lfhdr_t;

/*
 * Boot header.
 */
struct BootHeader {
	lfhdr_t type;		// File type.
	int nsect;		// Number of sections.
#if _LP64
	UINT64 entry;		// Entry point.
#else
	UINT32 entry;		// Entry point.
#endif
};

// Global functions and variables, sorted by filename.

// vers.c
extern const CHAR16 *getversion(void);
extern const CHAR16 *getrevision(void);
extern const CHAR16 *getblddate(void);
extern const CHAR16 *getrevdate(void);
extern const CHAR16 *getcommitno(void);
extern const CHAR16 *getbuildno(void);

// boot.c
extern BOOLEAN exit_flag;
extern EFI_HANDLE gImageHandle;

// helpers.c
extern UINTN StrDecimalToUintn(CHAR16 *str);
extern void SplitCommandLine(CHAR16 *line, CHAR16 **command, CHAR16 **arguments);
extern EFI_STATUS MountAtLba(EFI_BLOCK_IO_PROTOCOL *ParentBlockIo, EFI_LBA StartLba, EFI_SIMPLE_FILE_SYSTEM_PROTOCOL **FileSystem);
extern EFI_STATUS FindPartitionStart(EFI_BLOCK_IO_PROTOCOL *BlockIo, UINT32 *PartitionStart);
extern void HeliumBootPanic(EFI_STATUS Status, const CHAR16 *fmt, ...);
extern UINT32 SwapBytes32(UINT32 val);
extern UINT16 SwapBytes16(UINT16 val);
extern UINT64 GetTimeSeconds(void);
extern EFI_STATUS ReadFile(EFI_FILE_PROTOCOL *File, CHAR16 Buffer, UINTN BufferSize, UINTN *Actual);
extern void *MemMove(void *dst, const void *src, UINTN len);

// loadfile.c
extern EFI_STATUS LoadFile(CHAR16 *args);
extern EFI_STATUS LoadFileEFI(CHAR16 *args);

// commands.c
extern void about(CHAR16 *args);
extern void boot(CHAR16 *args);
extern void boot_efi(CHAR16 *args);
extern void cls(CHAR16 *args);
extern void echo(CHAR16 *args);
extern void exit(CHAR16 *args);
extern void help(CHAR16 *args);
extern void ls(CHAR16 *args);
extern void lsblk(CHAR16 *args);
extern void reboot(CHAR16 *args);
extern void print_revision(CHAR16 *args);
extern void print_version(CHAR16 *args);

// cmd_table.c
extern struct boot_command_tab cmd_tab[];

// fs_table.c
extern struct fs_tab_entry fs_tab[];

// video.c
extern EFI_STATUS InitVideo(void);
extern void PrintToScreen(const CHAR16 *Fmt, ...);
extern void InputToScreen(CHAR16 *Prompt, CHAR16 *InStr, UINTN StrLen);
extern void InitProgressBar(INTN Id, UINTN BarLimit, CONST CHAR8 *Buffer);
extern void UpdateProgressBar(INTN Id, UINTN NewProgress);
extern BOOLEAN VideoInitFlag;

// vtoc.c
extern EFI_STATUS ReadVtoc(struct svr4_vtoc *OutVtoc, EFI_BLOCK_IO_PROTOCOL *BlockIo, UINT32 PartitionStart);

#endif /* _BOOT_H_ */
