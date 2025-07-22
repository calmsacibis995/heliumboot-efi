/*
 * Copyright (c) 2025 Stefanos Stefanidis.
 * All rights reserved.
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

// vers.c
extern const CHAR16 *getversion(void);
extern const CHAR16 *getrevision(void);
extern const CHAR16 *getblddate(void);
extern const CHAR16 *getrevdate(void);
extern const CHAR16 *getcommitno(void);
extern const CHAR16 *getbuildno(void);

// boot.c
extern CHAR16 filepath[100];
extern EFI_FILE_HANDLE RootFS, File;
extern EFI_LOADED_IMAGE *LoadedImage;
extern BOOLEAN exit_flag;
extern EFI_HANDLE gImageHandle;

extern EFI_STATUS FindPartitionStart(EFI_BLOCK_IO_PROTOCOL *BlockIo, UINT32 *PartitionStart);

// loadfile.c
extern EFI_STATUS LoadFile(CHAR16 *args);
extern EFI_STATUS LoadFileEFI(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable, CHAR16 *args);

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

// video.c
extern void InitVideo(void);

// vtoc.c
EFI_STATUS ReadVtoc(struct svr4_vtoc **OutVtoc, EFI_BLOCK_IO_PROTOCOL *BlockIo, UINT32 PartitionStart);

#endif /* _BOOT_H_ */
