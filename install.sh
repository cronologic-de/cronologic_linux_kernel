# -----------------------------------------------------------------------------
# 		Batch script to install/uninstall cronologic linux PCI driver
# -----------------------------------------------------------------------------
#
# Prerequisites:
# - If called from DKMS, e.g. in the POST_INSTALL, someone (mainly 
#   dkms_install.sh/dkms_uninstall.sh) should export the Environment Variable 
#   `CRONO_DKMS_KERNELRELEASE` with the Kernel Release (mainly DKMS 
#   KERNELRELEASE) value BEFORE executing this script.
#
# Functions  __________________________________________________________________
#

# Command is passed as first parameter ($1).
# Command run result is in $CMD_RESULT
crono_run_command() {
    if [ -n "$DEBUG_CRONO" ]; then
        CMD_RESULT=$($1)
        echo "Command Result is: $? - <$CMD_RESULT>" 
    else
        CMD_RESULT=$($1) 2>> "$ERR_LOG_FILE"
    fi
    return $?
}

# Check lsmod, if driver is installed, DRVR_IS_INSTALLED=1 & CMD_RESULT=1, 
# otherwise, they are = null.
crono_is_driver_installed() {
    CMD_RESULT=
    if lsmod | grep "$DRVR_FILE_NAME" &> /dev/null ; then
        CMD_RESULT=1
        DRVR_IS_INSTALLED=1
        [ -n "$DEBUG_CRONO" ] && echo "Driver is installed."
    else
        DRVR_IS_INSTALLED=
        [ -n "$DEBUG_CRONO" ] && echo "Driver is not installed."
    fi
}

# Removes the driver module from system.
# Prerequisites: driver $DRVR_FILE_NAME.ko is already installed. 
# Set $DRVR_IS_INSTALLED to null upon success.
crono_remove_driver_module() 
{
    crono_run_command "sudo rmmod $DRVR_FILE_NAME.ko"
    if [ -n "$CMD_RESULT" ]; then
        printf "Crono: error removing driver: %s\n" "$CMD_RESULT"
    else 
        # Command ran successfully
        if [ -n "$DEBUG_CRONO" ]; then
            crono_is_driver_installed
            if [ -n "$CMD_RESULT" ] ; then
                echo "Error, driver is still listed in lmod!!!"
            else 
                echo "Successfully removed, driver is not listed in lmod now."
            fi
        fi 
        DRVR_IS_INSTALLED=
    fi
}

# Set $DRVR_IS_IN_KERNEL_BOOT_DIR
crono_check_driver_in_boot_directory()
{
    [ -n "$DEBUG_CRONO" ] && printf "Crono: checking driver previous boot startup settings..."
    [ -n "$DEBUG_CRONO" ] && printf "\n"
    crono_run_command "find $KERNEL_BOOT_DIR -name $DRVR_FILE_NAME.ko"
    DRVR_IS_IN_KERNEL_BOOT_DIR=$CMD_RESULT
    if [ -n "$DRVR_IS_IN_KERNEL_BOOT_DIR" ]; then
    # Driver module file is already found on /lib/modules
        [ -n "$DEBUG_CRONO" ] && echo "Driver module file is already found on /lib/modules"
    else
    # Driver module file is NOT found on /lib/modules
        [ -n "$DEBUG_CRONO" ] && echo "Driver file is not found in /lib/modules"
    fi
}

# Set $DRVR_IS_IN_STARTUP
crono_check_driver_in_boot_list() {
    [ -f $SYS_MOD_LOAD_FILE ] || echo "" >> $SYS_MOD_LOAD_FILE
    crono_run_command "grep -w $DRVR_FILE_NAME $SYS_MOD_LOAD_FILE"
    DRVR_IS_IN_STARTUP=$CMD_RESULT
    if [ -n "$DRVR_IS_IN_STARTUP" ]; then
        [ -n "$DEBUG_CRONO" ] && echo "Driver Name is found in boot $SYS_MOD_LOAD_FILE"
    else
        [ -n "$DEBUG_CRONO" ] && echo "Driver Name is not found in boot $SYS_MOD_LOAD_FILE"
    fi
}

