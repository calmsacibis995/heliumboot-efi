# HeliumBoot/EFI
HeliumBoot/EFI is a simple UEFI bootloader, intended for research on how bootloaders work.

## Features
HeliumBoot/EFI has these features that distinguish it from other simple bootloaders:
 * Portability - it runs on x86_64, aarch64, and riscv64 targets, powered by GNU-EFI.
 * Command support - A built-in command parser, simple, yet powerful, and easy to implement new commands.
 * Unix SVR4 VTOC support
 * File system support: S5, UFS, FAT32

## Currently implemented features
 * Main menu
 * Graphical display output
 * Unix SVR4 VTOC support
 * Command Monitor
 * EFI binary boot
 * FAT32 support

## Currently unimplemented features
 * ELF boot
 * COFF boot
 * a.out boot
 * S5 file system support
 * UFS file system support

## TODO list
 * Add more options in the main menu.

## Build instructions

### Dependencies
To build HeliumBoot/EFI, you will need:
 * `gcc` (for cross compiling, see the [Cross compilation](#Cross-compilation) section.
 * `make`

### Cloning the repository
`git clone https://github.com/calmsacibis995/heliumboot-efi.git`

### Submodules
HeliumBoot/EFI depends on GNU-EFI, which is a submodule of the repository. Update the submodules:

`cd heliumboot-efi`

`git submodule update --init`

### Building GNU-EFI
This step needs to be done before building HeliumBoot/EFI!

`cd gnu-efi`

`make`

`cd ..`

### Building HeliumBoot/EFI
Running `make` on the root directory of the source tree will result in a screen like this:

`You have to select a target.`

`Make sure you have built GNU-EFI first!`

`make x86_64_build:         Build HeliumBoot for x86_64`

`make aarch64_build:        Build HeliumBoot for AArch64`

`make riscv64_build:        Build HeliumBoot for 64-bit RISC-V`

`make x86_64_dev_build:     Build HeliumBoot/Dev for x86_64`

`make aarch64_dev_build:    Build HeliumBoot/Dev for AArch64`

`make riscv64_dev_build:    Build HeliumBoot/Dev for 64-bit RISC-V`

`make x86_64_debug_build:   Build HeliumBoot/Debug for x86_64`

`make aarch64_debug_build:  Build HeliumBoot/Debug for AArch64`

`make riscv64_debug_build:  Build HeliumBoot/Debug for 64-bit RISC-V`

It also reminds you to build GNU-EFI before building HeliumBoot/EFI. If you forgot to do this, go back to the previous two steps.

#### Target explanation
Note that `arch` means one of `x86_64`, `aarch64`, `riscv64`.
 * `arch_build`: Make a release build. Pick this if you are unsure.
 * `arch_dev_build`: Make a developer build. The only difference is different branding and build identification.
 * `arch_debug_build`: Make a debug build. This enables additional diagnostic messages to assist in debugging.

Choose your target, then run `make target`, where `target` is the one of the supported targets above.

### Cross compilation
TODO

## Supported hardware configurations
Although UEFI is present in (almost) all modern desktops, laptops, and other devices, there might be some incompatibilities between different UEFI implementations. Development is primarily done under QEMU with OVMF/Tiano, but feel free to test HeliumBoot/EFI on real hardware.

| Name                      | Manufacturer        | CPU architecture   | Release Year    | Firmware vendor      | Firmware vendor's revision   | EFI revision   | Emulator/VM? | Notes                                                                                                      |
|---------------------------|---------------------|--------------------|-----------------|----------------------|------------------------------|----------------|--------------|-----------------------------------------------------------------------------------------------------------|
| QEMU x86_64               | QEMU devlopers      | x86_64             | 2003            | Ubuntu/Canonical     | 1.0                          | 2.70           | Yes          | Works perfectly, uses direct framebuffer access for graphics.                                              |
| QEMU AArch64              | QEMU developers     | AArch64            | 2003            | Ubuntu/Canonical     | 1.0                          | 2.70           | Yes          | Works perfectly, uses BLT for graphics.                                                                    |
| MacBook A1181             | Apple               | x86_64             | early 2009      | Apple                | 1.10                         | 1.10           | No           | Works perfectly, uses direct framebuffer access for graphics.                                              |
| VirtualBox                | InnoTek/Sun/Oracle  | x86_64             | 2007            | EDK II               | 1.0                          | 2.70           | Yes          | Works perfectly, uses direct framebuffer access for graphics.                                              |
