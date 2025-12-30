/*
 * HeliumBoot/EFI - A simple UEFI bootloader.
 *
 * Copyright (c) 2025 Stefanos Stefanidis.
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
