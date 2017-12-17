obj-m += lora.o
lora-y := af_lora.o dgram.o

obj-m += lora-dev.o
lora-dev-y := dev.o

obj-m += sx1276.o

ifneq ($(CONFIG_SERIAL_DEV_BUS),)
obj-m += rn2483.o
obj-m += usi.o
obj-m += wimod.o
endif