crono_build() {
    printf "Crono: checking build prerequisites... "
    which make &> /dev/null 
    if [ $? -ne 0 ]; then
        printf "Error: <make> is not found, please install it\n"
        exit 1
    fi
    which gcc &> /dev/null 
    if [ $? -ne 0 ]; then
        printf "Crono: error: <gcc> is not found, please install it\n"
        exit 1
    fi
#    which g++ &> /dev/null 
#    if [ $? -ne 0 ]; then
#        printf "Crono: error: <g++> is not found, please install it\n"
#        exit 1
#    fi
    printf "done\n"

    printf "Crono: building driver code... \n"
    sudo make clean -s 2>> "$ERR_LOG_FILE"
    sudo make -s 2>> "$ERR_LOG_FILE"
    crono_run_command "find $DRVR_INST_SRC_PATH"
    if [ -z "$CMD_RESULT" ]; then
        # Driver module file is not found 
        printf "Crono: error Building the Driver: check log <$ERR_LOG_FILE> for details\n"
        exit 1
    fi
    printf "done\n"
}

#
# Initialize Variables  _______________________________________________________
#
# Kernel & machine variables
OS_REDHAT=
[ -f /etc/redhat-release ] && OS_REDHAT=1 
if [ -n "$OS_REDHAT" ] ; then
    SYS_MOD_LOAD_FILE=/usr/lib/modules-load.d/cronologic.conf
else
    SYS_MOD_LOAD_FILE=/etc/modules
fi    

MACHINE_TYPE=`uname -m`

if [ ${MACHINE_TYPE} == 'x86_64' ]; then
    # 64-bit stuff here
    DRVR_FILE_NAME="crono_pci_drvmod"
    RUNNING_KERNEL_VERSION=`uname -r`
    if [ -z "$CRONO_DKMS_KERNELRELEASE" ]; then
    # DKMS is NOT the caller, get info normally
        TARGET_KERNEL_VERSION=`uname -r`
        DRVR_INST_SRC_PATH="../build/linux/bin/debug_64/$DRVR_FILE_NAME.ko"
    else
    # DKMS IS the caller, use its info instead
    	TARGET_KERNEL_VERSION=$CRONO_DKMS_KERNELRELEASE
        DRVR_INST_SRC_PATH="/lib/modules/$TARGET_KERNEL_VERSION/updates/dkms/$DRVR_FILE_NAME.ko"
        if [ "$TARGET_KERNEL_VERSION" != "$RUNNING_KERNEL_VERSION" ]; then
            # DKMS installs for a target kernel version that is different
            # than the running kernel version, e.g. upgrading system.
            DMKS_DIFF_TARGET=1
        fi
    fi
    [ -n "$DEBUG_CRONO" ] && printf "Crono: DRVR_INST_SRC_PATH: $DRVR_INST_SRC_PATH\n"
else
    echo "Machine type <${MACHINE_TYPE}> is not supported"
    exit 1
fi
KERNEL_BOOT_DIR="/lib/modules/$TARGET_KERNEL_VERSION/kernel/drivers/pci"

# Script options variables
DRVR_IS_IN_KERNEL_BOOT_DIR=
DONT_STOP_LOADED_DRVR=
DONT_INSTALL_DRVR=
DONT_ADD_TO_BOOT=
UINSTALL_DRVR=
DEBUG_CRONO=

# Common functions variables
CMD_RESULT=
ERR_LOG_FILE="errlog"

# Installation Script About Messages
ABOUT_MSG="
This script installs/uninstalls cronologic PCI driver on Linux system. 
\n"

USAGE_MSG="
Usage:

    sudo bash ./install.sh [Options]

    [Options]
    -s  (S)top currently loaded driver (if exists). if '-s 0', currently loaded driver
        will not be stopped, otherwise, (by default) it's stopped if loaded.
        Ignored if -i is not '0', or if -u is used.
    -i  (I)nstall driver. if '-i 0', driver will not be installed, otherwise,
        (by default) driver is installed. Ignored if -u is used.
    -b  Add driver to (B)oot. If '-b 0', driver will not be added to boot, otherwise,
        (by default) driver is added. Ignored if -u is used.
    -u  Uninstall the driver and remove it from boot startup.
    -d  Display (D)ebug Messages.
    -h  Display (H)elp and usage and exit.
"

printf "$ABOUT_MSG"

