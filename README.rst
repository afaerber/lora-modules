LoRa modules for Linux
======================

This repository used to contain source code of Linux kernel modules
for a PF_LORA socket implementation and multiple chipset drivers.

Today it contains a Makefile for building those modules from an external
Linux repository.

It also contains a userspace example program for sending a packet.

Usage
-----

To build the kernel modules for a distro kernel (e.g., openSUSE Tumbleweed):

::

  $ git clone https://github.com/afaerber/lora-modules.git
  $ cd lora-modules
  $ git clone https://git.kernel.org/pub/scm/linux/kernel/git/afaerber/linux-lora.git -b lora-next

Review the lora-modules.git file include/linux/lora.h,
which reuses some existing number for AF_LORA lower than AF_MAX,
as well as two free-at-the-time ARPHRD and ETH_P numbers.
You may need to change these numbers to avoid conflicts.

::

  $ make

Before you attempt to load any of the modules,
always review what they are currently doing!
They might have a frequency hardcoded not suited for your region,
or might do other unexpected things for testing purposes.

To go ahead and load the modules locally, tainting your kernel:

::

  # ./load.sh

That will insmod the set of drivers, but the chipset drivers won't probe
unless you're using a Device Tree Overlay for your board and chipset.

Device Tree Overlays
--------------------

Examples of DT Overlays can be found here:
https://github.com/afaerber/dt-overlays

To apply a DT Overlay on the Raspberry Pi, use ``dtoverlay=foo`` in 
config.txt (extraconfig.txt on openSUSE and SUSE Linux Enterprise Server 15).

To apply a DT Overlay on boards using U-Boot, use the ``fdt apply`` command.

On other boards you may have to resort to replacing the whole Device Tree.

Browse the openSUSE HCL Wiki for specific expansion board instructions.

Have a lot of fun!
