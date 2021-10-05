#include "crono_kernel_module.h"
#include "crono_miscdevice.h"
// _____________________________________________________________________________
// Globals
//
MODULE_DESCRIPTION("cronologic PCI driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.9");

#ifndef CRONO_KERNEL_MODE
#pragma message("CRONO_KERNEL_MODE must be defined in the kernel module")
#endif

// `CRONO_KERNEL_MODE` must be defined indicating the code runs for driver.
// #define CRONO_KERNEL_MODE, no need to define it here as it's passed in
// Makefile

/**
 * The device type entry in the array, has the index = Device ID. e.g. device
 * CRONO_DEVICE_XTDC4 has entry at index 6 in the array.
 * Size has an additional `1`, as Device Type ID starts at `1`
 * (Device Type of ID=0 is undefined).
 */
static struct crono_device_type
    crono_device_types_info[CRONO_DEVICE_DEV_ID_MAX_COUNT + 1];

static const struct pci_device_id crono_pci_device_ids[] = {
    // Get all devices of cronologic Vendor ID
    {
        .vendor = CRONO_VENDOR_ID,
        .device = PCI_ANY_ID,
        .subvendor = PCI_ANY_ID,
        .subdevice = PCI_ANY_ID,
    },
    {/* end: all zeroes */}};
MODULE_DEVICE_TABLE(pci, crono_pci_device_ids);

static struct pci_driver crono_pci_driver = {
    .name = CRONO_PCI_DRIVER_NAME,
    .id_table = crono_pci_device_ids,
    .probe = crono_driver_probe,
};

// Miscellaneous Device Driver
static int Device_Open = 0; // Is device open?  Used to prevent multiple

static struct file_operations crono_miscdev_fops = {
    .owner = THIS_MODULE,

    .open = crono_miscdev_open,
    .release = crono_miscdev_release,

    .unlocked_ioctl = crono_miscdev_ioctl,
};

// _____________________________________________________________________________
// init & exit
//
static int __init crono_driver_init(void) {

        int ret = CRONO_SUCCESS;

        pr_info("Registering Driver...");

        // Reset memory
        memset(crono_device_types_info, 0,
               sizeof(struct crono_device_type) *
                   CRONO_DEVICE_DEV_ID_MAX_COUNT);

        // Register the driver, and start probing
        ret = pci_register_driver(&crono_pci_driver);
        if (ret) {
                pr_err("Error Registering PCI Driver, <%d>!!!", ret);
                return ret;
        }

        // Success
        pr_info("Done");

        return ret;
}
module_init(crono_driver_init);

/*
** Module exit function
*/
static void __exit crono_driver_exit(void) {

        int dt_index = -1;
        struct crono_device_type *dev_type = NULL;

        // Unregister all miscdevs
        // Loop on the device types have registered devices
        for_each_active_device_types(dev_type) {
                _crono_miscdev_type_exit(dev_type);
        }
        for_each_active_device_types_end;

        // Unregister the driver
        pr_info("Removing Driver...");
        pci_unregister_driver(&crono_pci_driver);
        pr_info(
            "Done"); // Code line is unreachable after unregistering the driver
}
module_exit(crono_driver_exit);

