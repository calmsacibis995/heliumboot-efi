/*
 * HeliumBoot/EFI - A simple UEFI bootloader.
 *
 * Copyright (c) 2025, 2026 Stefanos Stefanidis.
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

/*
 * File: src/exec_elf.c
 * Description: Common ELF loader. Loads ELF files and executes them.
 */

#include <efi.h>
#include <efilib.h>

#include <elf.h>
#include <limits.h>

#include "boot.h"
#include "fatelf.h"

#if defined(X86_64_BLD) || defined(__x86_64__)
static Elf64_Half ArchNum = EM_X86_64;
static const CHAR16 *ArchName = L"x86_64";
#elif defined(AARCH64_BLD) || defined(__aarch64__)
static Elf64_Half ArchNum = EM_AARCH64;
static const CHAR16 *ArchName = L"aarch64";
#elif defined(RISCV64_BLD) || defined(__riscv)
static Elf64_Half ArchNum = EM_RISCV;
static const CHAR16 *ArchName = L"riscv64";
#elif defined(MIPS64_BLD) || defined(__mips64__)
static Elf64_Half ArchNum = EM_MIPS;
static const CHAR16 *ArchName = L"mips64";
#elif defined(ITANIUM_BLD) || defined(__ia64__)
static Elf64_Half ArchNum = EM_IA_64;
static const CHAR16 *ArchName = L"ia64";
#endif

BOOLEAN
IsElf64(UINT8 *Header)
{
    CHAR16 *TargArch;
    Elf64_Ehdr *ElfHdr = (Elf64_Ehdr *)Header;

    /* Magic: any mismatch -> not ELF */
    if (ElfHdr->e_ident[EI_MAG0] != ELFMAG0 ||
        ElfHdr->e_ident[EI_MAG1] != ELFMAG1 ||
        ElfHdr->e_ident[EI_MAG2] != ELFMAG2 ||
        ElfHdr->e_ident[EI_MAG3] != ELFMAG3)
        return FALSE;

    if (ElfHdr->e_ident[EI_CLASS] != ELFCLASS64) {
        PrintToScreen(L"This is a 32-bit ELF file, but you have a 64-bit version of HeliumBoot.\n");
        return FALSE;
    }

    if (ElfHdr->e_ident[EI_DATA] != ELFDATA2LSB) {
        PrintToScreen(L"This is a 64-bit big-endian ELF file, but you are attempting to run it on a little endian system.\n");
        return FALSE;
    }

    if (ElfHdr->e_ident[EI_VERSION] != EV_CURRENT)
        return FALSE;

    if (ElfHdr->e_machine != ArchNum) {
        switch (ElfHdr->e_machine) {
            case EM_NONE:
                TargArch = L"an architecture that is not specified";
                break;
            case EM_386:
                TargArch = L"i386";
                break;
            case EM_X86_64:
                TargArch = L"x86_64";
                break;
            case EM_ARM:
                TargArch = L"arm";
                break;
            case EM_AARCH64:
                TargArch = L"aarch64";
                break;
            case EM_RISCV:
                TargArch = L"riscv64";
                break;
            case EM_IA_64:
                TargArch = L"Intel Itanium";
                break;
            case EM_MIPS:
                TargArch = L"mips";
                break;
            default:
                TargArch = L"an architecture that HeliumBoot does not support";
                break;
        }
        PrintToScreen(L"This is a valid ELF file, but it is for %s, but you are running on %s.\n", TargArch, ArchName);
        return FALSE;
    }

    // TODO: Add shared object support.
    if (ElfHdr->e_type != ET_EXEC) {
        PrintToScreen(L"This ELF file is not an executable.\n");
        return FALSE;
    }

    if (ElfHdr->e_ehsize != sizeof(Elf64_Ehdr)) {
        PrintToScreen(L"Invalid ELF header size!\n");
        return FALSE;
    }

    if (ElfHdr->e_phentsize != sizeof(Elf64_Phdr)) {
        PrintToScreen(L"Invalid ELF program header size!\n");
        return FALSE;
    }

    return TRUE;
}

