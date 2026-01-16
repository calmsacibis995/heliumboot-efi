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
 * Execute an a.out binary.
 */

#include <efi.h>
#include <efilib.h>

#include "aout.h"
#include "boot.h"

BOOLEAN
IsAOut(UINT8 *Header)
{
    struct Exec *exec = (struct Exec *)Header;

    switch (exec->a_magic) {
        case OMAGIC:
            break;
        case NMAGIC:
        case ZMAGIC:
            if (exec->a_text == 0)
                return FALSE;
            break;
        default:
            return FALSE;
    }

    return TRUE;
}

EFI_STATUS
LoadAOutBinary(EFI_FILE_HANDLE File)
{
    EFI_STATUS Status;
    struct Exec exec;
    UINTN Size;
    UINTN TotalMem;
    EFI_PHYSICAL_ADDRESS AllocAddr = 0;
    UINTN Pages = 0;
    VOID *LoadBase = NULL;
    UINTN ReadSize;

    /* Read header */
    Size = sizeof(exec);
    Status = uefi_call_wrapper(File->Read, 3, File, &Size, &exec);
    if (EFI_ERROR(Status) || Size != sizeof(exec)) {
        PrintToScreen(L"Failed to read full a.out header: %r\n", Status);
        return EFI_LOAD_ERROR;
    }

    /* OMAGIC stores text directly in data region - flatten it */
    if (exec.a_magic == OMAGIC) {
        exec.a_data += exec.a_text;
        exec.a_text = 0;
    }

    /* Basic validation and overflow checks */
    if ((UINTN)exec.a_text > (UINTN)-1) {
        PrintToScreen(L"Invalid text size\n");
        return EFI_LOAD_ERROR;
    }
    if ((UINTN)exec.a_data > (UINTN)-1) {
        PrintToScreen(L"Invalid data size\n");
        return EFI_LOAD_ERROR;
    }
    if ((UINTN)exec.a_bss > (UINTN)-1) {
        PrintToScreen(L"Invalid bss size\n");
        return EFI_LOAD_ERROR;
    }
    if ((UINTN)exec.a_text + (UINTN)exec.a_data > (UINTN)-1 - (UINTN)exec.a_bss) {
        PrintToScreen(L"Segment sizes overflow\n");
        return EFI_LOAD_ERROR;
    }

    TotalMem = (UINTN)exec.a_text + (UINTN)exec.a_data + (UINTN)exec.a_bss;
    if (TotalMem == 0) {
        PrintToScreen(L"Nothing to load\n");
        return EFI_LOAD_ERROR;
    }

    /* Allocate pages to hold the image */
    Pages = EFI_SIZE_TO_PAGES(TotalMem);
    AllocAddr = 0;
    Status = uefi_call_wrapper(gBS->AllocatePages, 4, AllocateAnyPages, EfiLoaderData, Pages, &AllocAddr);
    if (EFI_ERROR(Status)) {
        PrintToScreen(L"Failed to allocate pages for a.out image: %r\n", Status);
        return EFI_LOAD_ERROR;
    }
    LoadBase = (VOID *)(UINTN)AllocAddr;

    /* Read text segment (may be zero for OMAGIC) */
    if (exec.a_text) {
        ReadSize = (UINTN)exec.a_text;
        Status = uefi_call_wrapper(File->Read, 3, File, &ReadSize, LoadBase);
        if (EFI_ERROR(Status) || ReadSize != (UINTN)exec.a_text) {
            PrintToScreen(L"Failed to read text segment: %r\n", Status);
            goto fail;
        }
    }

    /* Read data segment */
    if (exec.a_data) {
        ReadSize = (UINTN)exec.a_data;
        Status = uefi_call_wrapper(File->Read, 3, File, &ReadSize, (UINT8 *)LoadBase + exec.a_text);
        if (EFI_ERROR(Status) || ReadSize != (UINTN)exec.a_data) {
            PrintToScreen(L"Failed to read data segment: %r\n", Status);
            goto fail;
        }
    }

    /* Zero BSS */
    if (exec.a_bss) {
        SetMem((UINT8 *)LoadBase + exec.a_text + exec.a_data, (UINTN)exec.a_bss, 0);
    }

#if defined(DEBUG_BLD)
    PrintToScreen(L"a.out entry: 0x%X  load base: 0x%lX\n", exec.a_entry, AllocAddr);
#endif

    /*
     * Compute entry pointer. In many a.out variants the entry is an offset
     * from the load base. If the entry looks like a small offset use it as
     * such; otherwise treat it as an absolute address.
     */
    {
        UINTN EntryAddr;
        if ((UINTN)exec.a_entry < TotalMem)
            EntryAddr = (UINTN)LoadBase + (UINTN)exec.a_entry;
        else
            EntryAddr = (UINTN)exec.a_entry;

        /* Jump to entry */
        void (*Entry)(void) = (void (*)(void))(UINTN)EntryAddr;
        Entry();
    }

    return EFI_SUCCESS;

fail:
    if (AllocAddr != 0 && Pages != 0)
        uefi_call_wrapper(gBS->FreePages, 2, AllocAddr, Pages);
    return EFI_LOAD_ERROR;
}
