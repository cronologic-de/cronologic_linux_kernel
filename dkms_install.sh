# -----------------------------------------------------------------------------
# 		Batch script to install cronologic linux PCI driver for DKMS
# -----------------------------------------------------------------------------
#
#               PLEASE DO NOT RUN "DIRECTLY" FROM COMMAND LINE
#                        ONLY USE `dkms install .`
# 
# Prerequisites:
# - If called from DKMS, e.g. in the POST_INSTALL, someone (mainly Makefile)
#   should save DKMS KERNELRELEASE value to file `crono_dkms_kver`, so the 
#   script reads it and passes it to `install.sh`. 
#
printf "Crono: installing cronologic PCI kernel module...\n"

# Get pwd, supposed to be /var/lib/dkms/crono_pci_drvmod/<version>/source
CRONO_PWD=$(pwd)

# `crono_dkms_kver` should have been created using our Makefile, 
# on /var/lib/dkms/crono_pci_drvmod/<version>/ with the DKMS kernel version.
cd ..

if [ ! -f crono_dkms_kver ]; then
    printf "Crono: Error, file 'crono_dkms_kver' is not found on $(pwd). Module can't be installed.\n"
    exit 1
fi

# export `CRONO_DKMS_KERNELRELEASE` to inform `install.sh` with the DKMS target 
# kernel version
export CRONO_DKMS_KERNELRELEASE="`cat crono_dkms_kver`"
printf "Crono: installing kernel module for DKMS kernel version [$CRONO_DKMS_KERNELRELEASE]\n"

# `install.sh` is on `source`
cd source
bash ./install.sh