EFI_STATUS
LoadElfBinary(EFI_HANDLE ImageHandle, EFI_FILE_HANDLE File)
{
    EFI_STATUS Status;
    Elf64_Ehdr Ehdr;
    Elf64_Phdr *Phdrs = NULL;
    Elf64_Phdr *Ph;
    UINTN Size, i, Pages, ReadSize, seg_offset_in_page;
    VOID *Target;
    EFI_PHYSICAL_ADDRESS AllocAddr = 0;

    Size = sizeof(Ehdr);
    Status = uefi_call_wrapper(File->Read, 3, File, &Size, &Ehdr);
    if (EFI_ERROR(Status) || Size != sizeof(Ehdr)) {
        PrintToScreen(L"Failed to read full ELF header: %r\n", Status);
        return EFI_LOAD_ERROR;
    }

    if (Ehdr.e_phnum == 0 || Ehdr.e_phentsize != sizeof(Elf64_Phdr))
        return EFI_UNSUPPORTED;

    /* Check overflow: e_phnum * sizeof(Elf64_Phdr) */
    if (Ehdr.e_phnum > (UINT_MAX / sizeof(Elf64_Phdr))) {
        PrintToScreen(L"Too many program headers\n");
        return EFI_UNSUPPORTED;
    }

    Size = Ehdr.e_phnum * sizeof(Elf64_Phdr);
    Status = uefi_call_wrapper(gBS->AllocatePool, 3, EfiLoaderData, Size, (VOID **)&Phdrs);
    if (EFI_ERROR(Status)) {
        PrintToScreen(L"Failed to allocate memory for program header: %r\n", Status);
        return Status;
    }

    Status = uefi_call_wrapper(File->SetPosition, 2, File, Ehdr.e_phoff);
    if (EFI_ERROR(Status)) {
        PrintToScreen(L"Failed to seek to program headers: %r\n", Status);
        goto fail;
    }

    Status = uefi_call_wrapper(File->Read, 3, File, &Size, Phdrs);
    if (EFI_ERROR(Status) || Size != Ehdr.e_phnum * sizeof(Elf64_Phdr)) {
        PrintToScreen(L"Failed to read program headers: %r\n", Status);
        Status = EFI_LOAD_ERROR;
        goto fail;
    }

    for (i = 0; i < Ehdr.e_phnum; i++) {
        Ph = &Phdrs[i];

        if (Ph->p_type != PT_LOAD)
            continue;

        if (Ph->p_memsz < Ph->p_filesz) {
            PrintToScreen(L"Invalid segment sizes (memsz < filesz)\n");
            Status = EFI_LOAD_ERROR;
            goto fail;
        }

        /* Ensure we read from the right place in the file */
        Status = uefi_call_wrapper(File->SetPosition, 2, File, Ph->p_offset);
        if (EFI_ERROR(Status)) {
            PrintToScreen(L"Failed to seek to segment @%llu: %r\n", Ph->p_offset, Status);
            goto fail;
        }

        /* Allocate pages to hold the segment; align and include offset within page */
        seg_offset_in_page = (UINTN)(Ph->p_vaddr & (EFI_PAGE_SIZE - 1));
        Pages = EFI_SIZE_TO_PAGES((UINTN)Ph->p_memsz + seg_offset_in_page);
        AllocAddr = 0;
        Status = uefi_call_wrapper(gBS->AllocatePages, 4, AllocateAnyPages, EfiLoaderData, Pages, &AllocAddr);
        if (EFI_ERROR(Status)) {
            PrintToScreen(L"Failed to allocate pages for segment: %r\n", Status);
            goto fail;
        }
        Target = (VOID *)(UINTN)(AllocAddr + seg_offset_in_page);

        /* Read file contents into memory */
        ReadSize = (UINTN)Ph->p_filesz;
        Status = uefi_call_wrapper(File->Read, 3, File, &ReadSize, Target);
        if (EFI_ERROR(Status) || ReadSize != (UINTN)Ph->p_filesz) {
            PrintToScreen(L"Failed to read segment data: %r\n", Status);
            Status = EFI_LOAD_ERROR;
            goto fail;
        }

        /* Zero BSS (memsz - filesz) */
        if (Ph->p_memsz > Ph->p_filesz)
            SetMem((UINT8 *)Target + ReadSize, (UINTN)(Ph->p_memsz - Ph->p_filesz), 0);
    }

    /* Free program headers now we are done */
    uefi_call_wrapper(gBS->FreePool, 1, Phdrs);
    Phdrs = NULL;

    /* Exit EFI boot services. */
    {
        EFI_STATUS es;
        EFI_MEMORY_DESCRIPTOR *MemMap = NULL;
        UINTN MapSize = 0, MapKey = 0, DescSize = 0;
        UINT32 DescVersion = 0;
        int attempts = 0;

        /* Loop to handle races where the memory map changes */
        for (attempts = 0; attempts < 5; attempts++) {
            es = uefi_call_wrapper(gBS->GetMemoryMap, 5, &MapSize, MemMap, &MapKey, &DescSize, &DescVersion);
            if (es == EFI_BUFFER_TOO_SMALL) {
                /* (Re)allocate buffer with some slack */
                if (MemMap)
                    uefi_call_wrapper(gBS->FreePool, 1, MemMap);
                MapSize += 2 * DescSize;
                es = uefi_call_wrapper(gBS->AllocatePool, 3, EfiLoaderData, MapSize, (VOID **)&MemMap);
                if (EFI_ERROR(es)) {
                    PrintToScreen(L"Failed to allocate memory for memory map: %r\n", es);
                    goto fail;
                }
                /* Try again to fill buffer */
                es = uefi_call_wrapper(gBS->GetMemoryMap, 5, &MapSize, MemMap, &MapKey, &DescSize, &DescVersion);
            }

            if (EFI_ERROR(es)) {
                PrintToScreen(L"GetMemoryMap failed: %r\n", es);
                if (MemMap) { uefi_call_wrapper(gBS->FreePool, 1, MemMap); MemMap = NULL; }
                goto fail;
            }

            es = uefi_call_wrapper(gBS->ExitBootServices, 2, ImageHandle, MapKey);
            if (!EFI_ERROR(es))
                break;

            /* If ExitBootServices failed we need to retry (map likely changed). */
            PrintToScreen(L"ExitBootServices failed (attempt %d): %r\n", attempts + 1, es);
            /* Keep the current MemMap buffer and loop to get updated map again */
        }
        if (attempts == 5) {
            PrintToScreen(L"ExitBootServices failed after retries\n");
            goto fail;
        }
    }

    /* Jump to entry. */
    void (*Entry)(void) = (void (*)(void))(UINTN)Ehdr.e_entry;
    Entry();

    return EFI_SUCCESS;

fail:
    if (Phdrs)
        uefi_call_wrapper(gBS->FreePool, 1, Phdrs);
    return EFI_LOAD_ERROR;
}

#if 0
EFI_STATUS
CheckSumElf(const UINT32 *Data, UINTN Size)
{
	EFI_STATUS Status;

	return EFI_SUCCESS;
}
#endif
