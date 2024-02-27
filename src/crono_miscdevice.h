#ifndef __CRONO_MISCDEVICE_H__
#define __CRONO_MISCDEVICE_H__

// _____________________________________________________________________________

#include "crono_kernel_module.h"
#include <linux/miscdevice.h>

/**
 * Miscellaneous Device Driver initialization and registration function.
 * Generate the device name using `CRONO_CONSTRUCT_MISCDEV_NAME`.
 *
 * @param crono_dev[in/out]: `crono_miscdev` related object, initialzied with
 * id.
 * @param dev[in]: the device passed in probe function.
 *
 * @return `CRONO_SUCCESS` in case of no error, or `errno` in case of error.
 */
static int _crono_miscdev_init(struct pci_dev *dev,
                               const struct pci_device_id *id,
                               struct crono_miscdev **crono_dev);
/**
 * The `open()` function in miscellaneous device driver `file_operations`
 * structure.
 * `inode` should be of a miscdev already registered by the driver.
 *
 * @return `CRONO_SUCCESS` in case of no error, `-EBUSY` (-16) in case miscdev
 * is already opened, or `-ENODEV`(-19) in case miscdev is not found in internal
 * pool.
 */
static int crono_miscdev_open(struct inode *inode, struct file *file);

/**
 * The `release()` function in miscellaneous device driver `file_operations`
 * structure.
 *
 * @return `CRONO_SUCCESS` in case of no error, `-ENODATA`(-61) in case miscdev
 * is not opened, or `-ENODEV`(-19) in case miscdev is not found in internal
 * pool.
 */
static int crono_miscdev_release(struct inode *inode, struct file *file);

/**
 * The `unlocked_ioctl()` function in miscellaneous device driver
 * `file_operations` structure.
 *
 * @param file[in]: is a valid miscdev file for the device.
 * @param cmd[in]: is a valid miscdev ioctl command defined in
 * 'crono_linux_kernel.h`.
 * @param arg[in/out]: is a pointer to valid `CRONO_SG_BUFFER_INFO` object in
 * user space memory.
 *
 * @return `CRONO_SUCCESS` in case of no error, or `errno` in case of error.
 */
static long crono_miscdev_ioctl(struct file *file, unsigned int cmd,
                                unsigned long arg);

/**
 * Fills the strcutre `dbdf` values from the device `dev` information.
 *
 * @param dev[in]: is a valid kernel pci_dev structure.
 * @param dbdf[out]: is a valid pinter to the structure to be filled.
 *
 * @return `CRONO_SUCCESS` in case of no error, or `errno` in case of error.
 */
static int _crono_get_DBDF_from_dev(struct pci_dev *dev,
                                    struct crono_dev_DBDF *dbdf);

/**
 * Internal function that locks a memory buffer using ioctl().
 * Calls 'pin_user_pages`.
 *
 * It locks the buffer, and `copy_to_user` is called for all memory.
 *
 * @param filp[in]: the file descriptor passed to ioctl.
 * @param arg[in/out]: is a valid `CRONO_SG_BUFFER_INFO` object pointer in user
 * space memory. The object should have all members set, except the `id`, which
 * will be sit inside this function upon successful return.
 *
 * @return `CRONO_SUCCESS` in case of no error, or `errno` in case of error.
 */
static int _crono_miscdev_ioctl_lock_sg_buffer(struct file *filp,
                                               unsigned long arg);

/**
 * @brief
 * Lock contiguous buffer for 32bit using dma_alloc_coherent
 *
 * @param filp
 * @param arg is an address of a valid `CRONO_CONTIG_BUFFER_INFO`
 * @return int
 */
static int _crono_miscdev_ioctl_lock_contig_buffer(struct file *filp,
                                                   unsigned long arg);

/**
 * Internal function that unlocks a memory buffer using ioctl().
 * Calls 'unpin_user_pages'
 *
 * @param filp[in]: the file descriptor passed to ioctl.
 * @param arg[in]: is a valid buffer internal id pointer in user space memory.
 * The `id` should be set based on previous call to
 * '_crono_miscdev_ioctl_lock_sg_buffer'.
 *
 * @return `CRONO_SUCCESS` in case of no error, or `errno` in case of error.
 */