// Probe
static int crono_driver_probe(struct pci_dev *dev,
                              const struct pci_device_id *id) {

        int ret = CRONO_SUCCESS;

        // Check the device to claim if concerned
        pr_info("Probe Device, ID = <0x%02X>", dev->device);
        if (id->vendor != CRONO_VENDOR_ID)
                return -EINVAL;
        if ((dev->device > CRONO_DEVICE_DEV_ID_MAX_COUNT) ||
            (dev->device < 0)) {
                pr_err("Error Device ID <0x%02x> not supported", dev->device);
                return -EINVAL;
        }

        // Check it's either PCIe or thunderbolt
        if (!pci_is_pcie(dev) && !pci_is_thunderbolt_attached(dev)) {
                pr_err(
                    "Error Device ID <0x%02x> is neither PCIe nor Thunderbolt",
                    dev->device);
                return -EINVAL;
        }

        // Enable the PCIe device
        ret = pci_enable_device(dev);
        if (ret < 0) {
                pr_err("Error enabling the device in probe");
                return ret;
        }

        // Enable DMA by setting the bus master bit in the PCI_COMMAND register
        pci_set_master(dev);

        // Register a miscdev for this device
        // Device ID (e.g. 0x06 of xTDC8) is the device type index in the device
        // types array.
        crono_device_types_info[dev->device].device_id = dev->device;
        ret = _crono_miscdev_type_init(&crono_device_types_info[dev->device],
                                       dev);
        if (ret) {
                goto error_miscdev;
        }

        // ___________________
        // Set DMA Mask
        // Since crono devices can all handle full 64 bit address as DMA source
        // and destination, we need to set 64-bit mask to avoid using `swiotlb`
        // by linux when calling `dma_map_sg`.
        //
        ret = pci_set_dma_mask(dev, DMA_BIT_MASK(64));
        if (ret != CRONO_SUCCESS) {
                pr_err("Device cannot perform DMA properly on this "
                       "platform, error <%d>",
                       ret);
                return ret;
        }

        return ret;

error_miscdev:
        pci_disable_device(dev);
        return ret;
}

// _____________________________________________________________________________
// Miscellaneous Device Driver
static int _crono_miscdev_type_init(struct crono_device_type *dev_type,
                                    struct pci_dev *dev) {

        char miscdev_name[CRONO_MAX_DEV_NAME_SIZE];
        int register_ret;
        struct miscdevice *new_crono_miscdev_dev;

        // Validations
        if (dev_type == NULL) {
                pr_err("_crono_miscdev_type_init Invalid Arguments");
                return -EINVAL;
        }
        if (dev_type->bound_drivers_count == CRONO_KERNEL_PCI_CARDS) {
                pr_err("miscdev reached maximum count for type of id <%d>",
                       dev_type->device_id);
                return -EBUSY;
        }

        // Generate the device name
        // `dev_type->bound_drivers_count` is the index of the new element
        new_crono_miscdev_dev =
            &(dev_type->cdevs[dev_type->bound_drivers_count].miscdev);
        new_crono_miscdev_dev->minor = MISC_DYNAMIC_MINOR;
        new_crono_miscdev_dev->fops = &crono_miscdev_fops;
        new_crono_miscdev_dev->name = miscdev_name;

        _crono_get_DBDF_from_dev(
            dev, &(dev_type->cdevs[dev_type->bound_drivers_count].dbdf));
        _crono_pr_info_DBDF(
            &(dev_type->cdevs[dev_type->bound_drivers_count].dbdf));

        CRONO_CONSTRUCT_MISCDEV_NAME(
            miscdev_name, dev_type->device_id,
            dev_type->cdevs[dev_type->bound_drivers_count].dbdf);

        pr_info("Initializing cronologic miscdev driver: <%s>...",
                miscdev_name);

        // Register the device driver
        register_ret = misc_register(new_crono_miscdev_dev);
        if (register_ret) {
                pr_err("Can't register misdev: <%s>, error: <%d>", miscdev_name,
                       register_ret);
                return register_ret;
        }

        // Set the driver internal information
        strcpy(dev_type->cdevs[dev_type->bound_drivers_count].name,
               miscdev_name);
        dev_type->cdevs[dev_type->bound_drivers_count].miscdev.name =
            dev_type->cdevs[dev_type->bound_drivers_count].name;
        dev_type->cdevs[dev_type->bound_drivers_count].dev = dev;

        dev_type->bound_drivers_count++;

        // Log and return
        pr_info("Done with minor: <%d>", new_crono_miscdev_dev->minor);
        return CRONO_SUCCESS;
}

static void _crono_miscdev_type_exit(struct crono_device_type *dev_type) {
        int device_index;
        // Loop on the registered devices of the this device type.
        for (device_index = 0; device_index < dev_type->bound_drivers_count;
             device_index++) {
                pr_info(
                    "Exiting cronologic miscdev driver: <%s>, minor: <%d>...",
                    dev_type->cdevs[device_index].miscdev.name,
                    dev_type->cdevs[device_index].miscdev.minor);
                misc_deregister(&(dev_type->cdevs[device_index].miscdev));
                pr_info("Done"); // Needed to log it didn't crash
        }
}

