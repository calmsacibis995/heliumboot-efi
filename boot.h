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

extern struct boot_command_tab cmd_tab[];
extern CHAR16 filepath[100];
extern EFI_FILE_HANDLE RootFS, File;
extern EFI_LOADED_IMAGE *LoadedImage;
extern const CHAR16 version[];
extern const CHAR16 revision[];
extern const CHAR16 blddate[];

// loadfile.c
extern EFI_STATUS LoadFile(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable);

// commands.c
extern void echo(CHAR16 *args);
extern void ls(CHAR16 *args);
extern void print_version(CHAR16 *args);

#endif /* _BOOT_H_ */
