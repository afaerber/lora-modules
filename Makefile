#
# Helpers to build modules out of linux-next based LoRa patch queue
#

KDIR ?= /lib/modules/`uname -r`/build

SDIR ?= $$PWD/linux
IDIR = $$PWD/include

MFLAGS_KCONFIG := CONFIG_LORA=m
MFLAGS_KCONFIG += CONFIG_LORA_DEV=m
MFLAGS_KCONFIG += CONFIG_LORA_MM002=m
MFLAGS_KCONFIG += CONFIG_LORA_RAK811=m
MFLAGS_KCONFIG += CONFIG_LORA_RF1276TS=m
MFLAGS_KCONFIG += CONFIG_LORA_RN2483=m
MFLAGS_KCONFIG += CONFIG_LORA_SX125X_CORE=m
MFLAGS_KCONFIG += CONFIG_LORA_SX127X=m
MFLAGS_KCONFIG += CONFIG_LORA_SX128X=m
MFLAGS_KCONFIG += CONFIG_LORA_SX130X=m
MFLAGS_KCONFIG += CONFIG_LORA_TING01M=m
MFLAGS_KCONFIG += CONFIG_LORA_USI=m
MFLAGS_KCONFIG += CONFIG_LORA_WIMOD=m

all: test
#	$(MAKE) -C $(KDIR) M=$$PWD
	$(MAKE) -C $(KDIR) M=$(SDIR)/net/lora $(MFLAGS_KCONFIG) \
		CFLAGS_MODULE=-I$(IDIR)
	$(MAKE) -C $(KDIR) M=$(SDIR)/drivers/net/lora \
		$(MFLAGS_KCONFIG) \
		CFLAGS_MODULE="-I$(IDIR) -DCONFIG_LORA_SX125X_CON"

usb:
	$(MAKE) -C $(KDIR) M=$(SDIR)/drivers/usb/class cdc-acm.ko

modules_install:
	for m in $(SDIR)/net/lora $(SDIR)/drivers/net/lora; do \
		$(MAKE) -C $(KDIR) M=$$m $(MFLAGS_KCONFIG) modules_install; \
	done

clean:
	$(MAKE) -C $(KDIR) M=$(SDIR)/net/lora $(MFLAGS_KCONFIG) clean
	$(MAKE) -C $(KDIR) M=$(SDIR)/drivers/net/lora $(MFLAGS_KCONFIG) clean
	@rm -f test nltest

test: test.c
	$(CC) -o test test.c

nltest: nltest.c
	$(CC) $(shell pkg-config --cflags --libs libnl-genl-3.0) -o nltest nltest.c