static long crono_miscdev_ioctl(struct file *filp, unsigned int cmd,
                                unsigned long arg) {
        int ret = CRONO_SUCCESS;

        switch (cmd) {
        case IOCTL_CRONO_LOCK_BUFFER:
                ret = _crono_miscdev_ioctl_lock_buffer(filp, arg);
                break;
        case IOCTL_CRONO_UNLOCK_BUFFER:
                ret = _crono_miscdev_ioctl_unlock_buffer(filp, arg);
                break;
        case IOCTL_CRONO_CLEANUP_SETUP:
                ret = _crono_miscdev_ioctl_cleanup_setup(filp, arg);
                break;
        default:
                pr_err("Error, unsupported ioctl command <%d>", cmd);
                ret = -ENOTTY;
                break;
        }
        return ret;
}

static int _crono_miscdev_ioctl_lock_buffer(struct file *filp,
                                            unsigned long arg) {
        int ret;
        DMASGBufLock_parameters params, arg_orig;
        CRONO_KERNEL_DMA *pDma = NULL; // Temp for internal handling
        CRONO_KERNEL_DMA arg_dma_orig; // Temp for internal handling
        CRONO_KERNEL_DMA_PAGE *Page_orig =
            NULL; // Keep it till after the `copy_to_user`
        CRONO_KERNEL_DMA_PAGE *ppage = NULL;
        size_t pages_size = 0;
#ifdef CRONO_DEBUG_ENABLED
        int ipage, loop_count;
#endif
        pr_debug("Entered _crono_miscdev_ioctl_lock_buffer");

        // ________________________________________
        // Lock and copy data from userspace memory
        //
        IOCTL_VALIDATE_LOCK_PARAMS(arg, params, arg_orig, ret, lock_err);
        IOCTL_COPY_KERNEL_DMA_FROM_USER(params, pDma, arg_dma_orig, lock_err);
        IOCTL_COPY_KERNEL_PAGES_FROM_USER(lock_err);
        IOCTL_COPY_CRONO_PAGES_FROM_USER(params, ppage, pages_size, ret,
                                         lock_err);

        pr_info("Locking buffer of size = <%ld>, address = <%p>",
                params.dwDMABufSize, params.pBuf);
        pr_debug("Allocated `ppage` of size <%ld>\n", pages_size);

        // ___________________
        // Validate parameters
        //
        LOGERR_RET_EINVAL_IF_NULL(params.pBuf, "Invalid buffer to be locked");
        if (params.dwDMABufSize > ULONG_MAX) {
                pr_err("Size <%ld> of requested buffer to lock exceeds "
                       "the maximum size allowed",
                       params.dwDMABufSize);

                IOCTL_CLEAN_KERNEL_DMA(params, lock_err);
                IOCTL_CLEAN_KERNEL_PAGES(lock_err);

                return -EINVAL;
        }

        // Pin the buffer, allocate and fill `params.kernel_pages`.
        ret = _crono_miscdev_ioctl_pin_buffer(filp, &params, GUP_NR_PER_CALL);
        if (CRONO_SUCCESS != ret) {
                IOCTL_CLEAN_CRONO_PAGES(ppage, lock_err);
                IOCTL_CLEAN_KERNEL_PAGES(lock_err);
                IOCTL_CLEAN_KERNEL_DMA(params, lock_err);
                return -EFAULT;
        }

        // Fill the Scatter/Gather list
        ret = _crono_miscdev_ioctl_generate_sg(filp, &params);
        if (CRONO_SUCCESS != ret) {
                IOCTL_CLEAN_CRONO_PAGES(ppage, lock_err);
                IOCTL_CLEAN_KERNEL_PAGES(lock_err);
                IOCTL_CLEAN_KERNEL_DMA(params, lock_err);
                return -EFAULT;
        }

        // Copy back all data to userspace memory
        IOCTL_DONE_WITH_LOCK_PARAMS(arg, arg_orig, lock_err);
        IOCTL_COPY_KERNEL_DMA_TO_USER(arg, params, arg_dma_orig, Page_orig, ret,
                                      lock_err);
        IOCTL_COPY_KERNEL_PAGES_TO_USER(lock_err);
        IOCTL_COPY_CRONO_PAGES_TO_USER(arg, params, ret, lock_err);

#ifdef CRONO_DEBUG_ENABLED
        // Log only first 5 pages
        // `IOCTL_DONE_WITH_LOCK_PARAMS` should have copied the values of
        // `pPhysicalAddr` in `arg`.
        loop_count =
            params.ppDma[0]->dwPages < 5 ? params.ppDma[0]->dwPages : 5;
        for (ipage = 0; ipage < loop_count; ipage++) {
                pr_debug("ioctl_lock: Userpsace Buffer Page <%d> "
                         "Physical Address is <%p>\n",
                         ipage,
                         (void *)(params.ppDma[0]->Page[ipage].pPhysicalAddr));
        }
#endif

        return CRONO_SUCCESS;

lock_err:
        return ret;
}

