#ifndef __CRONO_MISCDEVICE_H__
#define __CRONO_MISCDEVICE_H__
// _____________________________________________________________________________

#include "crono_kernel_module.h"
#include <linux/miscdevice.h>

static int _crono_miscdev_type_init(struct crono_device_type *dev_type,
                                    struct pci_dev *dev);
static void _crono_miscdev_type_exit(struct crono_device_type *dev_type);

static int crono_miscdev_open(struct inode *inode, struct file *file);
static int crono_miscdev_release(struct inode *inode, struct file *file);

/**
 * miscdev ioctl function.
 *
 * @param file[in]: is a valid miscdev file for the device.
 * @param cmd[in]: is a valid miscdev ioctl command defined in 'crono_driver.h`.
 * @param arg[in/out]: is a pointer to valid `DMASGBufLock_parameters`
 * structure.
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
 * @param filp[in]: the file descriptor passed to ioctl.
 * @param arg[in/out]: is `DMASGBufLock_parameters*` (referred as `params`).
 * @param params[in/out]: is a valid kernel pointer to the stucture.
 * @param params->dwOptions[in]: passed to `pin_user_pages` as `gup_flags`.
 * @param params->pBuf[in]: buffer that is allocated by the caller and will be
 * pinned.
 * @param params->dwDMABufSize[in/out]: passes the size of `params->pBuf` in
 * bytes allocated by the caller, and is updated to the size in bytes of the
 * pages pinned successfully upon return.
 * @param params->ppDma[0]->kernel_pages[in/out]: Holds the address of memory
 * allocated using the driver. Caller shouldn't declare, allocate, nor fill it,
 * and is NOT expected to use it either, just keep it and transfer it in and
 * out.
 * @param params->ppDma[0]->dwPages[out]: holds the actual number of the pages
 * pinned in `params->ppDma[0]->kernel_pages`.
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
 * @param arg[in/out]: is DMASGBufLock_parameters* (referred as `params`).
 * @param params[in]: is a valid kernel pointer to the stucture.
 * @param params->ppDma[0]->kernel_pages[in]: Holds the address of pages memory.
 * @param params->ppDma[0]->dwPages[in]: holds the number of the pages
 * pinned in `params->ppDma[0]->kernel_pages`.
 *
 * @return `CRONO_SUCCESS` in case of no error, or `errno` in case of error.
 */
static int _crono_miscdev_ioctl_unlock_buffer(struct file *filp,
                                              unsigned long arg);

/**
 * Internal function that creates SG list for the buffer in `params`
 * and saves its address in `pSG` member. It also fills the `Page` member.
 * Function is called by `ioctl`.
 *
 * You need to obey the DMA API such that Linux can program the IOMMU or other
 * guards. Otherwise it might happen that a possible IOMMU blocks the PCI device
 * from accessing these pages. So you need to tell Linux that a PCI device is
 * accessing these pages.
 *
 * `params->ppDma[0]->pSG` must be freed by the driver module using
 * `sg_free_table`.
 *
 * Prerequisites:
 *  - Buffer is locked.
 *  - Kernel Pages (`params->ppDma[0]->kernel_pages`) are filled.
 *  - Crono Pages (`params->ppDma[0]->Pages`) is allocated.
 *
 * @param filep[in]: A valid file descriptor of the device file.
 * @param params[in/out]: is a valid kernel pointer to the stucture.
 * @param params->ppDma[0]->kernel_pages[in]: array of pointers to the pages
 * pinned.
 * @param params->ppDma[0]->dwPages[in]: holds the actual number of the pages
 * pinned in `params->ppDma[0]->kernel_pages`.
 * @param params->ppDma[0]->pSG[out]: is allocated by the function.
 * @param params->ppDma[0]->Page[in/out]: is allocated by the caller, of
 * number of elements = `params->ppDma[0]->dwPages`.
 *
 * @return `CRONO_SUCCESS` in case of no error, or `errno` in case of error.
 */
