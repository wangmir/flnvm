obj-m	+= flnvm.o
flnvm-objs	:= flnvm_block.o flnvm_hil.o flnvm_storage.o

KDIR	:= /lib/modules/4.12.0-rc5+/build
PWD	:= $(shell pwd)

default:
	$(MAKE) -C $(KDIR) SUBDIRS=$(PWD) modules

clean:
	rm	-rf *.ko
	rm	-rf *.mod.*
	rm	-rf .*.cmd
	rm	-rf *.o
	rm	-rf .tmp_versions
