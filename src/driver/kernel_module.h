#ifndef __CRONO_KERNEL_MODULE_H__
#define __CRONO_KERNEL_MODULE_H__ 
// _____________________________________________________________________________

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/mm.h>
#include <linux/miscdevice.h>
#include "crono_driver.h"

// Copied from crono_kernel_interface.h
enum { CRONO_KERNEL_PCI_CARDS = 100 }; // Slots max X Functions max
#define CRONO_VENDOR_ID 0x1A13
#define CRONO_SUCCESS   0
typedef void *PVOID;
typedef unsigned long DWORD, *PDWORD, *LPDWORD;

/**
 * Structure to hold data of a specific device type, 
 * e.g. holds data of all installed (probed) cards of Device ID 0x06.
 * The stucture is filled by the probe function of the driver.
*/
struct crono_device_type 
{
    /**
     * The Device ID of the type.
    */
    int device_id ;
    
    /**
     * Number of devices bound to the driver of this Device ID.
     * e.g. if we have two xTDC4 devices bound to the driver, and device_id=0x06
     * then  'bound_drivers_count'=2. 
     * It's the number of valid elemnts in the arrays `miscdevs` and `dev_names`.    
    */
    int bound_drivers_count ; 

    /**
     * Array of miscdevices objects related to the bound devices.
     * Order MUST be synchronized with devices order in `dev_names` & `dev_DBDFs`.
    */
    struct miscdevice miscdevs[CRONO_KERNEL_PCI_CARDS]; 
    
    /**
     * Array of miscdev devices (files) names, e.g. `crono_06_0. 
     * e.g. Two xTDC4 devices will have: dev_names[0]="crono_06_0",
     * dev_names[1]="crono_06_1".
     * Order MUST be synchronized with devices order in `miscdevs`. 
    */
    char dev_names[CRONO_KERNEL_PCI_CARDS][CRONO_MAX_DEV_NAME_SIZE]; 

    /**
     * Array of devices DBDF.
     * Order MUST be synchronized with devices order in `miscdevs`. 
    */
    struct crono_dev_DBDF dev_DBDFs[CRONO_KERNEL_PCI_CARDS] ;

    /**
     * Array of pointer to the devices.
     * Filled by probing function, and used mainly by `dma_map_sg`.
     * No need to deallocate it, as it points to the kernel structure passed 
     * in probing function, assuming this is safe.
    */
   struct pci_dev* devs[CRONO_KERNEL_PCI_CARDS] ;
} ;

/**
 * Maximum Device ID value, should be synchronized with Device IDs
 * CRONO_DEVICE_xxx defined in crono_interface.h.
*/
#define CRONO_DEVICE_DEV_ID_MAX_COUNT   0x0D

static int	__init crono_driver_init(void);
static void __exit crono_driver_exit(void);
static int crono_driver_probe(struct pci_dev *dev, const struct pci_device_id *id);

// _____________________________________________________________________________
#endif // #define __CRONO_KERNEL_KERNEL_MODULE_H__ 