static int _crono_miscdev_ioctl_generate_sg(struct file *filp,
                                            DMASGBufLock_parameters *params);

/**
 * Internal function called by `_crono_miscdev_ioctl_lock_buffer`.
 *
 * Allocate and fill `params->ppDma[0]->kernel_pages` with the pinned pages
 * information, and `params->ppDma[0]->dwBytes` with the actual buffer size
 * pinned, and `params->npages` with the actual number of pages pinned.
 * `params->ppDma[0]->kernel_pages` is allocated for number of pages for
 * `params->dwDMABufSize`.
 *
 * The buffer must not be unmapped while DMA is still active, or serious
 * system instability is guaranteed.
 *
 * Upon success, `params->ppDma[0]->kernel_pages` should be freed using `kvfree`
 * when work is done with it.
 * If it failed, it will clean all up, your don't need to call
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
static int _crono_miscdev_ioctl_pin_buffer(struct file *filp,
                                           DMASGBufLock_parameters *params,
                                           unsigned long nr_per_call);

/**
 * Internal function that cleans up the parameters:
 *  - Unpin the `kernel_pages` if pinned.
 *  - Free `kernel_pages` if allocated.
 * All information is got from `ppDma[0]` element.
 *
 * @param filep[in]: A valid file descriptor of the device file.
 * @param params[in/out]: A valid structure.
 *
 * @returns `CRONO_SUCCESS` in case of success, or errno in case of error.
 */
static int _crono_miscdev_ioctl_cleanup_buffer(struct file *filp,
                                               DMASGBufLock_parameters *params);

/**
 *
 * The user mode part of the device driver adds a couple of register write
 * transactions to a buffer that are to be executed by the kernel module when
 * the user mode process ends/crashes. This should ensure that the DMA
 * controller of the device is disabled even if the user application crashes
 * unexpectedly
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
 * Macro is mainly used by internal functions called by `crono_miscdev_ioctl`
 * Validate `arg`.
 * Check whether `arg` memory is accessible, using `copy_from_user`, set
 * ret = `-EFAULT` and go to error if not.
 * Copy `arg` to `params` and `orig_parms` using `copy_from_user`.
 * `IOCTL_DONE_WITH_LOCK_PARAMS` must be called when done working with it.
 * Should be the only place that fills `orig_parms`, shouldn't be changed
 * afterwards.
 *
 * @param arg[in]: is `DMASGBufLock_parameters*` (referred as `params`).
 * @param params[out]: a valid `DMASGBufLock_parameters` struct, holds a copy of
 * `arg`.
 * @param arg_orig[out]: `DMASGBufLock_parameters` struct, holds a copy of
 * `arg`.
 * @param ret[out]: of type `int`, will be set to the error number in case of
 * error.
 * @param err_goto_label[in]: goto label in case any error is encountered,
 *  setting `ret` to error code.
 */
#define IOCTL_VALIDATE_LOCK_PARAMS(arg, params, arg_orig, ret, err_goto_label) \
        if (0 == arg) {                                                        \
                pr_err("Invalid parameter locking buffer");                    \
                ret = -EINVAL;                                                 \
                goto err_goto_label;                                           \
        }                                                                      \
        if (copy_from_user(&params, (void __user *)arg,                        \
                           sizeof(DMASGBufLock_parameters))) {                 \
                ret = -EFAULT;                                                 \
                goto err_goto_label;                                           \
        }                                                                      \
        if (copy_from_user(&arg_orig, (void __user *)arg,                      \
                           sizeof(DMASGBufLock_parameters))) {                 \
                ret = -EFAULT;                                                 \
                goto err_goto_label;                                           \
        }

