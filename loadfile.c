#include <efi.h>
#include <efilib.h>

#include "boot.h"

EFI_STATUS
LoadFile(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
	EFI_STATUS Status;
	EFI_HANDLE KernelImage;
	EFI_DEVICE_PATH *FilePath;		// EFI file path

	Status = uefi_call_wrapper(RootFS->Open, 5, RootFS, &File, filepath,
			EFI_FILE_MODE_READ, 0);
	if (!EFI_ERROR(Status)) {
		// Get file path.
		FilePath = FileDevicePath(LoadedImage->DeviceHandle, filepath);
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
		// Execute the file.
		Status = uefi_call_wrapper(SystemTable->BootServices->StartImage,
			3, KernelImage, NULL, NULL);
		if (EFI_ERROR(Status)) {
			Print(L"Failed to start kernel image\n");
			return Status;
		}
	} else {
		Print(L"Could not open file: %s (%r)\n", filepath, Status);
		return Status;
	}
}
