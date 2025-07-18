/*
 * Common file routines.
 */

#include <efi.h>
#include <efilib.h>

#include "boot.h"

EFI_STATUS
LoadFileEFI(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable, CHAR16 *path)
{
	EFI_STATUS Status;
	EFI_HANDLE KernelImage;
	EFI_DEVICE_PATH *FilePath;		// EFI file path

	// Open the file.
	Status = uefi_call_wrapper(RootFS->Open, 5, RootFS, &File, path, EFI_FILE_MODE_READ, 0);
	if (EFI_ERROR(Status)) {
		Print(L"Could not open file (%r)\n", Status);
		return Status;
	}

	// Get file path.
	FilePath = FileDevicePath(LoadedImage->DeviceHandle, path);
	if (FilePath == NULL) {
		Print(L"Could not create device path for file\n");
		return EFI_NOT_FOUND;
	}

	// Load the file.
	Status = uefi_call_wrapper(SystemTable->BootServices->LoadImage,
		6, FALSE, ImageHandle, FilePath, NULL, 0, &KernelImage);
	if (EFI_ERROR(Status)) {
		Print(L"Failed to load image (%r)\n", Status);
		FreePool(FilePath);
		uefi_call_wrapper(File->Close, 1, File);
		return Status;
	}

	// Free the pool.
	FreePool(FilePath);
	uefi_call_wrapper(File->Close, 1, File);

	// Execute the file.
	Status = uefi_call_wrapper(SystemTable->BootServices->StartImage,
		3, KernelImage, NULL, NULL);
	if (EFI_ERROR(Status)) {
		Print(L"Failed to start kernel image\n");
		return Status;
	}

	return Status;
}
