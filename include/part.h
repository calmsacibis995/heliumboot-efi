#ifndef _PART_H_
#define _PART_H_

#include <efi.h>
#include <efilib.h>

struct mbr_partition {
    UINT8 boot_indicator;
    UINT8 starting_chs[3];
    UINT8 os_type;
    UINT8 ending_chs[3];
    UINT32 starting_lba;
    UINT32 size_in_lba;
};

#endif /* _PART_H_ */
