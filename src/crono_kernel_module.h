#ifndef __CRONO_KERNEL_MODULE_H__
#define __CRONO_KERNEL_MODULE_H__
// _____________________________________________________________________________

#include <asm/uaccess.h>
#include <asm/unistd.h>
#include <linux/fcntl.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/syscalls.h>

#ifdef OLD_KERNEL_FOR_PIN
#include <linux/uaccess.h>
#endif

/**
 * Structure used to hold a clenup command information. One object per
 * command.
 */
typedef struct {
        uint32_t addr; // From the start address of BAR 0 region
        uint32_t data; // Use for 32 bit transfer.
} CRONO_KERNEL_CMD;

#include "crono_linux_kernel.h"

// Copied from crono_kernel_interface.h
enum { CRONO_KERNEL_PCI_CARDS = 8 }; // Slots max X Functions max
#define CRONO_VENDOR_ID 0x1A13
#define CRONO_SUCCESS 0
#define CLEANUP_CMD_COUNT 16

/**
 * Device information used during the driver lifetime.
 */
struct crono_device {
        /**
         * miscdevice object related to the bound device.
         */
        struct miscdevice miscdev;

        /**
         * miscdev device (file) name, e.g. `crono_06_0.
         * e.g. Two xTDC4 devices will have: dev_names[0]="crono_06_0",
         * dev_names[1]="crono_06_1".
         */
        char name[CRONO_MAX_DEV_NAME_SIZE];

        /**
         * Device DBDF.
         */
        struct crono_dev_DBDF dbdf;

        /**
         * Pointer to the devices.
         * Filled by probing function, and used mainly by `dma_map_sg`.
         * No need to deallocate it, as it points to the kernel structure passed
         * in probing function, assuming this is safe.
         */
        struct pci_dev *dev;

        /**
         * Device cleanup commands
         */
        CRONO_KERNEL_CMD cmds[CLEANUP_CMD_COUNT];
        uint32_t cmds_count; // Count of valid entries in `cmds`.
};

/**
 * Structure to hold data of a specific device type, and its probed devices.
 * e.g. holds data of all installed (probed) cards of Device ID 0x06.
 * The stucture is filled by the probe function of the driver.
 */
struct crono_device_type {
        /**
         * The Device ID of the type.
         * Is set to Non-Zero value if, at lease, one device is probed for this
         * type (ID).
         */
        int device_id;

        /**
         * Number of devices bound to the driver of this Device ID.
         * e.g. if we have two xTDC4 devices bound to the driver, and
         * device_id=0x06 then  'bound_drivers_count'=2. It's the number of
         * valid elemnts in the arrays `miscdevs` and `dev_names`.
         */
        int bound_drivers_count;

        /**
         * Array of probed devices of this device type (ID)
         */
        struct crono_device cdevs[CRONO_KERNEL_PCI_CARDS];
};

/**
 * Internal driver device ID of cronologic devices based on PCI Device ID
 */
#define CRONO_DEVICE_UNKNOWN 0x0
#define CRONO_DEVICE_HPTDC 0x1
#define CRONO_DEVICE_NDIGO5G 0x2
#define CRONO_DEVICE_NDIGO250M 0x4
#define CRONO_DEVICE_NDIGO_AVRG 0x3
#define CRONO_DEVICE_XTDC4 0x6
#define CRONO_DEVICE_FMC_TDC10 0x7
#define CRONO_DEVICE_TIMETAGGER4 0x8
#define CRONO_DEVICE_D_AVE12 0x9
#define CRONO_DEVICE_D_AVE14 0xa
#define CRONO_DEVICE_NDIGO2G14 0xb
#define CRONO_DEVICE_XHPTDC8 0xc
#define CRONO_DEVICE_NDIGO6G12 0xd

/**
 * Maximum Device ID value, should be synchronized with Device IDs
 * CRONO_DEVICE_xxx mentioned above
 */
#define CRONO_DEVICE_DEV_ID_MAX_COUNT CRONO_DEVICE_NDIGO6G12

static int crono_driver_probe(struct pci_dev *dev,
                              const struct pci_device_id *id);

typedef uint64_t DMA_ADDR;
/**
 * Internal DMA Buffer Information Wrapper, which wraps `buff_info1` object,
 * adding all members needed internally by the module.
 */
typedef struct {
        struct list_head list; // Linux list node info

        CRONO_BUFFER_INFO buff_info;

        void **kernel_pages; // Array of pointers to kernel page `page`. Needed
                             // to be cached for `unpin_user_pages`.
        void *sgt; // Scatter/Gather Table that holds the pinned pages.
        DMA_ADDR *userspace_pages; // Kernel memory has physical addresses of
                                   // userspace pages. Pages count =
                                   // `buff_info.pages_count`
        size_t pinned_size;        // Actual size pinned of the buffer in bytes.
        uint32_t pinned_pages_nr; // Number of actual pages pinned, needed to be
                                  // known if pin failed.
        struct pci_dev *devp;     // Owner device
        int app_pid; // Process ID of the userspace application that owns the
                     // buffer

} CRONO_BUFFER_INFO_WRAPPER;

/**
 * @brief Initialize the Buffer Wrappers list object. This is needed to be done
 * once, when the driver module is loaded.
 *
 * @return `CRONO_SUCCESS` in case of no error, or `errno` in case of error.
 */
static int _crono_init_buff_wrappers_list(void);

/**
 * Function displays information about the list of wrappers found in list
 * `buff_wrappers_head`.
 */
static void _crono_debug_list_wrappers(void);

/**
 * Cleanup ALL buffer wrappers
 *
 * @return `CRONO_SUCCESS` in case of no error, or `errno` in case of error.
 */
static int _crono_release_buffer_wrappers(void);

/**
 * Cleanup buffer wrappers of current userspace application process.
 *
 * @return `CRONO_SUCCESS` in case of no error, or `errno` in case of error.
 */
static int _crono_release_buffer_wrappers_cur_proc(void);

/**
 * Apply cleanup commands on registers in the first BAR (0).
 *
 * @param miscdev_inode[in]: inode of the underlying miscdev driver, of which,
 * cleanup commands will run.
 *
 * @return `CRONO_SUCCESS` in case of no error, or `errno` in case of error.
 */
static int _crono_apply_cleanup_commands(struct inode *miscdev_inode);

// _____________________________________________________________________________
#endif // #define __CRONO_KERNEL_KERNEL_MODULE_H__
