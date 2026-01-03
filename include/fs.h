/*
 * HeliumBoot/EFI - A simple UEFI bootloader.
 *
 * Copyright (c) 2026 Stefanos Stefanidis.
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

#ifndef _FS_H_
#define _FS_H_

#include <efi.h>
#include <efilib.h>

#include "s5fs.h"
#include "ufs.h"

typedef EFI_STATUS (*fs_detect_fn)(EFI_BLOCK_IO_PROTOCOL *bio, UINT32 slice_lba, void *sb_buffer);
typedef EFI_STATUS (*fs_mount_fn)(EFI_BLOCK_IO_PROTOCOL *bio, UINT32 slice_lba, void *sb_buffer, void **mount_out);
typedef EFI_STATUS (*fs_list_fn)(void *mount_ctx, const CHAR16 *path);
typedef EFI_STATUS (*fs_umount_fn)(void *mount_ctx);

/*
 * Filesystem table entry structure.
 */
struct fs_tab_entry {
	CHAR16 *fs_name;		// Filesystem name.
	fs_detect_fn detect_fs;	// Filesystem detection function.
	UINTN sb_size;			// Filesystem superblock size.
	fs_mount_fn mount_fs;	// Filesystem mount function.
	fs_list_fn list_dir;	// Filesystem directory listing function.
	fs_umount_fn umount_fs;	// Filesystem umount function.
};

extern struct fs_tab_entry fs_tab[];

#endif /* _FS_H_ */