#
# Get Script Options __________________________________________________________
#
while getopts s:i:b:p:hdu option_name
do
    case $option_name in
    b) if [ $OPTARG == "0" ]; then DONT_ADD_TO_BOOT=1; fi;;
    s) if [ $OPTARG == "0" ]; then DONT_STOP_LOADED_DRVR=1; fi;;
    i) if [ $OPTARG == "0" ]; then DONT_INSTALL_DRVR=1; fi;;
    u) UINSTALL_DRVR=1;;
    h) echo "$USAGE_MSG"
       [ -n "$DEBUG_CRONO" ] && set +x; 
       exit 0;;
    d) DEBUG_CRONO=1;;
    ?) echo "$USAGE_MSG"
       [ -n "$DEBUG_CRONO" ] && set +x; 
       exit 2;;
    esac
done

#
# Initialization  _____________________________________________________________
#
[ -n "$DEBUG_CRONO" ] && set -x; 
[ -n "$DEBUG_CRONO" ] && printf "Crono: driver file name: <%s>, path: <%s>\n" "$DRVR_FILE_NAME" "$DRVR_INST_SRC_PATH"

#
# Get Kernel Information ______________________________________________________
#
printf "Crono: checking kernel information... "
[ -n "$DEBUG_CRONO" ] && printf "\n"
if [ -z "$TARGET_KERNEL_VERSION" ]; then
    printf "Crono: error getting kernel level, try running using -d option"
    [ -n "$DEBUG_CRONO" ] && set +x; 
    exit 2
else
    [ -n "$DEBUG_CRONO" ] && printf "Crono: Target Kernel Version is: %s\n" "$TARGET_KERNEL_VERSION"
fi
echo "done"

#
# Building the driver code  ___________________________________________________
#
if [ -z "$UINSTALL_DRVR" ]; then
    if [ -z "$CRONO_DKMS_KERNELRELEASE" ]; then
    # Not called from DKMS, go ahead and build.
	    crono_build
    else
    # Don't build, as DKMS calls make, and .ko file is got from there instead
	    printf "Crono: Kernel Module will be got from DKMS build folder instead of building it.\n"
    fi
fi

#
# Uninstall the driver ________________________________________________________
# Uninstallation from "Running Kernel".
#
# If called from DKMS: only uninstall if the "Target Kenrel" is the "Running
# Kernel" (i.e. KDMS is uninstalling from the currently running kernel), 
# otherwise (i.e. when $DMKS_DIFF_TARGET has value, DKMS is uninstalling 
# from a different kernel version), don't uninstall the module from the current 
# running kernel.
#
if [ -n "$UINSTALL_DRVR" ] && [ -z "$DMKS_DIFF_TARGET" ]; then
    printf "Crono: uninstalling driver... "
    [ -n "$DEBUG_CRONO" ] && printf "\n"

    crono_is_driver_installed
    if [ -n "$CMD_RESULT" ] ; then
        # Remove installed module if found
        crono_remove_driver_module
        [ "$DRVR_IS_INSTALLED" == "" ] && echo "done"
    else
        echo "done"
    fi
    
    printf "Crono: unsetting driver from start on boot... "

    # Remove file from boot driver folder if found
    crono_check_driver_in_boot_directory
    ERR_REMOVING_FROM_BOOT=
    if [ -n "$DRVR_IS_IN_KERNEL_BOOT_DIR" ]; then
        FILE_PATH_ON_BOOT=$KERNEL_BOOT_DIR/$DRVR_FILE_NAME.ko
        [ -n "$DEBUG_CRONO" ] && printf "Crono: removing the file <%s>\n" "$FILE_PATH_ON_BOOT"
        crono_run_command "sudo rm $FILE_PATH_ON_BOOT"
        if [ -z "$CMD_RESULT" ]; then 
            DRVR_IS_IN_KERNEL_BOOT_DIR=
            [ -n "$DEBUG_CRONO" ] && echo "Crono: succesfully removed the file"
        else
            [ -n "$DEBUG_CRONO" ] && printf "Crono: error removing the file <%s>" "$CMD_RESULT"
            ERR_REMOVING_FROM_BOOT=1
        fi
    fi

    # Remove file from boot modules list if found
    crono_check_driver_in_boot_list
    if [ -n "$DRVR_IS_IN_STARTUP" ]; then
        [ -n "$DEBUG_CRONO" ] && printf "Crono: removing text <%s> from $SYS_MOD_LOAD_FILE\n" "$DRVR_FILE_NAME"
        crono_run_command "sudo sed -i s/$DRVR_FILE_NAME// $SYS_MOD_LOAD_FILE"
        crono_check_driver_in_boot_list
        if [ -n "$DRVR_IS_IN_STARTUP" ]; then
            echo "Crono: error removing the file from boot list"
            ERR_REMOVING_FROM_BOOT=1
        fi
    fi
    [ -z "$ERR_REMOVING_FROM_BOOT" ] && echo "done"

    # End, cleanup
    [ -n "$DEBUG_CRONO" ] && set +x; 

    # Do nothing more
    exit 0
