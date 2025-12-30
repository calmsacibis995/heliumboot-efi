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

#ifndef _VTOC_H_
#define _VTOC_H_

#include <efi.h>
#include <efilib.h>

#define V_NUMPAR 		16		/* The number of partitions */
#define VTOC_SEC       	29      /* VTOC sector number on disk */

#define VTOC_SANE		0x600DDEEE	/* Indicates a sane VTOC */
#define V_VERSION		0x01		/* layout version number */

#define ALT_SANITY      0xdeadbeef  /* magic # to validate alt table */
#define ALT_VERSION     0x02		/* version of table */

#define VALID_PD		0xCA5E600D

#define MAX_ALTENTS     253     /* Maximum # of slots for alts allowed for in the table. */

struct svr4_partition {
	UINT16 p_tag;       /* ID tag of partition */
	UINT16 p_flag;      /* permision flags */
	INT32 p_start;      /* start sector no of partition */
	INT32 p_size;       /* # of blocks in partition */
};

/*
 * VTOC structure.
 * Used to read the disk partition table out of SysVr4/Solaris disks.
 */
struct svr4_vtoc {
    UINT32 v_sanity;        /* sanity check */
    UINT32 v_version;       /* version number */
    INT8 v_volume[8];       /* volume name */
    UINT16 v_nparts;        /* number of partitions */
    UINT16 v_pad;           /* padding */
    UINT32 v_reserved[10];  /* reserved */
    struct svr4_partition v_part[V_NUMPAR]; /* partition table */
    INT32 timestamp[V_NUMPAR]; /* partition timestamp */
} __attribute__((packed));

/*
 * pdinfo structure.
 * Fields after relnext are SysV/386 specific.
 */
struct svr4_pdinfo {
    UINT32 driveid;     /* drive ID */
    UINT32 sanity;      /* sanity check */
    UINT32 version;     /* version number */
    INT8 serial[12];    /* drive serial number */
    UINT32 cyls;        /* number of cylinders */
    UINT32 tracks;      /* number of tracks */
    UINT32 sectors;     /* number of sectors */
    UINT32 bytes;       /* bytes per sector */
    UINT32 logicalst;   /* logical start sector */
    UINT32 errlogst;    /* error log start sector */
    UINT32 errlogsz;    /* error log size */
    UINT32 mfgst;       /* manufacturing start sector */
    UINT32 mfgsz;       /* manufacturing size */
    UINT32 defectst;    /* defect list start sector */
    UINT32 defectsz;    /* defect list size */
    UINT32 relno;       /* number of relocation areas */
    UINT32 relst;       /* reserved list start sector */
    UINT32 relsz;       /* reserved list size */
    UINT32 relnext;     /* reserved list next sector */
    UINT32 vtoc_ptr;    /* VTOC pointer */
    UINT16 vtoc_len;    /* VTOC length */
    UINT16 vtoc_pad;    /* VTOC padding */
    UINT32 alt_ptr;     /* alternate VTOC pointer */
    UINT16 alt_len;     /* alternate VTOC length */
    UINT16 alt_pad;     /* alternate VTOC padding */
} __attribute__((packed));

struct svr4_alt_table {
    UINT16 alt_used;        /* number of used alternate sectors */
    UINT16 alt_reserved;    /* reserved */
    INT32 alt_base;         /* base sector number */
    INT32 alt_bad[MAX_ALTENTS]; /* bad sector number */
};

struct svr4_alt_info {
    INT32 alt_sanity;       /* sanity check */
    UINT16 alt_version;     /* version number */
    UINT16 alt_pad;         /* padding */
    struct svr4_alt_table alt_trk;  /* bad track table */
    struct svr4_alt_table alt_sec;  /* bad sector table */
};

#endif /* _VTOC_H_ */
