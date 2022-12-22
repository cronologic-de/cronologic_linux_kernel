# 					Kernel Module - 64 Bit Release Build 
# -----------------------------------------------------------------------------

# Fedora signature consideration
CONFIG_MODULE_SIG=n

#_____________________
# Set global variables
#
ifdef KERNELRELEASE
# Runs from DKMS, use the kernel version being managed by DKMS
# Makefile is called more than once by DKMS, for clean, build, etc...
KVER 	:= $(KERNELRELEASE)
else
# Runs from command line, use the current kernel (version) 
KVER := $(shell uname -r)
endif

KDIR    := /lib/modules/$(KVER)/build
TARGET  := crono_pci_drvmod
PWD     := $(shell pwd)

# Relative path to /build/bin 
BLDBINPATH	:= ../../../build/linux/bin

# Turn it on to avoid dmesg error `tainting kernel`
CONFIG_MODULE_SIG=y

# Version information
KMAJ := $(shell echo $(KVER) | \
sed -e 's/^\([0-9][0-9]*\)\.[0-9][0-9]*\.[0-9][0-9]*.*/\1/')
KMIN := $(shell echo $(KVER) | \
sed -e 's/^[0-9][0-9]*\.\([0-9][0-9]*\)\.[0-9][0-9]*.*/\1/')
ADD_CCFLAGS_Y=
# No Indentation Tabs/Spaces in the following ifeq lines please
ifeq ($(shell if [ 5 -gt $(KMAJ) ] ; then echo $$? ; fi ;),0)
$(eval ADD_CCFLAGS_Y=-DOLD_KERNEL_FOR_PIN)
endif 
ifeq ($(shell if [ $(KMAJ) -eq 5 ] && [ 6 -ge $(KMIN) ] ; then echo $$? ; fi ;),0)
$(eval ADD_CCFLAGS_Y=-DOLD_KERNEL_FOR_PIN)
endif 

#_______________________
# Set compiler variables
#

# Target Specs
obj-m 			+= $(TARGET).o
$(TARGET)-objs 	+= crono_kernel_module.o 

# Macros & Flags
ccflags-y 		:= -DCRONO_KERNEL_MODE

# Support pin_user_pages for versions >= 5.6
ccflags-y 		+= $(ADD_CCFLAGS_Y)

# Include Paths
ccflags-y 		+= -I$(src)/../../include 

# Architecture
ccflags-y 		+= -m64

#_______________________
# Targets
#
all:
	$(call CRONO_CREATE_SYMLINKS)
	$(MAKE) -C $(KDIR) M=$(PWD) modules 
	$(call CRONO_COPY_DRVMOD_TARGET,$(TARGET).ko,,release_64)
	$(call CRONO_CLEAN_SYMLINKS)
	$(CRONO_CREATE_INST_SCRIPT)

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean || true
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

# $1: Target 
# $2: Empty
# $3: Build name (e.g. `release_64`)
# $(BLDBINPATH) must be defined
define CRONO_COPY_DRVMOD_TARGET
	mkdir -p $(BLDBINPATH)/$(3)
	cp -t $(BLDBINPATH)/$(3) $(1)
endef

ifdef KERNELRELEASE
INST_SCRIPT_PATH:=$(PWD)/../../../crono_dkms_kver
define CRONO_CREATE_INST_SCRIPT
# Create file with info on `/var/lib/dkms/crono_pci_drvmod/<version>`
# File is created once, as folder is cleanded up with `dkms remove` needed
# to re-install the module.
    $(info Crono: CRONO_CREATE_INST_SCRIPT is defined)
	echo "Crono: DKMS installation script will be created in `$(INST_SCRIPT_PATH)`"
	rm -f $(INST_SCRIPT_PATH)
	echo $(KERNELRELEASE) >> $(INST_SCRIPT_PATH)
endef
else
define CRONO_CREATE_INST_SCRIPT
	echo ""
endef
endif