static int _crono_miscdev_ioctl_pin_buffer(struct file *filp,
                                           DMASGBufLock_parameters *params,
                                           unsigned long nr_per_call) {

        unsigned long start_addr_to_pin; // Start address in pBuf to be pinned
                                         // by `pin_user_pages`.
#ifdef KERNL_SUPPORTS_PIN_UG
        unsigned long next_pages_addr; // Next address in pBuf to be pinned by
                                       // `pin_user_pages`.
#endif
        int buffer_pages_nr; // `params` buffer number of pages, calculated
                             // based on its size.
        long actual_pinned_nr_of_call; // Never unsigned, as it might contain
                                       // error returned
        int ret = CRONO_SUCCESS;
        int page_index;

        pr_debug("Entered _crono_miscdev_ioctl_pin_buffer");
        // Validate parameters
        LOGERR_RET_EINVAL_IF_NULL(params, "Invalid lock buffer parameters");
        LOGERR_RET_EINVAL_IF_NULL(params->ppDma[0],
                                  "Invalid dma struct parameters");

        // Allocate struct pages pointer array memory to be filled by
        // `pin_user_pages` params->kernel_pages contains virtual address,
        // however, you may DMA to/from that memory using the addresses returned
        // from it
        buffer_pages_nr = DIV_ROUND_UP(params->dwDMABufSize, PAGE_SIZE);
        pr_info(
            "Allocating kernel pages. Buffer size = <%ld>, pages number = <%d>",
            params->dwDMABufSize, buffer_pages_nr);

        // `kvmalloc_array` may return memory that is not physically contiguous.
        params->ppDma[0]->kernel_pages =
            kvmalloc_array(buffer_pages_nr, sizeof(struct page *), GFP_KERNEL);
        LOGERR_RET_ERRNO_IF_NULL(params->ppDma[0]->kernel_pages,
                                 "Error allocating pages memory", -ENOMEM);
        pr_debug("Allocated `kernel_pages` of size <%ld>\n",
                 buffer_pages_nr * sizeof(void *));

        start_addr_to_pin = (__u64)params->pBuf;
#ifdef KERNL_SUPPORTS_PIN_UG
        // https://elixir.bootlin.com/linux/v5.6/source/include/linux/mm.h#L1508
        // Pin buffer blocks, each of size = (nr_per_call * PAGE_SIZE) in every
        // iteration to its corresponding page address in
        // params->ppDma[0]->kernel_pages
        for (params->ppDma[0]->pinned_pages_nr = 0;
             start_addr_to_pin < (__u64)params->pBuf + params->dwDMABufSize;
             start_addr_to_pin = next_pages_addr) {
                // Next pages chunk starts after `nr_per_call` pages bytes
                next_pages_addr = start_addr_to_pin + nr_per_call * PAGE_SIZE;
                if (next_pages_addr >
                    ((__u64)params->pBuf + params->dwDMABufSize))
                // Exceeded the buffer size
                {
                        next_pages_addr =
                            (__u64)params->pBuf + params->dwDMABufSize;
                        nr_per_call =
                            (next_pages_addr - start_addr_to_pin) / PAGE_SIZE;
                }
                actual_pinned_nr_of_call = pin_user_pages(
                    start_addr_to_pin, nr_per_call, FOLL_WRITE,
                    (struct page **)(params->ppDma[0]->kernel_pages) +
                        params->ppDma[0]->pinned_pages_nr,
                    NULL);

                if (actual_pinned_nr_of_call < 0) {
                        // Error
                        pr_err("Error pinning user pages: <%ld>",
                               actual_pinned_nr_of_call);
                        ret = actual_pinned_nr_of_call;
                        break;
                }
                if (actual_pinned_nr_of_call == 0) {
                        // Success, end of mapping
                        pr_debug("ioctl_pin: Successful end of pinning");
                        break;
                }

                pr_debug(
                    "Pin Iteration: Count <%ld>, 1st kernel_pages <%p>, last "
                    "kernel_page <%p>",
                    actual_pinned_nr_of_call,
                    (params->ppDma[0]->kernel_pages +
                     params->ppDma[0]->pinned_pages_nr)[0],
                    (params->ppDma[0]->kernel_pages +
                     params->ppDma[0]->pinned_pages_nr)[nr_per_call - 1]);

                params->ppDma[0]->pinned_pages_nr += actual_pinned_nr_of_call;
        }
#else
#pragma message("Kernel version is older than 5.6")
        // https://elixir.bootlin.com/linux/v4.9/source/include/linux/mm.h#L1278
        down_read(&current->mm->mmap_sem);
        pr_debug(
            "Calling get_user_pages: address <0x%lx>, number of pages: <%d>",
            start_addr_to_pin, buffer_pages_nr);
        actual_pinned_nr_of_call = get_user_pages(
            start_addr_to_pin, buffer_pages_nr, (FOLL_WRITE | FOLL_FORCE),
            (struct page **)(params->ppDma[0]->kernel_pages), NULL);
        if (actual_pinned_nr_of_call >= 0) {
                pr_debug(
                    "get_user_pages is called successfully with return <%ld>",
                    actual_pinned_nr_of_call);
                params->ppDma[0]->pinned_pages_nr = actual_pinned_nr_of_call;
        } else {
                pr_err("get_user_pages is called with error return <%ld>",
                       actual_pinned_nr_of_call);
                up_read(&current->mm->mmap_sem);
                _crono_miscdev_ioctl_cleanup_buffer(filp, params);
                return -EFAULT;
        }
        up_read(&current->mm->mmap_sem);
#endif

        // Validate the number of pinned pages
        if (params->ppDma[0]->pinned_pages_nr < buffer_pages_nr) {
                // Apparently not enough memory to pin the whole buffer
                pr_err("Error insufficient available pages to pin");
                _crono_miscdev_ioctl_cleanup_buffer(filp, params);
                if (CRONO_SUCCESS == ret)
                        return -EFAULT;
                else
                        return ret;
        }

        // Fill in the ppDma Page Array
        for (page_index = 0; page_index < buffer_pages_nr; page_index++) {
                params->ppDma[0]->Page[page_index].pPhysicalAddr =
                    PFN_PHYS(page_to_pfn((struct page *)params->ppDma[0]
                                             ->kernel_pages[page_index]));
                params->ppDma[0]->Page[page_index].dwBytes = PAGE_SIZE;

                if (page_index < 5)
                        // Log only first 5 pages
                        pr_debug("ioctl_pin: Kernel Page <%d> is of "
                                 "physical address <%p>",
                                 page_index,
                                 (void *)params->ppDma[0]
                                     ->Page[page_index]
                                     .pPhysicalAddr);
        }

        // Set the actual buffer size, and actual count of pages pinned
        params->ppDma[0]->pUserAddr = params->pBuf;
        params->ppDma[0]->dwPages =
            params->ppDma[0]
                ->pinned_pages_nr; // Do we really need them to be the same?
#ifdef KERNL_SUPPORTS_PIN_UG
        // Calculate actual size in case pinned memory is larger than buffer
        // size
        params->ppDma[0]->dwBytes = start_addr_to_pin - (__u64)params->pBuf;
#else
        params->ppDma[0]->dwBytes = params->dwDMABufSize;
#endif

        pr_info(
            "Successfully Pinned buffer: size = <%d>, number of pages = <%d>",
            params->ppDma[0]->dwBytes, params->ppDma[0]->dwPages);

        return ret;
}

