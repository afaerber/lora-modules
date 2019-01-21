#!/bin/sh

rmmod fsk-nrf24l01p

set -e

BDIR=linux

insmod ${BDIR}/drivers/net/fsk/fsk-nrf24l01p.ko dyndbg