static int _crono_miscdev_ioctl_unlock_sg_buffer(struct file *filp,
                                                 unsigned long arg);

/**
 * @brief
 * Lock contiguous buffer for 32bit using dma_alloc_coherent
 *
 * @param filp
 * @param arg is the wrapper id (CRONO_CONTIG_BUFFER_INFO.id) of the buffer.
 */
static int _crono_miscdev_ioctl_unlock_contig_buffer(struct file *filp,
                                                     unsigned long arg);

/**
 * Internal function that creates SG list for the buffer in `buff_wrapper`
 * and saves its address in `sgt` member. It also fills the `Page` member.
 * Function is called by `ioctl`.
 *
 * You need to obey the DMA API such that Linux can program the IOMMU or other
 * guards. Otherwise it might happen that a possible IOMMU blocks the PCI device
 * from accessing these pages. So you need to tell Linux that a PCI device is
 * accessing these pages.
 *
 * `sgt` must be freed by the driver module using `sg_free_table`.
 *
 * Prerequisites:
 *  - Buffer is locked by `_crono_miscdev_ioctl_lock_sg_buffer`.
 *  - `kernel_pages` are filled.
 *  - `buff_info.pages` is allocated.
 *
 * @param filep[in]: A valid file descriptor of the device file.
 * @param buff_wrapper[in/out]: is a valid kernel pointer to the stucture
 * `CRONO_SG_BUFFER_INFO`.
 *
 * @return `CRONO_SUCCESS` in case of no error, or `errno` in case of error.
 */
static int
_crono_miscdev_ioctl_generate_sg(struct file *filp,
                                 CRONO_SG_BUFFER_INFO_WRAPPER *buff_wrapper);

/**
 * Internal function called by `_crono_miscdev_ioctl_lock_sg_buffer`.
 *
 * Allocate and fill `kernel_pages` with the pinned pages information, and
 * `buffer_info.size` with the actual buffer size pinned, and
 * `buff_info.pages_count` with the actual number of pages pinned.
 * `kernel_pages` is allocated for number of pages for `buff_info.pages_count`.
 *
 * The buffer must not be unmapped while DMA is still active, or serious
 * system instability is guaranteed.
 *
 * Upon success, `kernel_pages` should be freed using `kvfree` when work is done
 * with it. If it failed, it will clean all up, your don't need to call
 * `_crono_miscdev_ioctl_unlock_buffer`.
 *
 * Prerequisites:
 *  - `params` is in kernel space, i.e. `copy_from_user` is called by caller for
 * `params`.
 *  - `params->pBuf` is the buffer, allocated in userspace.
 *  - `params->dwDMABufSize` is the buffer size in bytes.
 *  - `params->vmas` is optional, and can be NULL.
 *
 * @param filep[in]: A valid file descriptor of the device file.
 * @param params[in/out]: Input with values filled/allocated of `pBuf`,
 * `dwDMABufSize`, and `kernel_pages`; and ouput with values filled of
 * `ppDma[0]->kernel_pages`, and `ppDma[0]->dwPages`.
 * @param nr_per_call[in]: Number of pages to be pinned in every call to
 * `pin_user_pages`.
 *
 * @returns `CRONO_SUCCESS` in case of success, or errno in case of error.
 *
 * @see
 * `pin_user_pages`:
 * https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/mm/gup.c#n2943
 */
static int
_crono_miscdev_ioctl_pin_buffer(struct file *filp,
                                CRONO_SG_BUFFER_INFO_WRAPPER *buff_wrapper,
                                unsigned long nr_per_call);

/**
 * For CRONO_SG_BUFFER_INFO_WRAPPER:
 * Unpin, unmap Scatter/Gather list, free all memory allocated for
 * `buff_wrapper`, and remove it from the buffer information wrappers.
 * `buff_wrapper` should have been initialized using
 * `_crono_init_sg_buff_wrapper`.
 *
 * For CRONO_CONTIG_BUFFER_INFO_WRAPPER:
 * free the memory.
 *
 * Caller should free `buff_wrapper`.
 *
 * @param buff_wrapper[in/out]
 * CRONO_SG_BUFFER_INFO_WRAPPER or CRONO_CONTIG_BUFFER_INFO_WRAPPER, based
 * on .ntrn.bwt
 *
 * @return `CRONO_SUCCESS` in case of success, or errno in case of error.
 * `buff_wrapper` should point to a freed memory upon successfulreturn.
 */