static int _crono_miscdev_ioctl_unlock_buffer(struct file *filp,
                                              unsigned long arg) {
        int ret = CRONO_SUCCESS;
        DMASGBufLock_parameters params, arg_orig;
        CRONO_KERNEL_DMA *pDma = NULL; // Temp for internal handling
        CRONO_KERNEL_DMA arg_dma_orig;
        CRONO_KERNEL_DMA_PAGE *Page_orig =
            NULL; // Keep it till after the `copy_to_user`

        IOCTL_VALIDATE_LOCK_PARAMS(arg, params, arg_orig, ret, unlock_err);
        IOCTL_COPY_KERNEL_PAGES_FROM_USER(unlock_err);
        IOCTL_COPY_KERNEL_DMA_FROM_USER(params, pDma, arg_dma_orig, unlock_err);

        _crono_miscdev_ioctl_cleanup_buffer(filp, &params);

        IOCTL_COPY_KERNEL_DMA_TO_USER(arg, params, arg_dma_orig, Page_orig, ret,
                                      unlock_err);
        IOCTL_COPY_KERNEL_PAGES_TO_USER(unlock_err);
        IOCTL_DONE_WITH_LOCK_PARAMS(arg, arg_orig, unlock_err);

        return ret;

unlock_err:
        return ret;
}

