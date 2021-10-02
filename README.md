# cronologic_linux_kernel
Linux Kernel Module that allows to map large PCIe DMA ring buffers.

It has been developed by cronologic GmbH & Co. KG to support the drivers for its time measurement devices such as:
* [xHPTDC8](https://www.cronologic.de/products/tdcs/xhptdc8-pcie) 8 channel 13ps streaming time-to-digital converter
* [xTDC4](https://www.cronologic.de/products/tdcs/xtdc4-pcie) 4 channel 13ps common-start time-to-digital converter
* [Timetagger](https://www.cronologic.de/products/tdcs/timetagger) 500ps low cost TDCs
* [Ndigo6G-12](https://www.cronologic.de/products/adcs/ndigo6g-12) 6.4 Gsps 12 bit ADC digitizer board with pulse extraction and 13ps TDC.
* [Ndigo5G-10](https://www.cronologic.de/products/adcs/cronologic-ndigo5g-10) 5 Gsps 10 bit ADC digitizer board with pulse extraction

However, the module could be useful to any PCIe developer who is using large buffers that are scattered in PCI space but shall look contiguous in user space. 

The user mode part of the driver is available in a [separate repository](https://github.com/cronologic-de/cronologic_linux_usermode).

The initial code has been written by [Bassem Ramzy](https://github.com/Bassem-Ramzy) with support from [Richard Weinberger](https://github.com/richardweinberger). It is licensed unter [GPL3](LICENSE).

---

# Installation

## Overview
This installation of the driver module is very simple, and is mainly done via `insmod`, however, an installation script is provided to support wider options, like debug, add to boot, uninstall, etc...</br>
The installation firstly builds the driver module code, that's why the minimal build packages are needed as prerequisites (_mentioned in the following sections_).</br>
User should **run the installation script with every kernel version used** on the machine, and **after every upgrade** to a new kernel version.</br>

## Supported Kernel Versions
Driver is tested on Kernel Versions starting **5.0**.

## Supported Distributions
The driver is tested on the following **64-bit** distributions:
- Ubuntu (ubuntu-20.04.1-desktop-amd64) 
  - 5.4.0-42-generic
  - 5.10.0-051000-generic
  - 5.11.0-37-generic
- CentOS (CentOS-Stream-8-x86_64-20210927):
  - 5.4.150-1.e18.elrepo.x86_64
  - 5.14.9-1.el8.elrepo.x86_64
- Fedora (Fedora-Workstation-Live-x86_64-34-1.2):
  - 5.14.9-200.fc34.x86_64

## Prerequisites
### Ubuntu 
- Make sure you have **sudo** access.
- Make sure that both `make` and `gcc` packages are installed, or install them using: 
```CMD
sudo apt-get install make gcc
```
- Make sure `modules` and `headers` of your current kernel version are installed, or install them using:
```CMD
sudo apt-get install linux-headers-$(uname -r) linux-modules-$(uname -r)
```

### CentOS 
- Make sure you have **sudo** access.
- Make sure that both `make` and `gcc` packages are installed, or install them using: 
```CMD
sudo yum install gcc make
```
- If the kernel development is not installed on your system for the current kernel verision, you can install it using `elrepo` as following
```CMD
sudo rpm --import https://www.elrepo.org/RPM-GPG-KEY-elrepo.org
sudo yum install http://www.elrepo.org/elrepo-release-8.el8.elrepo.noarch.rpm
```
- If you are using _Long Term Support_ version, then, you need to run the following commands:
```CMD
sudo dnf --enablerepo=elrepo-kernel install kernel-lt
sudo dnf --enablerepo=elrepo-kernel install kernel-lt-devel
```
- If you are using _Long Mainline Stable_ Version, then, you need to run the following commands:
```CMD
sudo dnf --enablerepo=elrepo-kernel install kernel-ml
sudo dnf --enablerepo=elrepo-kernel install kernel-ml-devel
```

### Fedora
- Make sure you have **sudo** access.
- Make sure that both `make` and `gcc` packages are installed, or install them using: 
```CMD
sudo yum install gcc make
```
- If the kernel development is not installed on your system for the current kernel verision, you can install it as following
```CMD
sudo yum install kernel-devel-$(uname -r)
```

### General Notes
* You can check if `make` and `gcc` are installed by running the following commands:
```CMD
make -v
```
and
```CMD
gcc -v
```
* You can check if `kernel-devel` is installed on your system by running the following command:
```CMD
ls /lib/modules/$(uname -r)/build
```

## Installation Steps
To install the driver:
1. Clone this repository, either download it as .zip file and extract it, or use git.
```CMD
git clone https://github.com/cronologic-de/cronologic_linux_kernel
```
2. From the `Terminal`, navigate into the directory `cronologic_linux_kernel`, for instance:
```CMD
cd cronologic_linux_kernel
```
3. Run the installation script:
```CMD
sudo bash ./install.sh
```
4. And, voi la. The driver module name is `crono_pci_drvmod`.

### `install.sh` 
#### Usage
```CMD
    sudo bash ./install.sh [Options]

    [Options]
    -s  (S)top currently loaded driver (if exists). if '-s 0', currently loaded driver
        will not be stopped, otherwise, (by default) it's stopped if loaded.
        Ignored if -i is not '0', or if -u is used.
    -i  (I)nstall driver. if '-i 0', driver will not be installed, otherwise,
        (by default) driver is installed. Ignored if -u is used.
    -b  Add driver to (B)oot. If '-b 0', driver will not be added to boot, otherwise,
        (by default) driver is added. Ignored if -u is used.
    -u  Uninstall the driver and remove it from bood startup.
    -d  Display (D)ebug Messages.
    -h  Display (H)elp and usage and exit.
```

#### Hints
* The script installs the `release` build of the driver, however, you can install the `debug` build of the driver, _that is built automatically by the script_, by replaceing the line:
```CMD
DRVR_INST_SRC_PATH="./build/bin/debug_64/$DRVR_FILE_NAME.ko"
```
with 
```CMD
DRVR_INST_SRC_PATH="./build/bin/release_64/$DRVR_FILE_NAME.ko"
```
The `debug` build of the driver module prints more information to the kernel messages displayed using `dmesg`.

* In case any error is encoutnered during installation, it should be either displayed explicitly on the terminal output or written in the error log file `errlog` found on the directory root.

* If the script encountered any error, it's highly recommended to rerun the script using `-d` (_debug_) option, which should provide further information about the step and command caused that error, e.g.
```CMD
sudo bash ./install.sh -d
```

* After successful installation, you should find the module listed using `lsmod`, e.g.
```CMD
$ lsmod | grep crono
crono_pci_drvmod       53248  0
```

## Uninstall the Driver
Run the following command and it will uninstall the driver (if installed), and remove it from boot (if it's there):
```CMD
sudo bash ./install.sh -u
```
