#include <efi.h>
#include <efilib.h>

CHAR16 filepath[100] = {0};
UINTN bufsize = sizeof(filepath);

extern const CHAR16 version[];
extern const CHAR16 revision[];
extern const CHAR16 blddate[];

CHAR16 *ArchNames[] = {
	L"ia32",
	L"x86_64",
	L"arm",
	L"aarch64",
	L"ia64",
	L"riscv64",
	NULL
};

#if defined(__i386__)
UINTN Platform = 0;
#elif defined(__x86_64__)
UINTN Platform = 1;
#elif defined(__arm__)
UINTN Platform = 2;
#elif defined(__aarch64__)
UINTN Platform = 3;
#endif

EFI_STATUS EFIAPI
efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
	EFI_STATUS Status;
	EFI_LOADED_IMAGE *LoadedImage;
	EFI_FILE_IO_INTERFACE *FileSystem;
	EFI_FILE_HANDLE RootFS, File;
	EFI_GUID LoadedImageProtocol = EFI_LOADED_IMAGE_PROTOCOL_GUID;
	EFI_GUID FileSystemProtocol = EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID;

	InitializeLib(ImageHandle, SystemTable);

	Print(L"\nHeliumBoot/EFI (%s)\n", ArchNames[Platform]);
	Print(L"%s - %s\n", version, revision);
	Print(L"Built %s\n\n", blddate);

	// Get info about our loaded image.
	Status = uefi_call_wrapper(SystemTable->BootServices->HandleProtocol,
		3, ImageHandle, &LoadedImageProtocol, (void**)&LoadedImage);
	if (EFI_ERROR(Status)) {
		Print(L"Could not get loaded image protocol\n");
		return Status;
	}

	// Get filesystem type and open the device.
	Status = uefi_call_wrapper(SystemTable->BootServices->HandleProtocol,
		3, LoadedImage->DeviceHandle, &FileSystemProtocol, (void**)&FileSystem);
	if (EFI_ERROR(Status)) {
		Print(L"Could not get filesystem protocol\n");
		return Status;
	}

	Status = uefi_call_wrapper(FileSystem->OpenVolume, 2, FileSystem, &RootFS);
	if (EFI_ERROR(Status)) {
		Print(L"Could not open root filesystem\n");
		return Status;
	}

	// HeliumBoot main loop.
	for (;;) {
		EFI_INPUT_KEY key;
		Print(L": ");
		Input(L"", filepath, sizeof(filepath) / sizeof(CHAR16));
		Print(L"\n");

		if (StrCmp(L"", filepath) == 0)
			continue;

		Status = uefi_call_wrapper(RootFS->Open, 5, RootFS, &File, filepath,
			EFI_FILE_MODE_READ, 0);
		if (!EFI_ERROR(Status)) {
			EFI_HANDLE KernelImage;
			Status = uefi_call_wrapper(SystemTable->BootServices->LoadImage,
				6, FALSE, ImageHandle, NULL, File, 0, &KernelImage);
			if (EFI_ERROR(Status)) {
				Print(L"Failed to load image\n");
				uefi_call_wrapper(File->Close, 1, File);
				return Status;
			}
			Status = uefi_call_wrapper(SystemTable->BootServices->StartImage,
				3, KernelImage, NULL, NULL);
			if (EFI_ERROR(Status)) {
				Print(L"Failed to start kernel image\n");
				return Status;
			}
		} else {
			Print(L"Could not open file: %s\n", filepath);
		}
	}

	return EFI_SUCCESS;
}
