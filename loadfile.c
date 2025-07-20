/*
 * Common file loading routines.
 */

#include <efi.h>
#include <efilib.h>

#include "boot.h"

EFI_STATUS
LoadFile(CHAR16 *args)
{
	EFI_STATUS Status;
	EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *SimpleFS;
	EFI_FILE_PROTOCOL *Root = NULL;
	EFI_FILE_PROTOCOL *File = NULL;
	EFI_FILE_INFO *FileInfo = NULL;
	UINTN FileInfoSize;
	VOID *Buffer = NULL;
	UINTN BufferSize;

	Status = gBS->HandleProtocol(gImageHandle,
		&gEfiSimpleFileSystemProtocolGuid, (void **)&SimpleFS);
	if (EFI_ERROR(Status)) {
		Print(L"Failed to get FS protocol: %r\n", Status);
		return Status;
	}

	Status = SimpleFS->OpenVolume(SimpleFS, &Root);
	if (EFI_ERROR(Status)) {
		Print(L"Failed to open volume: %r\n", Status);
		return Status;
	}

	Status = Root->Open(Root, &File, L"myprog.elf", EFI_FILE_MODE_READ, 0);
	if (EFI_ERROR(Status)) {
		Print(L"Failed to open file: %r\n", Status);
		return Status;
	}

	FileInfoSize = sizeof(EFI_FILE_INFO) + 200;
	Status = gBS->AllocatePool(EfiLoaderData, FileInfoSize,
		(void **)&FileInfo);
	if (EFI_ERROR(Status)) {
		Print(L"Failed to allocate pool: %r\n", Status);
		goto cleanup;
	}

	Status = File->GetInfo(File, &gEfiFileInfoGuid, &FileInfoSize, FileInfo);
	if (EFI_ERROR(Status)) {
		Print(L"Failed to get file info: %r\n", Status);
		goto cleanup;
	}

	BufferSize = FileInfo->FileSize;
	gBS->FreePool(FileInfo);

	Status = gBS->AllocatePool(EfiLoaderData, BufferSize, &Buffer);
	if (EFI_ERROR(Status)) {
		Print(L"Failed to allocate buffer: %r\n", Status);
		goto cleanup;
	}

	Status = File->Read(File, &BufferSize, Buffer);
	if (EFI_ERROR(Status)) {
		Print(L"Failed to read file: %r\n", Status);
		goto cleanup;
	}

	File->Close(File);
	Print(L"File loaded, size=%lu bytes\n", BufferSize);

cleanup:
	if (Buffer != NULL) {
		Print(L"Freeing the pool Buffer\n");
		gBS->FreePool(Buffer);
	}

	if (FileInfo != NULL) {
		Print(L"Freeing the pool FileInfo\n");
		gBS->FreePool(FileInfo);
	}

	if (File != NULL) {
		Print(L"Closing File\n");
		File->Close(File);
	}

	return Status;
}

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