static int _crono_release_buff_wrapper(void *buff_wrapper);

/**
 * The user mode part of the device driver adds a couple of register write
 * transactions to a buffer that are to be executed by the kernel module when
 * the user mode process ends/crashes. This should ensure that the DMA
 * controller of the device is disabled even if the user application crashes
 * unexpectedly
 *
 * @param arg[in]: is a pointer to valid `CRONO_SG_BUFFER_INFO` object in user
 * space memory.
 */
static int _crono_miscdev_ioctl_cleanup_setup(struct file *filp,
                                              unsigned long arg);

/**
 * Internal function to get a pointer to the corresponding device element in
 * `crono_device_types_info` array elements from the file descriptor `filep`,
 * matching the driver minor of both.
 *
 * @param filep[in]: A valid file descriptor of the device file.
 * @param devpp[out]: A valid pointer will contain a pointer to the device
 * structure object in `crono_device_types_info`.
 *
 * @returns `CRONO_SUCCESS` in case of success, or `-ENODATA` in case file not
 * found.
 */
static int _crono_get_dev_from_filp(struct file *filp, struct pci_dev **devpp);

/**
 * Get `crono_miscdev` object from misc device inode `miscdev_inode`.
 *
 * @param miscdev_inode[in]: valid inode of the misc device file.
 * @param ppcrono_miscdev[out]: pointer to the related `crono_miscdev` object in
 * the internal structures.
 *
 * @returns `CRONO_SUCCESS` in case of success, or `-ENODATA` in case file not
 */
static int
_crono_get_crono_dev_from_inode(struct inode *miscdev_inode,
                                struct crono_miscdev **ppcrono_miscdev);
/**
 * Get `crono_miscdev` object from misc device file* `filp` object.
 *
 * @param filp[in]: valid struct file of the misc device file.
 * @param crono_devpp[out]: pointer to the related `crono_miscdev` object in the
 * internal structures.
 *
 * @returns `CRONO_SUCCESS` in case of success, or `-ENODATA` in case file not
 */
static int _crono_get_crono_dev_from_filp(struct file *filp,
                                          struct crono_miscdev **crono_devpp);

/**
 * @brief Construct a new 'CRONO_SG_BUFFER_INFO_WRAPPER' object from `arg`.
 * Adds the wrapper to the buffer information wrappers list.
 * Call `_crono_release_buff_wrapper` when done working with the wrapper.
 *
 * @param filp[in]: the file descriptor passed to ioctl.
 * @param arg[in]: is a pointer to valid `CRONO_SG_BUFFER_INFO` object in user
 * space memory.
 * @param pp_buff_wrapper[out]
 */
static int
_crono_init_sg_buff_wrapper(struct file *filp, unsigned long arg,
                            CRONO_SG_BUFFER_INFO_WRAPPER **pp_buff_wrapper);

static int crono_driver_probe(struct pci_dev *dev,
                              const struct pci_device_id *id);

/**
 * If `val` is NULL, then it logs error message `err_msg` and returns `errno`.
 *
 * @param val: Variable that can be compared with NULL.
 * @param err_msg: String of no format.
 * @param errno: Error number will be returned AS IS (no -ve is added).
 */
#define LOGERR_RET_ERRNO_IF_NULL(val, err_msg, errNo)                          \
        if (NULL == val) {                                                     \
                pr_err(err_msg);                                               \
                return errNo;                                                  \
        }

/**
 * If `val` is NULL, then it logs error message `err_msg` and returns `-EINVAL`.
 *
 * @param val: Variable that can be compared with NULL.
 * @param err_msg: String of no format.
 */
#define LOGERR_RET_EINVAL_IF_NULL(val, err_msg)                                \
        LOGERR_RET_ERRNO_IF_NULL(val, err_msg, -EINVAL)

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "crono: " fmt
#define crono_kvfree(mem)                                                      \
        if (NULL != mem) {                                                     \
                kvfree(mem);                                                   \
                mem = NULL;                                                    \
        }

// _____________________________________________________________________________
#endif // #define __CRONO_MISCDEVICE_H__