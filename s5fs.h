/*
 * Copyright (c) 2025 Stefanos Stefanidis.
 * All rights reserved.
 */

#ifndef _S5FS_H_
#define _S5FS_H_

#include <efi.h>
#include <efilib.h>

/*
 * S5 superblock structure.
 */
struct s5_superblock {
};

#define FsMAGIC	0xfd187e20	/* s_magic */

#define FsMINBSHIFT	1
#define FsMINBSIZE	512

#define FsBSIZE(bshift)		(FsMINBSIZE << ((bshift) - FsMINBSHIFT))

#define Fs1b	1	/* 512-byte blocks */
#define Fs2b	2	/* 1024-byte blocks */
#define Fs4b	3	/* 2048-byte blocks */

#endif /* _S5FS_H_ */
