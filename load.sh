#!/bin/sh

rmmod lora-sx125x
rmmod lora-sx1301
rmmod lora-sx1257
rmmod lora-sx1276
rmmod lora-rn2483
rmmod lora-wimod
rmmod lora-usi
rmmod lora-rak811
rmmod lora-ting01m
rmmod lora-mm002
rmmod lora-rf1276ts
rmmod lora-dev

rmmod nllora
rmmod lora

set -e

BDIR=linux

insmod ${BDIR}/net/lora/lora.ko
insmod ${BDIR}/net/lora/nllora.ko

insmod ${BDIR}/drivers/net/lora/lora-dev.ko
insmod ${BDIR}/drivers/net/lora/lora-rn2483.ko dyndbg
insmod ${BDIR}/drivers/net/lora/lora-wimod.ko dyndbg
insmod ${BDIR}/drivers/net/lora/lora-usi.ko dyndbg
insmod ${BDIR}/drivers/net/lora/lora-rak811.ko dyndbg
insmod ${BDIR}/drivers/net/lora/lora-ting01m.ko dyndbg
insmod ${BDIR}/drivers/net/lora/lora-mm002.ko dyndbg
insmod ${BDIR}/drivers/net/lora/lora-rf1276ts.ko dyndbg
insmod ${BDIR}/drivers/net/lora/lora-sx1276.ko dyndbg
insmod ${BDIR}/drivers/net/lora/lora-sx1301.ko dyndbg
insmod ${BDIR}/drivers/net/lora/lora-sx125x.ko dyndbg