/**
 * Copy back the structure `arg_orig` to `arg`, just to make sure kernel
 * APIs are followed, but there should be no change in the direct elements of
 * `arg_orig`.
 *
 * Prerequisites:
 *  - `IOCTL_VALIDATE_LOCK_PARAMS` is called in the same block {}.
 *  - `arg_orig` is a copy of `arg`.
 *
 * @param arg[in/out]: is `DMASGBufLock_parameters*` (referred as `params`).
 * @param params[in]: a valid `DMASGBufLock_parameters` struct.
 * @param arg_orig[out]: `DMASGBufLock_parameters` struct, holds a copy of
 * `arg`.
 * @param err_goto_label: goto label in case any error is encountered,
 *  setting `ret` to error code.
 */
#define IOCTL_DONE_WITH_LOCK_PARAMS(arg, arg_orig, err_goto_label)             \
        if (copy_to_user((void __user *)arg, &arg_orig,                        \
                         sizeof(DMASGBufLock_parameters))) {                   \
                ret = -EFAULT;                                                 \
                goto err_goto_label;                                           \
        }

/**
 * Macro is mainly used by internal functions called by `crono_miscdev_ioctl`.
 * `IOCTL_VALIDATE_LOCK_PARAMS` must be called prior to calling this macro.
 * Allocate, and validate `pDma`, assign it to `params.ppDma`. Check
 * whether `arg` memory is accessible, using `copy_from_user`, return `EFAULT`
 * if not, and copy it to `params`.
 * `pDma` should be freed when done working with it.
 * IT DOES NOT copy any content of member pointers, e.g. `Page`, and
 * `kernel_pages`. `IOCTL_COPY_KERNEL_DMA_TO_USER` must be called when don
 * working with it.
 *
 * Prerequisites:
 *  - `pDma` of type `CRONO_KERNEL_DMA*` is defined.
 *  - `IOCTL_VALIDATE_LOCK_PARAMS` is called in the same block {}.
 *
 * @param params[in/out]: a valid `DMASGBufLock_parameters` struct.
 * @param pDma[out]: `CRONO_KERNEL_DMA*`
 * @param arg_dma_orig[out]: `CRONO_KERNEL_DMA`, holds a copy of
 * `arg->ppDma[0]`.
 * @param err_goto_label: goto label in case any error is encountered,
 * setting `ret` to error code.
 */
#define IOCTL_COPY_KERNEL_DMA_FROM_USER(params, pDma, arg_dma_orig,            \
                                        err_goto_label)                        \
        pDma =                                                                 \
            (CRONO_KERNEL_DMA *)kmalloc(sizeof(CRONO_KERNEL_DMA), GFP_KERNEL); \
        if (NULL == pDma) {                                                    \
                pr_err("Error allocating DMA struct");                         \
                ret = -ENOMEM;                                                 \
                goto err_goto_label;                                           \
        }                                                                      \
        if (0 == params.ulpDma) {                                              \
                pr_err("Invalid value of ulpDma passed, 0x%lx",                \
                       params.ulpDma);                                         \
                ret = -EFAULT;                                                 \
                goto err_goto_label;                                           \
        }                                                                      \
        pr_debug("Copying DMA structure from address <0x%lx>", params.ulpDma); \
        params.ppDma = &pDma;                                                  \
        if (copy_from_user(pDma, (void __user *)(params.ulpDma),               \
                           sizeof(CRONO_KERNEL_DMA))) {                        \
                ret = -EFAULT;                                                 \
                goto err_goto_label;                                           \
        }                                                                      \
        params.ppDma = &pDma;                                                  \
        if (copy_from_user(&arg_dma_orig, (void __user *)(params.ulpDma),      \
                           sizeof(CRONO_KERNEL_DMA))) {                        \
                ret = -EFAULT;                                                 \
                goto err_goto_label;                                           \
        }
