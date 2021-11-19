#ifndef __CRONO_MISCDEVICE_H__
#define __CRONO_MISCDEVICE_H__

// _____________________________________________________________________________

#include "crono_kernel_module.h"
#include <linux/miscdevice.h>

/**
 * Miscellaneous Device Driver initialization and registration function.
 * Generate the device name using `CRONO_CONSTRUCT_MISCDEV_NAME`.
 *
 * @param dev_type[in/out]: `crono_device_types_info` related object.
 * @param dev[in]: the device passed in probe function.
 *
 * @return `CRONO_SUCCESS` in case of no error, or `errno` in case of error.
 */
static int _crono_miscdev_type_init(struct crono_device_type *dev_type,
                                    struct pci_dev *dev);
/**
 * Miscellaneous Device Driver exit function, unregister all registered devices
 * of device type `dev_type`. It doesn't free any memory related to `dev_type`.s
 *
 * @param dev_type[in]: a valid device type object.
 *
 * @return `CRONO_SUCCESS` in case of no error, or `errno` in case of error.
 */
static void _crono_miscdev_type_exit(struct crono_device_type *dev_type);

/**
 * The `open()` function in miscellaneous device driver `file_operations`
 * structure.
 */
static int crono_miscdev_open(struct inode *inode, struct file *file);

/**
 * The `release()` function in miscellaneous device driver `file_operations`
 * structure.
 */
static int crono_miscdev_release(struct inode *inode, struct file *file);

/**
 * The `unlocked_ioctl()` function in miscellaneous device driver
 * `file_operations` structure.
 *
 * @param file[in]: is a valid miscdev file for the device.
 * @param cmd[in]: is a valid miscdev ioctl command defined in
 * 'crono_linux_kernel.h`.
 * @param arg[in/out]: is a pointer to valid `CRONO_BUFFER_INFO` object in user
 * space memory.
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
 * Calls pr_info for a formatted DDDD:BB:DD.F output.
 *
 * @param dbdf[in]: is a valid pinter to the structure to be printed.
 *
 * @return `CRONO_SUCCESS` in case of no error, or `errno` in case of error.
 */
static int _crono_pr_info_DBDF(struct crono_dev_DBDF *dbdf);

/**
 * Internal function that locks a memory buffer using ioctl().
 * Calls 'pin_user_pages`.
 *
 * It locks the buffer, and `copy_to_user` is called for all memory.
 *
 * @param filp[in]: the file descriptor passed to ioctl.
 * @param arg[in/out]: is a valid `CRONO_BUFFER_INFO` object pointer in user
 * space memory. The object should have all members set, except the `id`, which
 * will be sit inside this function upon successful return.
 *
 * @return `CRONO_SUCCESS` in case of no error, or `errno` in case of error.
 */
static int _crono_miscdev_ioctl_lock_buffer(struct file *filp,
                                            unsigned long arg);

/**
 * Internal function that unlocks a memory buffer using ioctl().
 * Calls 'unpin_user_pages'
 *
 * @param filp[in]: the file descriptor passed to ioctl.
 * @param arg[in]: is a valid `CRONO_BUFFER_INFO` object pointer in user
 * space memory. The object should have `id` set based on previous call to
 * '_crono_miscdev_ioctl_lock_buffer'.
 *
 * @return `CRONO_SUCCESS` in case of no error, or `errno` in case of error.
 */
