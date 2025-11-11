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

#ifndef _S5FS_H_
#define _S5FS_H_

#include <efi.h>
#include <efilib.h>

#include "vnode.h"

#define	NICINOD	100		/* number of superblock inodes */
#define	NICFREE	50		/* number of superblock free blocks */
#define	DIRSIZ	14

struct s5_dirent {
    UINT16 d_ino;
    INT8 d_name[DIRSIZ];
};

#define	SDSIZ	(sizeof(struct s5_direct))

/*
 * S5 superblock structure.
 */
struct s5_superblock {
    UINT16 s_isize;
    INT32 s_fsize;
    INT16 s_nfree;
    INT32 s_free[NICFREE];
    INT16 s_ninode;
    UINT16 s_inode[NICINOD];
    INT8 s_flock;
    INT8 s_ilock;
    INT8 s_fmod;
    INT8 s_ronly;
    INT32 s_time;
    INT16 s_dinfo[4];
    INT32 s_tfree;
    UINT16 s_tinode;
    INT8 s_fname[6];
    INT8 s_fpack[6];
    INT32 s_fill[12];
    INT32 s_state;
    INT32 s_magic;
    UINT32 s_type;
} __attribute__((packed));

#define	NADDR	13
#define	NSADDR	(NADDR*sizeof(INT32)/sizeof(INT16))

struct s5_inode {
    struct s5_inode *i_forw;    /* forward link */
    struct s5_inode *i_back;    /* backward link */
    struct s5_inode *av_forw;   /* forward link in free list */
    struct s5_inode *av_back;   /* backward link in free list */
    UINT16	i_flag;		/* flags */
	UINT16	i_number;	/* inode number */
	UINT32	i_dev;		/* device where inode resides */
	UINT16  i_mode;     /* file mode and type */
	UINT16	i_uid;		/* owner */
	UINT16	i_gid;		/* group */
	INT16 i_nlink;	/* number of links */
	INT32	i_size;		/* size in bytes */
	INT32	i_atime;	/* last access time */
	INT32	i_mtime;	/* last modification time */
	INT32	i_ctime;	/* last "inode change" time */
	INT32	i_addr[NADDR];	/* block address list */
	INT16	i_nilocks;	/* XXX -- count of recursive ilocks */
	INT16	i_owner;	/* XXX -- proc slot of ilock owner */
	INT32	i_nextr;	/* next byte read offset (read-ahead) */
	UINT16 	i_gen;		/* generation number */
	INT32   i_mapcnt;       /* number of mappings of pages */
	UINT32  i_vcode;	/* version code attribute */
	struct vnode i_vnode;	/* Contains an instance of a vnode */
	INT32	*i_map;		/* block list for the corresponding file */
	UINT32	i_rdev;		/* rdev field for block/char specials */

    INT32 i_mapsz;      /* kmem_alloc'ed size */
    INT32 i_oldsz;      /* i_size when you did the allocation */
};

/*
 * S5 mount private data.
 */
struct s5_mount {
    struct s5_superblock sb;
    UINT32 bsize;
    UINT32 inopb;
    UINT32 nindir;
    UINT32 bshift;
    UINT32 bmask;
    UINT32 nshift;
    UINT32 nmask;
    struct vnode *root_vnode;
    EFI_BLOCK_IO_PROTOCOL *bio;
    UINT32 slice_start_lba;
};

#define FsMAGIC	0xfd187e20	/* s_magic */

#define	SUPERB	((INT32)1)	/* block number of the super block */

#define FsMINBSHIFT	1
#define FsMINBSIZE	512

#define FsBSIZE(bshift)		(FsMINBSIZE << ((bshift) - FsMINBSHIFT))

#define Fs1b	1	/* 512-byte blocks */
#define Fs2b	2	/* 1024-byte blocks */
#define Fs4b	3	/* 2048-byte blocks */

// Public functions

extern EFI_STATUS DetectS5(EFI_BLOCK_IO_PROTOCOL *BlockIo, UINT32 SliceStartLBA, void *sb_void);
extern EFI_STATUS MountS5(EFI_BLOCK_IO_PROTOCOL *BlockIo, UINT32 SliceStartLBA, void *sb_buffer, void **mount_out);
extern EFI_STATUS ReadS5Dir(void *mount_ctx, const CHAR16 *path);

#endif /* _S5FS_H_ */