/**
 * Macro is mainly used by internal functions called by `crono_miscdev_ioctl`.
 * Make sure to keep `arg->ppDma[0]->Page` AS IS before and after calling to
 * `copy_to_user` as it's allocated using userspace.
 * On the contrary, don't do  that with `kernel_pages` and `sgt`, as they are
 * managed by kernel module and need to be copied AS IS to `arg`.
 *
 * Prerequisites:
 *  - `Page_orig` of type `CRONO_KERNEL_DMA_PAGE*` is defined.
 *  - `IOCTL_COPY_KERNEL_DMA_FROM_USER` is called in the same block {}.
 *
 * @param arg[in/out]: is `DMASGBufLock_parameters*` (referred as `params`).
 * @param params[in]: a valid `DMASGBufLock_parameters` struct.
 * @param arg_dma_orig[in]: `CRONO_KERNEL_DMA`, holds a copy of `arg->ppDma[0]`.
 * @param Page_orig[out]: is `CRONO_KERNEL_DMA_PAGE *`.
 * @param ret[out]: of type `int`, will be set to the error number in case of
 * error.
 * @param err_goto_label: goto label in case any error is encountered,
 * setting `ret` to error code.
 */
#define IOCTL_COPY_KERNEL_DMA_TO_USER(arg, params, arg_dma_orig, Page_orig,    \
                                      ret, err_goto_label)                     \
        Page_orig = arg_dma_orig.Page;                                         \
        memcpy(&arg_dma_orig, params.ppDma[0], sizeof(CRONO_KERNEL_DMA));      \
        arg_dma_orig.Page = Page_orig; /* Restore it */                        \
        if (copy_to_user((void __user *)(params.ulpDma), &arg_dma_orig,        \
                         sizeof(CRONO_KERNEL_DMA))) {                          \
                ret = -EFAULT;                                                 \
                goto err_goto_label;                                           \
        }                                                                      \
        IOCTL_CLEAN_KERNEL_DMA(params, err_goto_label);
/**
 * 'IOCTL_COPY_KERNEL_DMA_FROM_USER' must be called prior to calling this macro.
 * Clean up all work done by `IOCTL_COPY_KERNEL_DMA_FROM_USER`.
 *
 * Prerequisites:
 *  - `IOCTL_COPY_KERNEL_DMA_FROM_USER` is called in the same block {}.
 *
 * @param params[in]: a valid `DMASGBufLock_parameters` struct.
 * @param err_goto_label: goto label in case any error is encountered,
 * setting `ret` to error code.
 */
#define IOCTL_CLEAN_KERNEL_DMA(params, err_goto_label)                         \
        if (NULL != params.ppDma[0])                                           \
                kfree(params.ppDma[0]);

/**
 * Nothing needs to be copied, as the kernel pages array is only used internally
 * by the kernel module and not by the userspace.
 * Just keep the address of array `ppDma[0]->kernel_pages` for unlock to free
 * it.
 *
 * @param err_goto_label: goto label in case any error is encountered,
 * setting `ret` to error code.
 */
#define IOCTL_COPY_KERNEL_PAGES_FROM_USER(err_goto_label)
/**
 * Nothing needs to be copied, as the kernel pages array is only used internally
 * by the kernel module and not by the userspace.
 * Just keep the address of array `ppDma[0]->kernel_pages` for unlock to free
 * it.
 *
 * @param err_goto_label: goto label in case any error is encountered,
 * setting `ret` to error code.
 */
#define IOCTL_COPY_KERNEL_PAGES_TO_USER(err_goto_label)
/**
 * Nothing needs to be cleaned, as `IOCTL_COPY_KERNEL_PAGES_FROM_USER`
 * does nothing.
 *
 * @param err_goto_label: goto label in case any error is encountered,
 * setting `ret` to error code.
 */
#define IOCTL_CLEAN_KERNEL_PAGES(err_goto_label)

