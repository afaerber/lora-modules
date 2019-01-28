#!/bin/sh

rmmod enocean-esp
rmmod enocean-dev

set -e

modprobe crc8

BDIR=linux

insmod ${BDIR}/drivers/net/enocean/enocean-dev.ko dyndbg
insmod ${BDIR}/drivers/net/enocean/enocean-esp.ko dyndbg
