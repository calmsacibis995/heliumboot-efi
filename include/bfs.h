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

/*
 * bfs.h
 * SVR4 BFS filesystem structures and definitions.
 */

#ifndef _BFS_H_
#define _BFS_H_

#include "vnode.h"

#define BFS_MAXFNLEN 14			/* Maximum file length */
#define BFS_MAXFNLENN (BFS_MAXFNLEN+1)  /* Used for NULL terminated copies */

/*
 * BFS superblock structure on disk.
 */
struct bfs_superblock {
    INT32 bdsup_bfsmagic;		/* Magic number */
	INT32 bdsup_start;		/* Filesystem data start offset */
	INT32 bdsup_end;		/* Filesystem data end offset */

	/*
	 * The next four words are used to promote sanity in compaction.  Used
	 * correctly, a crash at any point during compaction is recoverable.
	 */
	INT32 bdcp_fromblock;		/* "From" block of current transfer */
	INT32 bdcp_toblock;		/* "To" block of current transfer */
	INT32 bdcpb_fromblock;	/* Backup of "from" block */
	INT32 bdcpb_toblock;		/* Backup of "to" block */
	INT32 bdsup_filler[121];	/* Padding */
};

/*
 * We keep a linked list of all referenced BFS vnodes.  bfs_inactive will remove
 * them from the list, and bfs_fillvnode will add to and search through the list.
 */
struct bfs_core_vnode {
	struct vnode *core_vnode;
	struct bfs_core_vnode *core_next;
};

/*
 * BFS private data.
 */
struct bfs_private {
    INT32 bsup_start;		/* The filesystem data start offset */
	INT32 bsup_end;			/* The filesystem data end offset */
	INT32 bsup_freeblocks;		/* # of freeblocks (for statfs) */
	INT32 bsup_freedrents;		/* # of free dir entries (for statfs) */
	struct vnode *bsup_devnode;	/* The device special vnode */
	struct vnode *bsup_root;	/* Root vnode */
	INT32 bsup_lastfile;		/* Last file directory offset */

	INT32 bsup_inomapsz;
	INT8 *bsup_inomap;

	/* Linked vnode list */

	struct bfs_core_vnode *bsup_incore_vlist;	

	/*
	 * bsup_ioinprog is the count of the number of io operations is 
	 * in progress.  Compaction routines sleep on this being zero
	 */
	UINT16 bsup_ioinprog;
	struct vnode *bsup_writelock;	/* The file which is open for write */

	/* Booleans */

	UINT8 bsup_fslocked;	/* Fs is locked when compacting */
	UINT8 bsup_compacted;	/* Fs compacted, no removes done */
};

struct bfsvattr {
	vtype_t va_type;	/* vnode type (for create) */
	UINT32 va_mode;	/* file access mode */
	INT32 va_uid;		/* owner user id */
	INT32 va_gid;		/* owner group id */
	UINT32 va_nlink;	/* number of references to file */
	INT32 va_atime;	/* time of last access */
	INT32 va_mtime;	/* time of last modification */
	INT32 va_ctime;	/* time file ``created'' */
	INT32 va_filler[4];	/* padding */
};

/*
 * The bfs_dirent is the "inode" of BFS.  Always on disk, it is pointed
 * to (by disk offset) by the vnode and is referenced every time an
 * operation is done on the vnode.  It must be referenced every time,
 * as things can move around very quickly.
 */
struct bfs_dirent {
	UINT16 d_ino;				/* inode */
	INT32 d_sblock;			/* Start block */
	INT32 d_eblock;			/* End block */
	INT32 d_eoffset;			/* EOF disk offset (absolute) */
	struct bfsvattr d_fattr;		/* File attributes */
};

struct bfs_ldirs {
	UINT16 l_ino;
	INT8 l_name[BFS_MAXFNLEN];
};

/*
 * BFS mount private data.
 */
struct bfs_mount {
    struct bfs_superblock bfs_sb;
    struct bfs_private bfs_private;
    struct vnode *bfs_root_vnode;
    EFI_BLOCK_IO_PROTOCOL *bio;
    UINT32 slice_start_lba;
};

#define RLIM_INFINITY   0x7fffffff

#define BFS_MAGIC   0x1BADFACE
#define BFS_SUPEROFF	0
#define BFS_DIRSTART    (BFS_SUPEROFF + sizeof(struct bfs_superblock))
#define BFS_DEVNODE(vfsp)   (vfsp->bfs_private->bsup_devnode)
#define BFS_BSIZE   512
#define BFS_ULT		RLIM_INFINITY	/* file size limit not enforced */
#define BFS_YES		(char)1
#define BFS_NO		(char)0
#define CHUNKSIZE	4096
#define BIGFILE		500
#define SMALLFILE	10
#define BFSROOTINO	2
#define DIRBUFSIZE	1024

#define BFS_OFF2INO(offset) \
	((offset - BFS_DIRSTART) / sizeof(struct bfs_dirent)) + BFSROOTINO

#define BFS_INO2OFF(inode) \
	((inode - BFSROOTINO) * sizeof(struct bfs_dirent)) + BFS_DIRSTART

#define BFS_SANITYWSTART (BFS_SUPEROFF + (sizeof(INT32) * 3))

extern EFI_STATUS DetectBFS(EFI_BLOCK_IO_PROTOCOL *BlockIo, UINT32 SliceStartLBA, void *sb_void);
extern EFI_STATUS MountBFS(EFI_BLOCK_IO_PROTOCOL *BlockIo, UINT32 SliceStartLBA, void *sb_buffer, void **mount_out);
extern EFI_STATUS ReadBFSDir(void *mount_ctx, const CHAR16 *path);
extern EFI_STATUS UmountBFS(void *mount);
extern EFI_STATUS OpenBFS(void *mount_ctx, const CHAR16 *filename, UINTN mode, void **file_out);

#endif /* _BFS_H_ */
