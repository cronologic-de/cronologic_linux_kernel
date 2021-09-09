#ifndef _CRONO_DRIVER_H_
#define _CRONO_DRIVER_H_

#ifdef CRONO_KERNEL_MODE
#include <linux/kernel.h>
#endif // #ifdef CRONO_KERNEL_MODE

/**
 * The header file used by external applications and userspace. 
*/

typedef uint64_t UINT64;
typedef UINT64 DMA_ADDR;
typedef void *PVOID;
typedef unsigned long DWORD, *PDWORD, *LPDWORD;
#if defined(KERNEL_64BIT)
    typedef UINT64 KPTR;
#else
    typedef DWORD KPTR;
#endif

typedef struct
{
    DMA_ADDR pPhysicalAddr; // Physical address of page.
    DWORD dwBytes;          // Size of page.
} CRONO_KERNEL_DMA_PAGE;

typedef struct
{
    DWORD hDma;             // Handle of DMA buffer
#if !defined(CRONO_KERNEL_MODE) && defined(KERNEL_64BIT)
	PVOID64 pUserAddr;
#else
	PVOID	pUserAddr;        // Beginning of buffer.
#endif
    KPTR  pKernelAddr;      // Kernel mapping of kernel allocated buffer
    DWORD dwBytes;          // Size of buffer.
    DWORD dwOptions;        // Allocation options:

                // DMA_KERNEL_BUFFER_ALLOC, DMA_KBUF_BELOW_16M,
                // DMA_LARGE_BUFFER, DMA_ALLOW_CACHE, DMA_KERNEL_ONLY_MAP,
                // DMA_FROM_DEVICE, DMA_TO_DEVICE, DMA_ALLOW_64BIT_ADDRESS
    DWORD dwPages;          // Number of pages in buffer.
    DWORD hCard;            // Handle of relevant card as received from
                            // WD_CardRegister()
    CRONO_KERNEL_DMA_PAGE *Page;
#ifdef __linux__
    void** kernel_pages ;   // Array of page pointers, Needed to be cached for `unpin_user_pages`
                            // Created, filled and freed by the driver module
    void* sgt ;             // SG Table. Created, filled and freed by the driver module 
    DWORD pinned_pages_nr ; // Number of actual pages pinned, needed to be known if pin failed 
#endif
} CRONO_KERNEL_DMA;

/**
 * Structure passed to ioctl() in the `arg` parameter. 
*/
typedef struct
{
	struct vm_area_struct **vmas;
	unsigned long npages ;
    CRONO_KERNEL_DMA **ppDma ;  // Should be filled as it's the parameter sent in `CRONO_KERNEL_DMASGBufLock` $$ still used?
	void* pBuf;
	unsigned long dwOptions;
	size_t dwDMABufSize;
	unsigned long error_code;
} DMASGBufLock_parameters;


#ifdef __linux__
/**
 * CRONO PCI Driver Name passed in pci_driver structure, and is found under 
 * /sys/bus/pci/drivers after installing the driver module.
*/
#define CRONO_PCI_DRIVER_NAME       "crono_pci_driver"
/**
 * Maximum size of the misdev name string under /dev
*/
#define CRONO_MAX_DEV_NAME_SIZE     100
/**
 * pin_user_pages() number of pages per call to be accessed in misdev ioclt().
*/
#define GUP_NR_PER_CALL             512

/**
 * The macro is used to construct the misdev file name on /dev.
 * 
 * @param dbdf[in]: is a valid structure of type `crono_dev_DBDF`.
*/
#define CRONO_CONSTRUCT_MISCDEV_NAME(dev_name, device_id, dbdf) \
	snprintf(dev_name, CRONO_MAX_DEV_NAME_SIZE, "crono_%02X_%02X%02X%02X%01X", \
        device_id, dbdf.domain, dbdf.bus, dbdf.dev, dbdf.func) ;

struct crono_dev_DBDF
{
    unsigned int domain ;
    unsigned int bus ;
    unsigned int dev ;
    unsigned int func ;
};

#endif // #ifdef __linux__

#endif // #define _CRONO_DRIVER_H_