#ifndef __CRONO_KERNEL_MODULE_H__
#define __CRONO_KERNEL_MODULE_H__
// _____________________________________________________________________________

#include <asm/unistd.h>
#include <linux/dma-mapping.h>
#include <linux/fcntl.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/sched.h>
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
#define CRONO_SUCCESS 0 // Must be equal to its value in the interface headers
#define CLEANUP_CMD_COUNT 16
/**
 * The index of the device BAR (0 to 6) that is used to set the registers by.
 * This value is used for all devices.
 * '0' is used for the first BAR.
 */
#define DEVICE_BAR_INDEX 0

/**
 * Maximum number of miscdev generated by the module
 */
#define CRONO_MAX_MSCDEV_COUNT 32

/**
 * Device information used during the driver lifetime.
 */
struct crono_miscdev {

        /**
         * The Device ID
         */
        int device_id;

        /**
         * miscdevice object related to the bound device.
         * Is a pointer to an element in `miscdev_pool`.
         */
        struct miscdevice miscdev;

        /**
         * miscdev device (file) name, e.g. `crono_06_0003000`
         */
        char name[CRONO_DEV_NAME_MAX_SIZE];
        /**
         * Device DBDF.
         */
        struct crono_dev_DBDF dbdf;

        /**
         * Pointer to the device.
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

        /**
         * A counter of the number of times `open()` is called for this device.
         */
        uint32_t open_count;
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

typedef uint64_t DMA_ADDR;

// Buffer Wrapper Type
#define BWT_SG 1
#define BWT_CONTIG 2
typedef struct {
        int bwt;
        struct list_head list; // Linux list node info
        struct pci_dev *devp;  // Owner device
        int app_pid; // Process ID of the userspace application that owns the
                     // buffer
} CRONO_BUFFER_INFO_WRAPPER_INTERNAL;

/**
 * Internal DMA Buffer Information Wrapper, which wraps `buff_info1` object,
 * adding all members needed internally by the module.
 */
typedef struct {
        CRONO_BUFFER_INFO_WRAPPER_INTERNAL ntrn;
        void **kernel_pages; // Array of pointers to kernel page `page`. Needed
                             // to be cached for `unpin_user_pages`.
        void *sgt; // Scatter/Gather Table that holds the pinned pages.
        DMA_ADDR *userspace_pages; // Kernel memory has physical addresses of
                                   // userspace pages. Pages count =
                                   // `buff_info.pages_count`
        size_t pinned_size;        // Actual size pinned of the buffer in bytes.
        uint32_t pinned_pages_nr; // Number of actual pages pinned, needed to be
                                  // known if pin failed.

        CRONO_SG_BUFFER_INFO buff_info;

} CRONO_SG_BUFFER_INFO_WRAPPER;

typedef struct {
        CRONO_BUFFER_INFO_WRAPPER_INTERNAL ntrn;
        dma_addr_t dma_handle;

        CRONO_CONTIG_BUFFER_INFO buff_info;

} CRONO_CONTIG_BUFFER_INFO_WRAPPER;

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