/**
 * Macro is mainly used by internal functions called by `crono_miscdev_ioctl`.
 * `IOCTL_VALIDATE_LOCK_PARAMS` must be called prior to calling this
 * macro.
 * Allocate, and validate `ppage`, which must be freed using `kfree` when done
 * working with it.
 * `params.ppDma[0]->Page` array should be allocated in userspace by caller, and
 * is of number of elements = `params.pDma[0].dwPages`.
 * `IOCTL_COPY_CRONO_PAGES_TO_USER` must be called when done working with it.
 *
 * Prerequisites:
 *  - `pages_size` of type `int` is defined.
 *  - `ppage` of type `CRONO_KERNEL_DMA_PAGE*` is defined.
 *  - `IOCTL_VALIDATE_LOCK_PARAMS` is called.
 *
 * @param params[in/out]: a valid `DMASGBufLock_parameters` struct.
 * @param ppage[out]: is `CRONO_KERNEL_DMA_PAGE *`.
 * @param pages_size[out]: is `size_t `. Size of 'ppage' memory in bytes.
 * @param ret[out]: of type `int`, will be set to the error number in case of
 * error.
 * @param err_goto_label: goto label in case any error is encountered,
 * setting `ret` to error code.
 */
#define IOCTL_COPY_CRONO_PAGES_FROM_USER(params, ppage, pages_size, ret,       \
                                         err_goto_label)                       \
        pages_size = sizeof(CRONO_KERNEL_DMA_PAGE) * params.ppDma[0]->dwPages; \
        pr_debug("Allocating kernel pages structure of size <%ld>",            \
                 pages_size);                                                  \
        ppage = (CRONO_KERNEL_DMA_PAGE *)kvmalloc(pages_size, GFP_KERNEL);     \
        if (NULL == ppage) {                                                   \
                pr_err("Error allocating memory");                             \
                ret = -ENOMEM;                                                 \
                goto err_goto_label;                                           \
        }                                                                      \
        pr_debug("Copying kernel pages structure from address <0x%lx>",        \
                 params.ulPage);                                               \
        if (copy_from_user(ppage, (void __user *)(params.ulPage),              \
                           pages_size)) {                                      \
                IOCTL_CLEAN_CRONO_PAGES(ppage, err_goto_label);                \
                ret = -EFAULT;                                                 \
                goto err_goto_label;                                           \
        }                                                                      \
        params.ppDma[0]->Page = ppage;
/**
 * Macro is mainly used by internal functions called by `crono_miscdev_ioctl`.
 * 'IOCTL_COPY_CRONO_PAGES_FROM_USER' must be called prior to calling this
 * macro. Copy back the `Page` array elements from kernel module to the
 * userspace.
 *
 * Prerequisites:
 *
 * @param err_goto_label: goto label in case any error is encountered,
 * setting `ret` to error code.
 * @param ret[out]: of type `int`, will be set to the error number in case of
 * error.
 */
#define IOCTL_COPY_CRONO_PAGES_TO_USER(arg, params, ret, err_goto_label)       \
        if (copy_to_user((void __user *)(params.ulPage),                       \
                         params.ppDma[0]->Page, pages_size)) {                 \
                IOCTL_CLEAN_CRONO_PAGES(ppage, err_goto_labelr);               \
                ret = -EFAULT;                                                 \
                goto err_goto_label;                                           \
        }
/**
 * Macro is mainly used by internal functions called by `crono_miscdev_ioctl`.
 * 'IOCTL_COPY_CRONO_PAGES_FROM_USER' must be called prior to calling this
 * macro. Clean up all work done by `IOCTL_COPY_CRONO_PAGES_FROM_USER`.
 *
 * Prerequisites:
 *  - `ret` of type `int` is defined.
 *
 * @param err_goto_label: goto label in case any error is encountered,
 * setting `ret` to error code.
 */
#define IOCTL_CLEAN_CRONO_PAGES(ppage, err_goto_label)                         \
        {                                                                      \
                kvfree(ppage);                                                 \
                ppage = NULL;                                                  \
        }

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
        {                                                                      \
                kvfree(mem);                                                   \
                mem = NULL;                                                    \
        }

// _____________________________________________________________________________
#endif // #define __CRONO_MISCDEVICE_H__