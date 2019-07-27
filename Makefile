#
# Helpers to build modules out of linux-next based LoRa patch queue
#

KDIR ?= /lib/modules/`uname -r`/build

SDIR ?= $$PWD/linux
IDIR = $$PWD/include

MFLAGS_KCONFIG := CONFIG_LORA=m
MFLAGS_KCONFIG += CONFIG_LORA_DEV=m
MFLAGS_KCONFIG += CONFIG_LORA_MIPOT_32001353=m
MFLAGS_KCONFIG += CONFIG_LORA_MM002=m
MFLAGS_KCONFIG += CONFIG_LORA_RAK811=m
MFLAGS_KCONFIG += CONFIG_LORA_RF1276TS=m
MFLAGS_KCONFIG += CONFIG_LORA_RN2483=m
MFLAGS_KCONFIG += CONFIG_LORA_SX125X_CORE=m
MFLAGS_KCONFIG += CONFIG_LORA_SX127X=m
MFLAGS_KCONFIG += CONFIG_LORA_SX128X=m
MFLAGS_KCONFIG += CONFIG_LORA_SX128X_SPI=y
MFLAGS_KCONFIG += CONFIG_LORA_SX130X=m
MFLAGS_KCONFIG += CONFIG_LORA_TING01M=m
MFLAGS_KCONFIG += CONFIG_LORA_USI=m
MFLAGS_KCONFIG += CONFIG_LORA_WIMOD=m

MFLAGS_KCONFIG += CONFIG_FSK=m
MFLAGS_KCONFIG += CONFIG_FSK_CC1120=m
MFLAGS_KCONFIG += CONFIG_FSK_NRF24L01P=m
MFLAGS_KCONFIG += CONFIG_FSK_MRF89XA=m
MFLAGS_KCONFIG += CONFIG_FSK_S2LP=m
MFLAGS_KCONFIG += CONFIG_FSK_SI443X=m

all: test
#	$(MAKE) -C $(KDIR) M=$$PWD
	$(MAKE) -C $(KDIR) M=$(SDIR)/net/fsk \
		$(MFLAGS_KCONFIG) \
		CFLAGS_MODULE=-I$(IDIR)
	$(MAKE) -C $(KDIR) M=$(SDIR)/net/lora \
		$(MFLAGS_KCONFIG) \
		CFLAGS_MODULE=-I$(IDIR)
	$(MAKE) -C $(KDIR) M=$(SDIR)/drivers/net/lora \
		$(MFLAGS_KCONFIG) \
		KBUILD_EXTRA_SYMBOLS="$(SDIR)/net/lora/Module.symvers $(SDIR)/net/fsk/Module.symvers" \
		CFLAGS_MODULE="-I$(IDIR) -DCONFIG_FSK -DCONFIG_LORA_SX125X_CON -DCONFIG_LORA_SX128X_SPI"
	$(MAKE) -C $(KDIR) M=$(SDIR)/drivers/net/lorawan \
		$(MFLAGS_KCONFIG) \
		KBUILD_EXTRA_SYMBOLS=$(SDIR)/net/lora/Module.symvers \
		CFLAGS_MODULE=-I$(IDIR)

fsk:
	$(MAKE) -C $(KDIR) M=$(SDIR)/net/fsk \
		$(MFLAGS_KCONFIG) \
		CFLAGS_MODULE=-I$(IDIR)
	$(MAKE) -C $(KDIR) M=$(SDIR)/drivers/net/fsk \
		$(MFLAGS_KCONFIG) \
		KBUILD_EXTRA_SYMBOLS=$(SDIR)/net/fsk/Module.symvers \
		CFLAGS_MODULE=-I$(IDIR)

enocean:
	$(MAKE) -C $(KDIR) M=$(SDIR)/drivers/net/enocean \
		CFLAGS_MODULE=-I$(IDIR)

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

clean-fsk:
	$(MAKE) -C $(KDIR) M=$(SDIR)/net/fsk $(MFLAGS_KCONFIG) clean
	$(MAKE) -C $(KDIR) M=$(SDIR)/drivers/net/fsk $(MFLAGS_KCONFIG) clean

clean-enocean:
	$(MAKE) -C $(KDIR) M=$(SDIR)/drivers/net/enocean clean

test: test.c
	$(CC) -o test test.c

txenocean: txenocean.c
	$(CC) -o txenocean txenocean.c

nltest: nltest.c
	$(CC) $(shell pkg-config --cflags --libs libnl-genl-3.0) -o nltest nltest.c
