#!/bin/sh

rmmod fsk-nrf24l01p
rmmod fsk-si443x
rmmod cfgfsk

set -e

BDIR=linux

insmod ${BDIR}/net/fsk/cfgfsk.ko dyndbg

insmod ${BDIR}/drivers/net/fsk/fsk-nrf24l01p.ko dyndbg
insmod ${BDIR}/drivers/net/fsk/fsk-si443x.ko dyndbg
