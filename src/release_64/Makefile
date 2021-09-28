# -----------------------------------------------------------------------------
# 					Kernel Module - 64 Bit Release Build 
# -----------------------------------------------------------------------------

#_____________________
# Set global variables
#
TARGET 	:= crono_pci_drvmod
KDIR 	:= /lib/modules/$(shell uname -r)/build
PWD 	:= $(shell pwd)

# Relative path to /build/bin 
BLDBINPATH	:= ../../build/bin

# Turn it on to avoid dmesg error `tainting kernel`
CONFIG_MODULE_SIG=y

#_______________________
# Set compiler variables
#

# Target Specs
obj-m 			+= $(TARGET).o
$(TARGET)-objs 	+= crono_kernel_module.o 
# Macros & Flags
ccflags-y 		:= -DCRONO_KERNEL_MODE
# Include Paths
ccflags-y 		+= -I$(src)/../../include 
# Architecture
ccflags-y 		+= -m64

#_______________________
# Targets
#
all:
	$(call CRONO_CREATE_SYMLINKS)
	$(call CRONO_GENERATE_LINUX_VERSION_H)
	$(MAKE) -C $(KDIR) M=$(PWD) modules 
	$(call CRONO_COPY_DRVMOD_TARGET,$(TARGET).ko,,release_64)
	$(call CRONO_CLEAN_SYMLINKS)
	
clean:
	-$(MAKE) -C $(KDIR) M=$(PWD) clean || true
	$(call CRONO_CLEAN_SYMLINKS)
	rm -f $(BLDBINPATH)/release_64/$(TARGET).ko

# _____________________________________________________________________________
# Custom Functions
#

# Create symbolic links for source files to be built in this direcotory
define CRONO_CREATE_SYMLINKS
	ln -sf $(PWD)/../crono_kernel_module.c $(PWD)/crono_kernel_module.c
	ln -sf $(PWD)/../crono_kernel_module.h $(PWD)/crono_kernel_module.h
	ln -sf $(PWD)/../crono_miscdevice.h $(PWD)/crono_miscdevice.h
endef

# Cleanup symbolic links 
define CRONO_CLEAN_SYMLINKS
	rm -f $(PWD)/crono_kernel_module.c
	rm -f $(PWD)/crono_kernel_module.h
	rm -f $(PWD)/crono_miscdevice.h
endef

# $1: Target 
# $2: Empty
# $3: Build name (e.g. `release_64`)
# $(BLDBINPATH) must be defined
define CRONO_COPY_DRVMOD_TARGET
	mkdir -p $(BLDBINPATH)/$(3)
	cp -t $(BLDBINPATH)/$(3) $(1)
endef

# Generates version.h from currently used kernel, and create a symbolic link 
# for it in our include directory, to be used as a more accurate way than
# /usr/include/linux/version.h
define CRONO_GENERATE_LINUX_VERSION_H
	cd /usr/src/linux-headers-`uname -r` ; \
		make include/generated/uapi/linux/version.h 
	ln -sf /usr/src/linux-headers-`uname -r`/include/generated/uapi/linux/version.h \
		../../include/version.h
endef