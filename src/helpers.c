#include <efi.h>
#include <efilib.h>

#include "boot.h"
#include "part.h"

typedef struct BLOCK_IO {
    EFI_BLOCK_IO_PROTOCOL BlockIo;
    EFI_BLOCK_IO_PROTOCOL *ParentBlockIo;
    EFI_LBA StartLba; // Start LBA of the partition
} PARTITION_BLOCK_IO;

UINTN
StrDecimalToUintn(CHAR16 *str)
{
    UINTN result = 0;

    while (*str >= L'0' && *str <= L'9') {
        result = result * 10 + (*str - L'0');
        str++;
    }

    return result;
}

static EFI_STATUS
PartitionReadBlocks(EFI_BLOCK_IO_PROTOCOL *This, UINT32 MediaId, EFI_LBA Lba, UINTN BufferSize, VOID *Buffer)
{
    PARTITION_BLOCK_IO *PartitionBlockIo = (PARTITION_BLOCK_IO *)This;
    EFI_BLOCK_IO_PROTOCOL *ParentBlockIo = PartitionBlockIo->ParentBlockIo;

    EFI_LBA ParentLba = PartitionBlockIo->StartLba + Lba;

    return uefi_call_wrapper(ParentBlockIo->ReadBlocks, 5, ParentBlockIo, MediaId, ParentLba, BufferSize, Buffer);
}

EFI_STATUS
MountAtLba(EFI_BLOCK_IO_PROTOCOL *ParentBlockIo, EFI_LBA StartLba, EFI_SIMPLE_FILE_SYSTEM_PROTOCOL **FileSystem)
{
    EFI_STATUS Status;
    PARTITION_BLOCK_IO *PartitionBlockIo = NULL;
    EFI_HANDLE PartitionHandle = NULL;

    PartitionBlockIo = AllocateZeroPool(sizeof(PARTITION_BLOCK_IO));
    if (!PartitionBlockIo)
        return EFI_OUT_OF_RESOURCES;

    PartitionBlockIo->ParentBlockIo = ParentBlockIo;
    PartitionBlockIo->StartLba = StartLba;

    PartitionBlockIo->BlockIo.Media = ParentBlockIo->Media;
    PartitionBlockIo->BlockIo.Revision = ParentBlockIo->Revision;
    PartitionBlockIo->BlockIo.Reset = ParentBlockIo->Reset;
    PartitionBlockIo->BlockIo.ReadBlocks = PartitionReadBlocks;
    PartitionBlockIo->BlockIo.FlushBlocks = ParentBlockIo->FlushBlocks;

    PartitionHandle = NULL;
    Status = uefi_call_wrapper(BS->InstallMultipleProtocolInterfaces, 5, &PartitionHandle, &BlockIoProtocol,
        &PartitionBlockIo->BlockIo, NULL);
    if (EFI_ERROR(Status)) {
        Print(L"No filesystem detected at LBA %llu: %r\n", StartLba, Status);
        uefi_call_wrapper(BS->UninstallMultipleProtocolInterfaces, 5, PartitionHandle, &BlockIoProtocol,
            &PartitionBlockIo->BlockIo, NULL);
        return Status;
    }

    return EFI_SUCCESS;
}

EFI_STATUS
FindPartitionStart(EFI_BLOCK_IO_PROTOCOL *BlockIo, UINT32 *PartitionStart)
{
	EFI_STATUS Status;
	UINTN BlockSize = BlockIo->Media->BlockSize;
	UINT8 *MbrBuffer;
	struct mbr_partition *Partitions;

	if (BlockIo->Media->LogicalPartition) {
		Print(L"The drive chosen is not a drive.\n");
		return EFI_UNSUPPORTED;
	}

	Status = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, BlockSize, (void**)&MbrBuffer);
	if (EFI_ERROR(Status)) {
		Print(L"Failed to allocate memory for MBR: %r\n", Status);
		return Status;
	}

	Status = uefi_call_wrapper(BlockIo->ReadBlocks, 5, BlockIo,
		BlockIo->Media->MediaId, 0, BlockSize, MbrBuffer);
	if (EFI_ERROR(Status)) {
		Print(L"Failed to read MBR: %r\n", Status);
		FreePool(MbrBuffer);
		return Status;
	}

	// Check for valid MBR signature.
	if (MbrBuffer[510] != 0x55 || MbrBuffer[511] != 0xAA) {
		Print(L"Invalid MBR signature.\n");
		FreePool(MbrBuffer);
		return EFI_NOT_FOUND;
	}

	Partitions = (struct mbr_partition *)(MbrBuffer + 0x1BE);

	for (int i = 0; i < 4; i++) {
		if (Partitions[i].os_type == 0x63) {
			*PartitionStart = Partitions[i].starting_lba;
			Print(L"Found System V partition at LBA %u\n", *PartitionStart);
			FreePool(MbrBuffer);
			return EFI_SUCCESS;
		}
	}

	Print(L"No System V partition found in MBR\n");
	FreePool(MbrBuffer);
	return EFI_NOT_FOUND;
}

UINT32
SwapBytes32(UINT32 val)
{
    return ((val & 0x000000FF) << 24) | ((val & 0x0000FF00) << 8) | ((val & 0x00FF0000) >> 8) | ((val & 0xFF000000) >> 24);
}

UINT16
SwapBytes16(UINT16 val)
{
    return (val >> 8) | (val << 8);
}