static int _crono_miscdev_ioctl_cleanup_setup(struct file *filp,
                                              unsigned long arg) {
        int ret = CRONO_SUCCESS;
        struct pci_dev *devp;

        ret = _crono_get_dev_from_filp(filp, &devp);
        if (ret != CRONO_SUCCESS) {
                if (-ENODATA == ret) {
                        pr_err("Error getting device information from "
                               "file descriptor");
                }
                return ret;
        }

        // Disable DMA by clearing the bus master bit. Paired with
        // pci_set_master()
        pci_clear_master(devp);

        // PCI device is not in use by the system anymore. This only involves
        // disabling PCI bus-mastering
        // Note we don’t actually disable the device until all callers of
        // pci_enable_device() have called pci_disable_device().
        pci_disable_device(devp);

        return ret;
}

static int _crono_miscdev_ioctl_generate_sg(struct file *filp,
                                            DMASGBufLock_parameters *params) {
        struct pci_dev *devp;
        int ret;
        struct sg_table *sgt = NULL;
        int mapped_buffers_count = 0;
#ifdef USE__sg_alloc_table_from_pages
        int page_index;
        struct scatterlist *sg;
        struct scatterlist *sg_from_pages = NULL;
        DWORD dw_mapped_pages_size = 0;
#endif

        // Validate parameters
        LOGERR_RET_EINVAL_IF_NULL(params->ppDma[0]->kernel_pages,
                                  "Invalid pages to get addresses for");
        if (params->ppDma[0]->sgt != NULL) {
                pr_err("Invalid DMA SG address, already allocated");
                return -EFAULT;
        }

        // Allocate sgt, as `allocate_sg_table` does not allocate it.
        sgt = params->ppDma[0]->sgt =
            (struct sg_table *)kvzalloc(sizeof(struct sg_table), GFP_KERNEL);
        if (NULL == sgt) {
                pr_err("Error allocating memory");
                return -ENOMEM;
        }

        // Get the corresponding device structure to be used in `dma_map_sg`
        ret = _crono_get_dev_from_filp(filp, &devp);
        if (ret != CRONO_SUCCESS) {
                if (-ENODATA == ret) {
                        pr_err("Error getting device information from "
                               "file descriptor");
                }
                // Clean up
                crono_kvfree(sgt);

                // Other errors occurred, probably invalid parameters,
                // `_crono_get_dev_from_filp` should have displayed the
                // corresponding error message.
                return ret;
        }

        // Allocate sg_table to hold the scatter/gather segments array.
        // With scatter lists, we map a region gathered from several regions
        // If passed_pages_count > SG_MAX_SINGLE_ALLOC (much smaller than 4096),
        // then a chained sg table will be setup
        pr_info("Allocating SG Table for buffer size = <%d>, number of "
                "pages = <%d> ...",
                params->ppDma[0]->dwBytes, params->ppDma[0]->dwPages);
#ifndef USE__sg_alloc_table_from_pages
        ret = sg_alloc_table_from_pages(
            sgt, // The sg table header to use
            (struct page **)params->ppDma[0]
                ->kernel_pages,        // Pointer to an array of page pointers
            params->ppDma[0]->dwPages, // Number of pages in the pages array
            0, // Offset from start of the first page to the start of a buffer
            params->ppDma[0]
                ->dwBytes, // Number of valid bytes in the buffer (after offset)
            GFP_KERNEL);
        if (ret)
#else
        // If the physical address got by `PFN_PHYS`, didn't work, then,
        // `__sg_alloc_table_from_pages` should be called with `PAGE_SIZE`
        // parameter, then `sg_dma_address` should be used to fill in the pages
        // addresses, however, it's not guaranteed that the buffers coming out
        // of `dma_map_sg` are 1-PAGE-ONLY SIZE.
        sg_from_pages = __sg_alloc_table_from_pages(
            sgt, // The sg table header to use
            (struct page **)params->ppDma[0]
                ->kernel_pages,        // Pointer to an array of page pointers
            params->ppDma[0]->dwPages, // Number of pages in the pages array
            0, // Offset from start of the first page to the start of a buffer
            params->ppDma[0]
                ->dwBytes, // Number of valid bytes in the buffer (after offset)
            PAGE_SIZE, NULL, 0, GFP_KERNEL);
        if (PTR_ERR_OR_ZERO(sg_from_pages))
#endif
        {
                pr_err("Error allocating SG table from pages");

                // Clean up
                crono_kvfree(sgt);

                return ret;
        }
        pr_info("Done");

#ifdef USE__sg_alloc_table_from_pages
        // Using `__sg_alloc_table_from_pages` results in `nents` number equal
        // to Pages Number; while using `sg_alloc_table_from_pages` will most
        // probably result in a different number of `nents`.
        if (sgt->nents != params->ppDma[0]->dwPages) {
                pr_err("Inconsistent SG table elements number with "
                       "number of pages");
                return -EFAULT;
        }
#endif

        pr_info("Mapping SG...");
        mapped_buffers_count =
            dma_map_sg(&devp->dev, sgt->sgl, sgt->nents, DMA_BIDIRECTIONAL);
        // `ret` is the number of DMA buffers to transfer. `dma_map_sg`
        // coalesces buffers that are adjacent to each other in memory, so `ret`
        // may be less than nents.
        if (mapped_buffers_count < 0) {
                pr_err("Error mapping SG: <%d>", ret);

                // Clean up
                sg_free_table(sgt); // Free sgl, even if it's chained
                crono_kvfree(sgt);

                return ret;
        }
        pr_info("Done");

        pr_debug(
            "SG Table is allocated of scatter lists total nents number = <%d>"
            ", Mapped buffers count <%d>",
            sgt->nents, mapped_buffers_count);

        // Success
        return CRONO_SUCCESS;
}

