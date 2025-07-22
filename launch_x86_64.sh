#!/bin/bash

qemu-system-x86_64 -L /usr/share/ovmf -pflash ~/OVMF.fd -hda fat.img -net none