fi

#
# Check currently installed driver  ___________________________________________
#
printf "Crono: checking currently installed (loaded) driver...  "
[ -n "$DEBUG_CRONO" ] && printf "\n"

crono_is_driver_installed

echo "done"

#
# Stop Loaded Driver  _____________________________________________________________
# Stop from "Running Kernel".
#
# If called from DKMS: only stop driver if the "Target Kenrel" is the "Running
# Kernel" (i.e. KDMS is calls for the currently running kernel), otherwise 
# (i.e. when $DMKS_DIFF_TARGET has value, DKMS is called for a different kernel 
# version), don't stop the module on the current running kernel.
#
if [ -n "$DRVR_IS_INSTALLED" ] && [ -z "$DONT_STOP_LOADED_DRVR" ] \
   && [ -z "$DMKS_DIFF_TARGET" ]; then
# Driver is installed and can stop it
    printf "Crono: uninstalling the installed (loaded) driver... "
    [ -n "$DEBUG_CRONO" ] && printf "\n"
    crono_remove_driver_module
    [ "$DRVR_IS_INSTALLED" == "" ] && echo "done"
else
    [ -n "$DEBUG_CRONO" ] && printf "Crono: will not stop installed driver, "\
        "either not installed or option not to, "\
        "or DKMS runs on a different kernel version.\n" "$CMD_RESULT"
fi

#
# Install driver  _____________________________________________________________
# Install on "Running Kernel".
#
# If called from DKMS: only install driver if the "Target Kenrel" is the "Running
# Kernel" (i.e. KDMS is calls for the currently running kernel), otherwise 
# (i.e. when $DMKS_DIFF_TARGET has value, DKMS is called for a different kernel 
# version), don't isntall the module on the current running kernel.
#
if [ -z "$DONT_INSTALL_DRVR" ] && [ -z "$DMKS_DIFF_TARGET" ]; then
    if [ -n "$DRVR_IS_INSTALLED" ]; then
    # Uninstall the installed driver
        printf "Crono: uninstalling the installed (loaded) driver... "
        [ -n "$DEBUG_CRONO" ] && printf "\n"
        crono_remove_driver_module
        [ "$DRVR_IS_INSTALLED" == "" ] && echo "done"
    fi
    printf "Crono: installing driver... "
    [ -n "$DEBUG_CRONO" ] && printf "\n"
    crono_run_command "sudo insmod $DRVR_INST_SRC_PATH"
    if [ -n "$CMD_RESULT" ]; then
        printf "Crono: error installing driver: %s\n" "$CMD_RESULT"
    else 
        echo "done"
        DRVR_IS_INSTALLED=1
    fi
else
    [ -n "$DEBUG_CRONO" ] && printf "Crono: will not install driver, option not to.\n" "$CMD_RESULT"
fi

#
# Adding to boot  _____________________________________________________________
#
if [ -z "$DONT_ADD_TO_BOOT" ]; then 
# Allowed to add driver to boot
    printf "Crono: setting driver to start on boot... "
    [ -n "$DEBUG_CRONO" ] && printf "\n"

    # Copy/replace the driver module  _________________________________________
    crono_check_driver_in_boot_directory 
    crono_run_command "sudo cp $DRVR_INST_SRC_PATH /lib/modules/$TARGET_KERNEL_VERSION/kernel/drivers/pci/$DRVR_FILE_NAME.ko"
    [ -n "$CMD_RESULT" ] && printf "Crono: error copying the file: %s" "$CMD_RESULT"
    crono_run_command "sudo depmod"
    [ -n "$CMD_RESULT" ] && printf "Crono: error updating modules dependencies: %s" "$CMD_RESULT"
 
    # Add driver to boot list  ________________________________________________
    crono_check_driver_in_boot_list
    if [ -z "$DRVR_IS_IN_STARTUP" ]; then
        echo $DRVR_FILE_NAME | sudo tee -a $SYS_MOD_LOAD_FILE > /dev/null
        [ -n "$DEBUG_CRONO" ] && echo "Crono: Driver Name is successfully added to boot $SYS_MOD_LOAD_FILE"
    fi

    echo "done"
fi