static int
_crono_miscdev_ioctl_cleanup_buffer(struct file *filp,
                                    DMASGBufLock_parameters *params) {
        struct pci_dev *devp;
        int ret;
#ifndef KERNL_SUPPORTS_PIN_UG
        int ipage;
#endif

        if (NULL == params || NULL == params->ppDma[0] ||
            NULL == params->ppDma[0]->kernel_pages) {

                pr_debug("Nothing to clean for the buffer");
                return CRONO_SUCCESS;
        }

#ifdef KERNL_SUPPORTS_PIN_UG
        // Unpin pages
        pr_debug("Unpinning pages of address = <%p>, and number = <%d>",
                 params->ppDma[0]->kernel_pages,
                 params->ppDma[0]->pinned_pages_nr);
        unpin_user_pages((struct page **)(params->ppDma[0]->kernel_pages),
                         params->ppDma[0]->pinned_pages_nr);
#else
        pr_debug("Putting pages of address = <%p>, and number = <%d>",
                 params->ppDma[0]->kernel_pages,
                 params->ppDma[0]->pinned_pages_nr);
        for (ipage = 0; ipage < params->ppDma[0]->pinned_pages_nr; ipage++) {
                put_page(params->ppDma[0]->kernel_pages[ipage]);
        }
#endif
        ret = _crono_get_dev_from_filp(filp, &devp);
        if (ret != CRONO_SUCCESS) {
                pr_err("Error getting device information from "
                       "file descriptor: <%d>",
                       ret);
                return ret;
        }

        // Unmap Scatter/Gather list
        dma_unmap_sg(&devp->dev,
                     ((struct sg_table *)params->ppDma[0]->sgt)->sgl,
                     ((struct sg_table *)params->ppDma[0]->sgt)->nents,
                     DMA_BIDIRECTIONAL);

        // Clean allocated memory for Scatter/Gather list
        if (NULL != params->ppDma[0]->sgt) {
                sg_free_table(params->ppDma[0]->sgt);
                crono_kvfree(params->ppDma[0]->sgt);
        }

        // Clean allocated memory for kernel pages
        crono_kvfree(params->ppDma[0]->kernel_pages);

        // Success
        return CRONO_SUCCESS;
}

