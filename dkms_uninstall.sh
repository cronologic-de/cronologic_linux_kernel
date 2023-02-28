# -----------------------------------------------------------------------------
# 		Batch script to uninstall cronologic linux PCI driver for DKMS
# -----------------------------------------------------------------------------
# Prerequisites:
# - If called from DKMS, e.g. in the POST_INSTALL, someone (mainly Makefile)
#   should save DKMS KERNELRELEASE value to file `crono_dkms_kver`, so the 
#   script reads it it and pass it to `install.sh`. 
#
# This "intermediate" script is needed to pass `-u` to `install.sh`, which
# can't be done in DKMS `POST_INSTALL`, which doesn't allow spaces in the
# command.
#
printf "Crono: uninstalling cronologic PCI kernel module...\n"

# Get pwd, supposed to be /var/lib/dkms/crono_pci_drvmod/<version>/source
CRONO_PWD=$(pwd)

# `crono_dkms_kver` should have been created using our Makefile, 
# on /var/lib/dkms/crono_pci_drvmod/<version>/ with the DKMS kernel version
cd ..

if [ ! -f crono_dkms_kver ]; then
    printf "Crono: Error, file 'crono_dkms_kver' is not found on $(pwd). Module can't be uninstalled.\n"
    exit 1
fi

# export `CRONO_DKMS_KERNELRELEASE` to inform `install.sh` with the DKMS target 
# kernel version
export CRONO_DKMS_KERNELRELEASE=`cat crono_dkms_kver`
printf "Crono: uninstalling kernel module for DKMS kernel version [$CRONO_DKMS_KERNELRELEASE]\n"

# `install.sh` is on `source`
cd source
bash ./install.sh -u