static int _crono_miscdev_ioctl_unlock_buffer(struct file *filp,
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
 *  - Buffer is locked by `_crono_miscdev_ioctl_lock_buffer`.
 *  - `kernel_pages` are filled.
 *  - `buff_info.pages` is allocated.
 *
 * @param filep[in]: A valid file descriptor of the device file.
 * @param buff_wrapper[in/out]: is a valid kernel pointer to the stucture
 * `CRONO_BUFFER_INFO`.
 *
 * @return `CRONO_SUCCESS` in case of no error, or `errno` in case of error.
 */
static int
_crono_miscdev_ioctl_generate_sg(struct file *filp,
                                 CRONO_BUFFER_INFO_WRAPPER *buff_wrapper);

/**
 * Internal function called by `_crono_miscdev_ioctl_lock_buffer`.
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
                                CRONO_BUFFER_INFO_WRAPPER *buff_wrapper,
                                unsigned long nr_per_call);

/**
 * Unpin, unmap Scatter/Gather list, free all memory allocated for
 * `buff_wrapper`, and remove it from the buffer information wrappers.
 * `buff_wrapper` should have been initialized using `_crono_init_buff_wrapper`.
 *
 * Caller should free `buff_wrapper`.
 *
 * @param buff_wrapper[in/out]
 *
 * @return `CRONO_SUCCESS` in case of success, or errno in case of error.
 * `buff_wrapper` should point to a freed memory upon successfulreturn.
 */
static int _crono_release_buff_wrapper(CRONO_BUFFER_INFO_WRAPPER *buff_wrapper);

/**
 * The user mode part of the device driver adds a couple of register write
 * transactions to a buffer that are to be executed by the kernel module when
 * the user mode process ends/crashes. This should ensure that the DMA
 * controller of the device is disabled even if the user application crashes
 * unexpectedly
 *
 * @param arg[in]: is a pointer to valid `CRONO_BUFFER_INFO` object in user
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
 * @brief Construct a new 'CRONO_BUFFER_INFO_WRAPPER' object from `arg`.
 * Adds the wrapper to the buffer information wrappers list.
 * Call `_crono_release_buff_wrapper` when done working with the wrapper.
 *
 * @param filp[in]: the file descriptor passed to ioctl.
 * @param arg[in]: is a pointer to valid `CRONO_BUFFER_INFO` object in user
 * space memory.
 * @param pp_buff_wrapper[out]
 */
static int
_crono_init_buff_wrapper(struct file *filp, unsigned long arg,
                         CRONO_BUFFER_INFO_WRAPPER **pp_buff_wrapper);

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

/**
 * Loops on `crono_device_types_info` array, and passes types have registered
 * devices. Code following this macro will guarantee that `dev_type` has at
 * least one device registered, and is of index = `dt_index` in this array.
 *
 * The following local variables can be used as following:
 *  `dt_index`: loop index that will contain the index of the device type
 *              in array `crono_device_types_info`.
 *  `dev_type`: a pointer to `crono_device_type` struct object holds pointer to
 *              the registerd device in `crono_device_types_info`.
 *
 * Prerequisites:
 *  - `dt_index` of type `int' is defined.
 *  - `dev_type` of type `struct crono_device_type*` is defined.
 *
 * Macro block must be closed using `for_each_active_device_types_end`.
 */
#define for_each_active_device_types(dev_type)                                 \
        for (dt_index = 0; dt_index < CRONO_DEVICE_DEV_ID_MAX_COUNT;           \
             dt_index++) {                                                     \
                if (0 ==                                                       \
                    crono_device_types_info[dt_index].bound_drivers_count)     \
                        continue;                                              \
                dev_type = &crono_device_types_info[dt_index];
/**
 * Macro must be used to close code block of `for_each_active_device_types`
 */
#define for_each_active_device_types_end }
/**
 * Loop on `crono_device_types_info` array active types, and passe devices.
 * Code following this macro will guarantee that `dev_type` has at least
 * one device registered, and is of index = `dt_index` in this array.
 * And, iteration device of this type is of index = `dev_index`.
 *
 * The following local variables are defined, and can be used as following:
 *  `dt_index`:  loop index that will contain the index of the device type
 *               in array `crono_device_types_info`.
 *  `dev_type`:  a pointer to `crono_device_type` struct object in
 *               `crono_device_types_info` holds the registerd device(s) in.
 *  `dev_index`: loop index that contain the index of the registered device
 *               in `crono_device_types_info` member arrays.
 *
 * Prerequisites:
 *  - of `iterate_active_device_types`.
 *  - `dev_index` of type `int` is defined.
 *
 * Macro block must be closed using `end_iterate_devs_of_active_device_types`.
 */
#define for_each_dev_of_active_device_types(dev_type, dev_index)               \
        for_each_active_device_types(dev_type) {                               \
                for (dev_index = 0; dev_index < dev_type->bound_drivers_count; \
                     dev_index++) {
/**
 * Macro must be used to close code block of
 * `iterate_devs_of_active_device_types`
 */
#define for_each_dev_of_active_device_types_end                                \
        }                                                                      \
        }                                                                      \
        for_each_active_device_types_end;

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