// _____________________________________________________________________________
// Methods
//
/* Called when a process tries to open the device file, like
 * "cat /dev/mycharfile"
 */
static int crono_miscdev_open(struct inode *inode, struct file *file) {

        static int counter = 0;

        pr_debug("Opening device file");
        if (Device_Open) {
                pr_debug("Device is busy, opened <%d> times", Device_Open);
        }

        Device_Open++;
        pr_info("Driver open() is called <%d> times", counter++);

        return CRONO_SUCCESS;
}

/* Called when a process closes the device file */
static int crono_miscdev_release(struct inode *inode, struct file *file) {

        Device_Open--; /* We're now ready for our next caller */

        // Decrement the usage count, or else once you opened the file, you'll
        //  never get rid of the module.

        pr_info("Driver release() is called");

        return CRONO_SUCCESS;
}

// _____________________________________________________________________________

static int _crono_get_DBDF_from_dev(struct pci_dev *dev,
                                    struct crono_dev_DBDF *dbdf) {
        if (NULL == dev)
                return -EINVAL;
        if (NULL == dbdf)
                return -EINVAL;

        // Reset all to zeros
        memset(dbdf, 0, sizeof(struct crono_dev_DBDF));

        if (dev->bus != NULL) {
                dbdf->bus = dev->bus->number;
                if (dev->bus->parent != NULL) {
                        dbdf->domain = dev->bus->parent->number;
                }
        }
        dbdf->dev = PCI_SLOT(dev->devfn);
        dbdf->func = PCI_FUNC(dev->devfn);

        // Success
        return CRONO_SUCCESS;
}

static int _crono_pr_info_DBDF(struct crono_dev_DBDF *dbdf) {

        if (NULL == dbdf)
                return -EINVAL;

        pr_info("Device BDBF: <%04X:%02X:%02X.%01X>", dbdf->domain, dbdf->bus,
                dbdf->dev, dbdf->func);

        return CRONO_SUCCESS;
}

static int _crono_get_dev_from_filp(struct file *filp, struct pci_dev **devpp) {

        unsigned int file_drv_minor;
        int dt_index = -1;
        struct crono_device_type *dev_type = NULL;
        int dev_index = -1;

        // Validate parameters
        LOGERR_RET_EINVAL_IF_NULL(filp, "Invalid file to get dev for");
        LOGERR_RET_EINVAL_IF_NULL(devpp, "Invalid device pointer");

        // Get the file descriptor minor to match it with the device
        file_drv_minor = iminor(filp->f_path.dentry->d_inode);

        // Loop on the registered devices of every device type
        for_each_dev_of_active_device_types(dev_type, dev_index) {
                if (dev_type->cdevs[dev_index].miscdev.minor != file_drv_minor)
                        continue;
                // Found a device with the same minor of the file
                *devpp = dev_type->cdevs[dev_index].dev;
                return CRONO_SUCCESS;
        }
        for_each_dev_of_active_device_types_end;

        // Corresponding device is not found
        return -ENODATA;
}
