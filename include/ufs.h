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

#ifndef _UFS_H_
#define _UFS_H_

#include <efi.h>
#include <efilib.h>

/*
 * Cylinder group related limits.
 *
 * For each cylinder we keep track of the availability of blocks at different
 * rotational positions, so that we can lay out the data to be picked
 * up with minimum rotational latency.  NRPOS is the number of rotational
 * positions which we distinguish.  With NRPOS 8 the resolution of our
 * summary information is 2ms for a typical 3600 rpm drive.
 */
#define NRPOS           8       /* number distinct rotational positions */

#define MAXCPG          32      /* maximum fs_cpg */

/*
 * The path name on which the file system is mounted is maintained
 * in fs_fsmnt. MAXMNTLEN defines the amount of space allocated in 
 * the super block for this name.
 * The limit on the amount of summary information per file system
 * is defined by MAXCSBUFS. It is currently parameterized for a
 * maximum of two million cylinders.
 */
#define MAXMNTLEN 512
#define MAXCSBUFS 32

/*
 * Per cylinder group information; summarized in blocks allocated
 * from first cylinder group data blocks.  These blocks have to be
 * read in from fs_csaddr (size fs_cssize) in addition to the
 * super block.
 *
 * N.B. sizeof (struct csum) must be a power of two in order for
 * the ``fs_cs'' macro to work (see below).
 */
struct ufs_csum {
    INT32 cs_ndir;      /* number of directories */
    INT32 cs_nbfree;    /* number of free blocks */
    INT32 cs_nifree;    /* number of free inodes */
    INT32 cs_nffree;    /* number of free fragments */
};

/*
 * Super block for a file system.
 */
#define FS_MAGIC    0x011954
#define FSACTIVE    0x5e72d81a  /* fs_state: mounted */
#define FSOKAY      0x7c269d38  /* fs_state: clean */
#define FSBAD       0xcb096f43  /* fs_state: bad root */

struct ufs_superblock {
    struct ufs_superblock *fs_link;
    struct ufs_superblock *fs_rlink;
    INT32 fs_sblkno;  /* super block number */
    INT32 fs_cblkno;  /* cylinder group block number */
    INT32 fs_iblkno;  /* inode block number */
    INT32 fs_dblkno;  /* data block number */
    INT32 fs_cgoffset; /* cylinder group offset */
    INT32 fs_cgmask;  /* cylinder group mask */
    INT32 fs_time;    /* last update time */
    INT32 fs_size;    /* size of the file system in blocks */
    INT32 fs_dsize;   /* size of the data blocks in blocks */
    INT32 fs_ncg;     /* number of cylinder groups */
    INT32 fs_bsize;   /* size of a block in bytes */
    INT32 fs_fsize;   /* size of a fragment in bytes */
    INT32 fs_frag;    /* number of fragments per block */
    INT32 fs_minfree; /* minimum free space percentage */
    INT32 fs_rotdelay; /* rotational delay in milliseconds */
    INT32 fs_rps;     /* revolutions per second */
    INT32 fs_bmask;   /* block mask */
    INT32 fs_fmask;   /* fragment mask */
    INT32 fs_bshift;  /* block size shift */
    INT32 fs_fshift;  /* fragment size shift */
    INT32 fs_maxcontig; /* maximum contiguous blocks */
    INT32 fs_maxbpg;  /* maximum blocks per group */
    INT32 fs_fragshift; /* fragment shift */
    INT32 fs_fsbtodb; /* bytes to disk blocks */
    INT32 fs_sbsize; /* size of super block in bytes */
    INT32 fs_csmask; /* cylinder group summary mask */
    INT32 fs_csshift; /* cylinder group summary shift */
    INT32 fs_nindir; /* number of inodes per cylinder group */
    INT32 fs_inopb;  /* number of inodes per block */
    INT32 fs_nspf; /* number of sectors per fragment */
    INT32 fs_optim; /* optimization preference */
    INT32 fs_state; /* file system state */
    INT32 fs_sparecon[2]; /* reserved for future use */
    INT32 fs_id[2]; /* file system ID */
    INT32 fs_csaddr; /* cylinder group summary address */
    INT32 fs_cssize; /* size of cylinder group summary */
    INT32 fs_cgsize; /* size of a cylinder group */
    INT32 fs_ntrak; /* number of tracks per cylinder */
    INT32 fs_nsect; /* number of sectors per track */
    INT32 fs_spc; /* sectors per cylinder */
    INT32 fs_ncyl; /* number of cylinders */
    INT32 fs_cpg; /* cylinders per group */
    INT32 fs_ipg; /* inodes per group */
    INT32 fs_fpg; /* fragments per group */
    struct ufs_csum fs_cstotal; /* total cylinder group summary */
    INT8 fs_fmod; /* file system modified flag */
    INT8 fs_clean; /* file system clean flag */
    INT8 fs_ronly; /* read-only flag */
    INT8 fs_flags;
    INT8 fs_fsmnt[MAXMNTLEN]; /* file system mount point */
    INT32 fs_cgrotor; /* cylinder group rotation */
    struct ufs_csum *fs_csp[MAXCSBUFS]; /* cylinder group summary pointers */
    INT32 fs_cpc; /* cylinders per cylinder group */
    INT16 fs_postbl[MAXCPG][NRPOS]; /* cylinder group position table */
    INT32 fs_magic; /* magic number */
    UINT16 fs_rotbl[1]; /* rotational position table */
    /* actually longer */
};

#endif /* UFS_H_ */
