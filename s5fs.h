/*
 * Copyright (c) 2025 Stefanos Stefanidis.
 * All rights reserved.
 */

#ifndef _S5FS_H_
#define _S5FS_H_

#include <efi.h>
#include <efilib.h>

#define	NICINOD	100		/* number of superblock inodes */
#define	NICFREE	50		/* number of superblock free blocks */

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

#define FsMAGIC	0xfd187e20	/* s_magic */

#define	SUPERB	((INT32)1)	/* block number of the super block */

#define FsMINBSHIFT	1
#define FsMINBSIZE	512

#define FsBSIZE(bshift)		(FsMINBSIZE << ((bshift) - FsMINBSHIFT))

#define Fs1b	1	/* 512-byte blocks */
#define Fs2b	2	/* 1024-byte blocks */
#define Fs4b	3	/* 2048-byte blocks */

// Public functions

EFI_STATUS DetectS5(EFI_BLOCK_IO_PROTOCOL *BlockIo, UINT32 SliceStartLBA, struct s5_superblock *sb);

#endif /* _S5FS_H_ */
