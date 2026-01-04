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

#ifndef _S5FS_H_
#define _S5FS_H_

#include <efi.h>
#include <efilib.h>

#include <assert.h>

#include "vnode.h"

#define	NICINOD	100		/* number of superblock inodes */
#define	NICFREE	50		/* number of superblock free blocks */
#define	DIRSIZ	14

struct s5_direct {
    UINT16 d_ino;
    INT8 d_name[DIRSIZ];
};

#define	SDSIZ	(sizeof(struct s5_direct))

/*
 * S5 superblock structure.
 */
struct s5_superblock {
    UINT16	s_isize;	/* size in blocks of i-list */
	INT32	s_fsize;	/* size in blocks of entire volume */
	INT16	s_nfree;	/* number of addresses in s_free */
	INT32	s_free[NICFREE];/* free block list */
	INT16	s_ninode;	/* number of i-nodes in s_inode */
	UINT16	s_inode[NICINOD];/* free i-node list */
	INT8	s_flock;	/* lock during free list manipulation */
	INT8	s_ilock;	/* lock during i-list manipulation */
	INT8  	s_fmod; 	/* super block modified flag */
	INT8	s_ronly;	/* mounted read-only flag */
	INT32	s_time; 	/* last super block update */
	INT16	s_dinfo[4];	/* device information */
	INT32	s_tfree;	/* total free blocks*/
	UINT16	s_tinode;	/* total free inodes */
	INT8	s_fname[6];	/* file system name */
	INT8	s_fpack[6];	/* file system pack name */
	INT32	s_fill[12];	/* adjust to make sizeof filsys */
	INT32	s_state;	/* file system state */
	INT32	s_magic;	/* magic number to indicate new file system */
	UINT32	s_type;		/* type of new file system */
};

static_assert(sizeof(struct s5_superblock) == 512);

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
	INT32	*i_map;		/* block list for the corresponding file (unused for UEFI) */
	UINT32	i_rdev;		/* rdev field for block/char specials */

    INT32 i_mapsz;      /* kmem_alloc'ed size */
    INT32 i_oldsz;      /* i_size when you did the allocation */
};

#define S5ROOTINO   2

/*
 * S5 mount private data.
 */
struct s5_mount {
    struct s5_superblock sb;
    UINT32 bsize;
	UINT32 bshift;
	UINT32 bmask;
    UINT32 inopb;
	UINT32 inoshift;
    UINT32 nindir;
    UINT32 nshift;
    UINT32 nmask;
    struct vnode *root_vnode;
    EFI_BLOCK_IO_PROTOCOL *bio;
    UINT32 slice_start_lba;
};

struct s5_dinode {
    UINT16	di_mode;	/* mode and type of file */
	INT16	di_nlink;    	/* number of links to file */
	UINT16	di_uid;      	/* owner's user id */
	UINT16	di_gid;      	/* owner's group id */
	INT32	di_size;     	/* number of bytes in file */
	INT8  	di_addr[39];	/* disk block addresses */
	UINT8	di_gen;		/* file generation number */
	INT32	di_atime;   	/* time last accessed */
	INT32	di_mtime;   	/* time last modified */
	INT32	di_ctime;   	/* time created */
} __attribute__((packed));

static_assert(sizeof(struct s5_dinode) == 64);

#define FsMAGIC	0xfd187e20	/* s_magic */

#define	SUPERB	((INT32)1)	/* block number of the super block */

#define FsMINBSHIFT	1
#define FsMINBSIZE	512

#define FsBSIZE(bshift)		(FsMINBSIZE << ((bshift) - FsMINBSHIFT))

#define Fs1b	1	/* 512-byte blocks */
#define Fs2b	2	/* 1024-byte blocks */
#define Fs4b	3	/* 2048-byte blocks */

#define FsMAXBSIZE	8192

#define SUPERBOFF	512

// Macro functions

#define FsITOD(fs, x)	(INT32)(((UINT32)(x)+(2*(fs)->inopb-1)) >> (fs)->inoshift)
#define FsITOO(fs, x)	(INT32)(((UINT32)(x)+(2*(fs)->inopb-1)) & ((fs)->inopb-1))

// Public functions

extern EFI_STATUS DetectS5(EFI_BLOCK_IO_PROTOCOL *BlockIo, UINT32 SliceStartLBA, void *sb_void);
extern EFI_STATUS MountS5(EFI_BLOCK_IO_PROTOCOL *BlockIo, UINT32 SliceStartLBA, void *sb_buffer, void **mount_out);
extern EFI_STATUS ReadS5Dir(void *mount_ctx, const CHAR16 *path);
extern EFI_STATUS UmountS5(void *mount);

#endif /* _S5FS_H_ */
