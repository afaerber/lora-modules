ifneq ($(KERNELRELEASE),)
include Kbuild
else

KDIR ?= /lib/modules/`uname -r`/build

all:
	$(MAKE) -C $(KDIR) M=$$PWD

modules_install:
	$(MAKE) -C $(KDIR) M=$$PWD modules_install

clean:
	$(MAKE) -C $(KDIR) M=$$PWD clean

endif
