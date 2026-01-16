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

    if (ElfHdr->e_ident[EI_MAG0] != ELFMAG0 && ElfHdr->e_ident[EI_MAG1] != ELFMAG1 && ElfHdr->e_ident[EI_MAG2] != ELFMAG2 && ElfHdr->e_ident[EI_MAG3] != ELFMAG3)
        return FALSE;

    if (ElfHdr->e_ident[EI_CLASS] != ELFCLASS64) {
        PrintToScreen(L"This is a 32-bit ELF file, but you have a 64-bit version of HeliumBoot.\n");
        return FALSE;
    }

    if (ElfHdr->e_ident[EI_DATA] != ELFDATA2LSB) {
        PrintToScreen(L"This is a 64-bit big-endian ELF file, but you are attempting to run it on a little endian system.\n");
        return FALSE;
    }

    if (ElfHdr->e_ident[EI_VERSION] != 1)
        return FALSE;

    if (ElfHdr->e_ident[EI_ABIVERSION] != 1)
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
LoadElfBinary(EFI_FILE_HANDLE File)
{
    EFI_STATUS Status;
    Elf64_Ehdr Ehdr;
    Elf64_Phdr *Phdrs = NULL;
    UINTN Size, i;

    Size = sizeof(Ehdr);
    Status = uefi_call_wrapper(File->Read, 3, File, &Size, &Ehdr);
    if (EFI_ERROR(Status)) {
        PrintToScreen(L"Failed to read ELF file: %r\n");
        return EFI_LOAD_ERROR;
    }

    if (Ehdr.e_phnum == 0 || Ehdr.e_phentsize != sizeof(Elf64_Phdr))
        return EFI_UNSUPPORTED;

    Size = Ehdr.e_phnum * sizeof(Elf64_Phdr);
    Status = uefi_call_wrapper(gBS->AllocatePool, 3, EfiLoaderData, Size, (VOID **)&Phdrs);
    if (EFI_ERROR(Status)) {
        PrintToScreen(L"Failed to allocate memory for program header: %r\n", Status);
        return Status;
    }

    Status = uefi_call_wrapper(File->SetPosition, 2, File, Ehdr.e_phoff);
    if (EFI_ERROR(Status))
        return EFI_LOAD_ERROR;

    Status = uefi_call_wrapper(File->Read, 3, File, &Size, Phdrs);
    if (EFI_ERROR(Status))
        return EFI_LOAD_ERROR;

    for (i = 0; i < Ehdr.e_phnum; i++) {
        Elf64_Phdr *Ph = &Phdrs[i];
        EFI_PHYSICAL_ADDRESS Addr;
        UINTN Pages, ReadSize;

        switch (Ph->p_type) {
            case PT_LOAD:
                break;
            case PT_INTERP:
            case PT_SHLIB:
            case PT_PHDR:
            case PT_NULL:
            case PT_DYNAMIC:
            case PT_NOTE:
            default:
                continue;
        }

        Pages = EFI_SIZE_TO_PAGES(Ph->p_memsz);
        Addr = Ph->p_paddr ? Ph->p_paddr : Ph->p_vaddr;

        Status = uefi_call_wrapper(gBS->AllocatePages, 4, AllocateAddress, EfiLoaderData, Pages, &Addr);
        if (EFI_ERROR(Status))
            goto fail;

        ReadSize = Ph->p_filesz;
        Status = uefi_call_wrapper(File->Read, 3, File, &ReadSize, (VOID *)(UINTN)Addr);
        if (EFI_ERROR(Status))
            goto fail;

        // Clear BSS.
        if (Ph->p_memsz > Ph->p_filesz)
            SetMem((VOID *)(UINTN)(Addr + Ph->p_filesz), Ph->p_memsz - Ph->p_filesz, 0);
    }

    void (*Entry)(void);
    Entry = (void (*)(void))(UINTN)Ehdr.e_entry;

    // We shall not return.
    Entry();

    return EFI_SUCCESS;

fail:
    if (Phdrs)
        uefi_call_wrapper(gBS->FreePool, 1, Phdrs);

    return EFI_LOAD_ERROR;
}
