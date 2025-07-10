#ifndef _BOOT_H_
#define _BOOT_H_

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
extern const CHAR16 version[];
extern const CHAR16 revision[];
extern const CHAR16 blddate[];

// boot.c
extern CHAR16 filepath[100];
extern EFI_FILE_HANDLE RootFS, File;
extern EFI_LOADED_IMAGE *LoadedImage;
extern BOOLEAN exit_flag;
extern EFI_HANDLE gImageHandle;

// loadfile.c
extern EFI_STATUS LoadFile(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable, CHAR16 *args);

// commands.c
extern void boot(CHAR16 *args);
extern void boot_efi(CHAR16 *args);
extern void cls(CHAR16 *args);
extern void echo(CHAR16 *args);
extern void exit(CHAR16 *args);
extern void help(CHAR16 *args);
extern void ls(CHAR16 *args);
extern void reboot(CHAR16 *args);
extern void print_revision(CHAR16 *args);
extern void print_version(CHAR16 *args);

// cmd_table.c
extern struct boot_command_tab cmd_tab[];

#endif /* _BOOT_H_ */
