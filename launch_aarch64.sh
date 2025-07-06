#!/bin/bash
qemu-system-aarch64 -M virt \
	-machine virtualization=true \
	-machine virt,gic-version=3 \
	-cpu max,pauth-impdef=on -smp 2 -m 4096 \
	-drive if=pflash,format=raw,file=/home/zerocool32/efi.img,readonly=on \
	-drive if=pflash,format=raw,file=/home/zerocool32//varstore.img \
	-drive if=virtio,format=raw,file=fat.img \
	-device virtio-scsi-pci,id=scsi0 \
	-object rng-random,filename=/dev/urandom,id=rng0 \
	-device virtio-rng-pci,rng=rng0 \
	-net none \
	-device virtio-gpu
	-device usb-kbd
