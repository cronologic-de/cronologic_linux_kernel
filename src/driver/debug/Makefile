# -----------------------------------------------------------------------------
# 					Kernel Module - 32 Bit Debug Build 
# -----------------------------------------------------------------------------

#_____________________
# Set global variables
#
TARGET 	:= crono_pci_drvmod
KDIR 	:= /lib/modules/$(shell uname -r)/build
PWD 	:= $(shell pwd)
MAKE 	:= make

#_______________________
# Set compiler variables
#

# Target Specs
obj-m 			+= $(TARGET).o
$(TARGET)-objs 	+= crono_kernel_module.o 
# Macros & Flags
ccflags-y 		:= -DCRONO_KERNEL_MODE -DCRONO_DEBUG_ENABLED 
# Include Paths
ccflags-y 		+= -I$(src)/../../../lib/include 
# Architecture
KBUILD_AFLAGS 	+= -march=i386

#_______________________
# Targets
#
all:
	ln -s $(PWD)/../crono_kernel_module.c $(PWD)/crono_kernel_module.c
	ln -s $(PWD)/../kernel_module.h $(PWD)/kernel_module.h
	ln -s $(PWD)/../crono_miscdevice.h $(PWD)/crono_miscdevice.h
	$(MAKE) -C $(KDIR) M=$(PWD) modules  ARCH=i386
	rm $(PWD)/crono_kernel_module.c
	rm $(PWD)/kernel_module.h
	rm $(PWD)/crono_miscdevice.h
       
clean:
	-$(MAKE) -C $(KDIR) M=$(PWD) clean || true

install:
	$(MAKE) -C $(KDIR) M=$(PWD) modules	\
    	INSTALL_MOD_PATH=$(INSTALL_ROOT) modules_install
