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

#ifndef _BFS_H_
#define _BFS_H_

/*
 * BFS in-memory superblock structure.
 */
struct bfs_superblock_info {
};

/*
 * BFS on-disk superblock structure.
 */
struct bfs_superblock {
    INT32 bdsup_bfsmagic;		/* Magic number */
	INT32 bdsup_start;		/* Filesystem data start offset */
	INT32 bdsup_end;		/* Filesystem data end offset */

	/*
	 * The next four words are used to promote sanity in compaction.  Used
	 * correctly, a crash at any point during compaction is recoverable
	 */
	INT32 bdcp_fromblock;		/* "From" block of current transfer */
	INT32 bdcp_toblock;		/* "To" block of current transfer */
	INT32 bdcpb_fromblock;	/* Backup of "from" block */
	INT32 bdcpb_toblock;		/* Backup of "to" block */
	INT32 bdsup_filler[121];	/* Padding */
};

#define BFS_MAGIC	0x1BADFACE
#define BFS_SUPEROFF	0
#define BFS_DIRSTART	(BFS_SUPEROFF + sizeof(struct bfs_superblock))
#define BFS_BSIZE	512

// Public functions.

extern EFI_STATUS DetectBFS(EFI_BLOCK_IO_PROTOCOL *BlockIo, UINT32 SliceStartLBA, void *sb_void);
extern EFI_STATUS MountBFS(EFI_BLOCK_IO_PROTOCOL *BlockIo, UINT32 SliceStartLBA, void *sb_buffer, void **mount_out);
extern EFI_STATUS ReadBFSDir(void *mount_ctx, const CHAR16 *path);

#endif /* _BFS_H_ */
