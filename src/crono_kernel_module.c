#include "crono_kernel_module.h"
#include "crono_miscdevice.h"

// _____________________________________________________________________________
// Globals
//
MODULE_DESCRIPTION("cronologic PCI driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");

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

// DMA Buffer Information Wrappers List Variables and Functions
/**
 * @brief Head of list of all locked Buffer Wrappers
 */
struct list_head buff_wrappers_head;

/**
 * @brief Count of Buffer Wrappers in 'buff_wrappers_head' list
 */
static int buff_wrappers_new_id = 0;

static int _crono_init_buff_wrappers_list(void) {
        INIT_LIST_HEAD(&buff_wrappers_head);
        return CRONO_SUCCESS;
}
// _____________________________________________________________________________
// init & exit
//
static int __init crono_driver_init(void) {

        int ret = CRONO_SUCCESS;

        pr_info("Registering PCI Driver...");

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

        // Initialize the buffer information wrappers list object
        if (CRONO_SUCCESS != (ret = _crono_init_buff_wrappers_list())) {
                return ret;
        }

        // Success
        pr_info("Done registering cronologic PCI driver");

        return ret;
}
module_init(crono_driver_init);

/*
** Module exit function
*/
static void __exit crono_driver_exit(void) {

        int dt_index = -1;
        struct crono_device_type *dev_type = NULL;
        struct list_head *pos, *n;
        CRONO_BUFFER_INFO_WRAPPER *temp_buff_wrapper;

        // Unregister all miscdevs
        // Loop on the device types have registered devices
        for_each_active_device_types(dev_type) {
                _crono_miscdev_type_exit(dev_type);
        }
        for_each_active_device_types_end;

        // Clean up all buffer information wrappers and list
        pr_info("Cleanup wrappers list...");
        list_for_each_safe(pos, n, &buff_wrappers_head) {
                _crono_release_buff_wrapper(temp_buff_wrapper);
                temp_buff_wrapper =
                    list_entry(pos, CRONO_BUFFER_INFO_WRAPPER, list);
                pr_debug("Found unlocked buffer wrapper: id<%d>",
                         temp_buff_wrapper->buff_info.id);
                list_del(&temp_buff_wrapper->list);
        }
        _crono_debug_list_wrappers();

        // Unregister the driver
        pr_info("Removing Driver...");
        pci_unregister_driver(&crono_pci_driver);
        pr_info(
            "Done removing cronologic PCI driver"); // Code line is unreachable
                                                    // after unregistering the
                                                    // driver
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

        // Set DMA Mask
        // Since crono devices can all handle full 64 bit address as DMA source
        // and destination, we need to set 64-bit mask to avoid using `swiotlb`
        // by linux when calling `dma_map_sg`.
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
                pr_err("Invalid miscdev_type_init argument `dev_type`");
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

        if (NULL == dev_type) {
                pr_err("Invalid miscdev_type_exit argument `dev_type`");
                return;
        }

        // Loop on the registered devices of the this device type.
        for (device_index = 0; device_index < dev_type->bound_drivers_count;
             device_index++) {
                pr_info(
                    "Exiting cronologic miscdev driver: <%s>, minor: <%d>...",
                    dev_type->cdevs[device_index].miscdev.name,
                    dev_type->cdevs[device_index].miscdev.minor);
                misc_deregister(&(dev_type->cdevs[device_index].miscdev));

                // Log to inform deregisteration didn't crash
                pr_info("Done exiting miscdev driver: <%s>",
                        dev_type->cdevs[device_index].miscdev.name);
        }
}

static long crono_miscdev_ioctl(struct file *filp, unsigned int cmd,
                                unsigned long arg) {
        int ret = CRONO_SUCCESS;

        pr_debug("ioctl is called for command <%d>", cmd);

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
        CRONO_BUFFER_INFO_WRAPPER *buff_wrapper = NULL;
#ifdef CRONO_DEBUG_ENABLED
        int ipage, loop_count;
#endif
        pr_debug("Entered _crono_miscdev_ioctl_lock_buffer");

        // Validate, initialize, and lock variables
        if (CRONO_SUCCESS !=
            (ret = _crono_init_buff_wrapper(filp, arg, &buff_wrapper))) {
                return ret;
        }

        // Pin the buffer, allocate and fill `buff_wrapper.kernel_pages`.
        pr_info("Locking buffer of size = <%ld>, address = <%p>",
                buff_wrapper->buff_info.size, buff_wrapper->buff_info.addr);
        if (CRONO_SUCCESS != (ret = _crono_miscdev_ioctl_pin_buffer(
                                  filp, buff_wrapper, GUP_NR_PER_CALL))) {
                goto lock_err;
        }

        // Fill the Scatter/Gather list
        if (CRONO_SUCCESS !=
            (ret = _crono_miscdev_ioctl_generate_sg(filp, buff_wrapper))) {
                goto lock_err;
        }

        // Copy pinned pages physical address to user space
        if (copy_to_user((void __user *)(buff_wrapper->buff_info.upages),
                         buff_wrapper->userspace_pages,
                         buff_wrapper->buff_info.pages_count *
                             sizeof(DMA_ADDR))) {
                pr_err("Error copying back pages addresses");
                ret = -EFAULT;
                goto lock_err;
        }

        // Copy back all data to userspace memory
        if (copy_to_user((void __user *)arg, &(buff_wrapper->buff_info),
                         sizeof(CRONO_BUFFER_INFO))) {
                pr_err("Error copying back buffer information");
                ret = -EFAULT;
                goto lock_err;
        }

#ifdef CRONO_DEBUG_ENABLED
        // Log only first 5 pages, if found
        loop_count = buff_wrapper->buff_info.pages_count < 5
                         ? buff_wrapper->buff_info.pages_count
                         : 5;
        for (ipage = 0; ipage < loop_count; ipage++) {
                pr_debug("ioctl_lock: Userpsace Buffer Page <%d> "
                         "Physical Address is <%llx>\n",
                         ipage, buff_wrapper->userspace_pages[ipage]);
        }
#endif

        // Cleanup
        return CRONO_SUCCESS;

lock_err:
        _crono_release_buff_wrapper(buff_wrapper);
        return ret;
}

static int
_crono_miscdev_ioctl_pin_buffer(struct file *filp,
                                CRONO_BUFFER_INFO_WRAPPER *buff_wrapper,
                                unsigned long nr_per_call) {

        unsigned long start_addr_to_pin; // Start address in pBuf to be pinned
                                         // by `pin_user_pages`.
#ifndef OLD_KERNEL_FOR_PIN
        unsigned long next_pages_addr; // Next address in pBuf to be pinned by
                                       // `pin_user_pages`.
#endif
        long actual_pinned_nr_of_call; // Never unsigned, as it might contain
                                       // error returned
        int ret = CRONO_SUCCESS;
        int page_index;

        pr_debug("Entered _crono_miscdev_ioctl_pin_buffer");
        // Validate parameters
        LOGERR_RET_EINVAL_IF_NULL(buff_wrapper,
                                  "Invalid lock buffer parameters");

        // Allocate struct pages pointer array memory to be filled by
        // `pin_user_pages`. `kernel_pages` contains virtual address,
        // however, you may DMA to/from that memory using the addresses returned
        // from it
        pr_info("Allocating kernel pages. Buffer size = <%ld>, pages number = "
                "<%d>...",
                buff_wrapper->buff_info.size,
                buff_wrapper->buff_info.pages_count);
        // `kvmalloc_array` may return memory that is not physically contiguous.
        buff_wrapper->kernel_pages =
            kvmalloc_array(buff_wrapper->buff_info.pages_count,
                           sizeof(struct page *), GFP_KERNEL);
        LOGERR_RET_ERRNO_IF_NULL(buff_wrapper->kernel_pages,
                                 "Error allocating pages memory", -ENOMEM);
        pr_debug("Allocated `kernel_pages` <%p>, count <%d>, size <%ld>\n",
                 (void *)buff_wrapper->kernel_pages,
                 buff_wrapper->buff_info.pages_count,
                 buff_wrapper->buff_info.pages_count * sizeof(void *));

        start_addr_to_pin = (__u64)buff_wrapper->buff_info.addr;
#ifndef OLD_KERNEL_FOR_PIN
        // https://elixir.bootlin.com/linux/v5.6/source/include/linux/mm.h#L1508
        // Pin buffer blocks, each of size = (nr_per_call * PAGE_SIZE) in every
        // iteration to its corresponding page address in
        // buff_wrapper->kernel_pages
        for (buff_wrapper->pinned_pages_nr = 0;
             start_addr_to_pin <
             (__u64)buff_wrapper->buff_info.addr + buff_wrapper->buff_info.size;
             start_addr_to_pin = next_pages_addr) {
                // Next pages chunk starts after `nr_per_call` pages bytes
                next_pages_addr = start_addr_to_pin + nr_per_call * PAGE_SIZE;
                if (next_pages_addr > ((__u64)buff_wrapper->buff_info.addr +
                                       buff_wrapper->buff_info.size))
                // Exceeded the buffer size
                {
                        next_pages_addr = (__u64)buff_wrapper->buff_info.addr +
                                          buff_wrapper->buff_info.size;
                        nr_per_call =
                            (next_pages_addr - start_addr_to_pin) / PAGE_SIZE;
                }
                actual_pinned_nr_of_call = pin_user_pages(
                    start_addr_to_pin, nr_per_call, FOLL_WRITE,
                    (struct page **)(buff_wrapper->kernel_pages) +
                        buff_wrapper->pinned_pages_nr,
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
                    (buff_wrapper->kernel_pages +
                     buff_wrapper->pinned_pages_nr)[0],
                    (buff_wrapper->kernel_pages +
                     buff_wrapper->pinned_pages_nr)[nr_per_call - 1]);

                buff_wrapper->pinned_pages_nr += actual_pinned_nr_of_call;
        }
#else
#pragma message("Kernel version is older than 5.6")
        // https://elixir.bootlin.com/linux/v4.9/source/include/linux/mm.h#L1278
        down_read(&current->mm->mmap_sem);
        pr_debug(
            "Calling get_user_pages: address <0x%lx>, number of pages: <%d>",
            start_addr_to_pin, buff_wrapper->buff_info.pages_count);
        actual_pinned_nr_of_call = get_user_pages(
            start_addr_to_pin, buff_wrapper->buff_info.pages_count,
            (FOLL_WRITE | FOLL_FORCE),
            (struct page **)(buff_wrapper->kernel_pages), NULL);
        if (actual_pinned_nr_of_call >= 0) {
                pr_debug(
                    "get_user_pages is called successfully with return <%ld>",
                    actual_pinned_nr_of_call);
                buff_wrapper->pinned_pages_nr = actual_pinned_nr_of_call;
        } else {
                pr_err("get_user_pages is called with error return <%ld>",
                       actual_pinned_nr_of_call);
                up_read(&current->mm->mmap_sem);
                _crono_release_buff_wrapper(buffer_wrapper);
                return -EFAULT;
        }
        up_read(&current->mm->mmap_sem);
#endif

        // Validate the number of pinned pages
        if (buff_wrapper->pinned_pages_nr <
            buff_wrapper->buff_info.pages_count) {
                // Apparently not enough memory to pin the whole buffer
                pr_err("Error insufficient available pages to pin");
                _crono_release_buff_wrapper(buff_wrapper);
                if (CRONO_SUCCESS == ret)
                        return -EFAULT;
                else
                        return ret;
        }

        // Fill in the `userspace_pages` Page Array
        for (page_index = 0; page_index < buff_wrapper->buff_info.pages_count;
             page_index++) {
                buff_wrapper->userspace_pages[page_index] =
                    PFN_PHYS(page_to_pfn(
                        (struct page *)buff_wrapper->kernel_pages[page_index]));

                if (page_index < 5)
                        // Log only first 5 pages
                        pr_debug("ioctl_pin: Kernel Page <%d> is of "
                                 "physical address <%llx>",
                                 page_index,
                                 buff_wrapper->userspace_pages[page_index]);
        }

#ifndef OLD_KERNEL_FOR_PIN
        // Calculate actual size in case pinned memory is larger than buffer
        // size
        buff_wrapper->pinned_size =
            start_addr_to_pin - (__u64)buff_wrapper->buff_info.addr;
#else
        buff_wrapper->pinned_size = buff_wrapper->buff_info.size;
#endif

        pr_info(
            "Successfully Pinned buffer: size = <%ld>, number of pages = <%d>",
            buff_wrapper->buff_info.size, buff_wrapper->buff_info.pages_count);

        return ret;
}

static int _crono_miscdev_ioctl_unlock_buffer(struct file *filp,
                                              unsigned long arg) {
        int ret = CRONO_SUCCESS;
        int wrapper_id = -1;
        CRONO_BUFFER_INFO_WRAPPER *found_buff_wrapper = NULL;
        CRONO_BUFFER_INFO_WRAPPER *temp_buff_wrapper = NULL;
        struct list_head *pos, *n;

        // Lock the memeory from user space to kernel space
        if (0 == arg) {
                pr_err("Invalid parameter `arg` unlocking buffer");
                return -EINVAL;
        }
        if (copy_from_user(&wrapper_id, (void __user *)arg, sizeof(int))) {
                pr_err("Error copying user data");
                return -EFAULT;
        }
        pr_debug("Unlocking buffer of wrapper id <%d>...", wrapper_id);

        // Find the related buffer_wrapper in the list
        _crono_debug_list_wrappers();
        list_for_each_safe(pos, n, &buff_wrappers_head) {
                temp_buff_wrapper =
                    list_entry(pos, CRONO_BUFFER_INFO_WRAPPER, list);
                if (temp_buff_wrapper->buff_info.id == wrapper_id)
                        found_buff_wrapper = temp_buff_wrapper;
        }
        if (NULL == found_buff_wrapper) {
                pr_err("Buffer Wrapper of id <%d> is not found in "
                       "internal list",
                       wrapper_id);
                return -EINVAL;
        } else {
                pr_debug("Found wrapper of id <%d> in the internal list",
                         found_buff_wrapper->buff_info.id);
        }

        // Clean up buffer memory allocated in the kernel module
        ret = _crono_release_buff_wrapper(found_buff_wrapper);

        // Copy back just to obey DMA APIs rules, now we have all
        // information needed in `buff_info`
        if (copy_to_user((void __user *)arg, &wrapper_id, sizeof(int))) {
                return -EFAULT;
        }

        // Free the wrapper after all members cleanup is done
        kvfree(found_buff_wrapper);

        return ret;
}

static int _crono_miscdev_ioctl_cleanup_setup(struct file *filp,
                                              unsigned long arg) {
        int ret = CRONO_SUCCESS;
        struct pci_dev *devp;

        // Get the relevant `pci_dev` object from the internal structure.
        ret = _crono_get_dev_from_filp(filp, &devp);
        if (ret != CRONO_SUCCESS) {
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

static int
_crono_miscdev_ioctl_generate_sg(struct file *filp,
                                 CRONO_BUFFER_INFO_WRAPPER *buff_wrapper) {

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
        LOGERR_RET_EINVAL_IF_NULL(buff_wrapper->kernel_pages,
                                  "Invalid pages to get addresses for");
        if (buff_wrapper->sgt != NULL) {
                pr_err("Invalid DMA SG address, already allocated");
                return -EFAULT;
        }

        // Allocate sgt, as `allocate_sg_table` does not allocate it.
        sgt = buff_wrapper->sgt =
            (struct sg_table *)kvzalloc(sizeof(struct sg_table), GFP_KERNEL);
        if (NULL == sgt) {
                pr_err("Error allocating memory");
                return -ENOMEM;
        }

        // Get the corresponding device structure to be used in `dma_map_sg`
        ret = _crono_get_dev_from_filp(filp, &devp);
        if (ret != CRONO_SUCCESS) {
                // Clean up
                crono_kvfree(sgt);
                return ret;
        }

        // Allocate sg_table to hold the scatter/gather segments array.
        // With scatter lists, we map a region gathered from several regions
        // If passed_pages_count > SG_MAX_SINGLE_ALLOC (much smaller than 4096),
        // then a chained sg table will be setup
        pr_info("Allocating SG Table for buffer size = <%ld>, number of "
                "pages = <%d>...",
                buff_wrapper->buff_info.size,
                buff_wrapper->buff_info.pages_count);
#ifndef USE__sg_alloc_table_from_pages
        ret = sg_alloc_table_from_pages(
            sgt, // The sg table header to use
            (struct page **)buff_wrapper
                ->kernel_pages, // Pointer to an array of page pointers
            buff_wrapper->buff_info
                .pages_count, // Number of pages in the pages array
            0, // Offset from start of the first page to the start of a buffer
            buff_wrapper->buff_info
                .size, // Number of valid bytes in the buffer (after offset)
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
            (struct page **)buff_wrapper
                ->kernel_pages, // Pointer to an array of page pointers
            buff_wrapper->buff_info
                .pages_count, // Number of pages in the pages array
            0, // Offset from start of the first page to the start of a buffer
            buff_wrapper->buff_info
                .size, // Number of valid bytes in the buffer (after offset)
            PAGE_SIZE, NULL, 0, GFP_KERNEL);
        if (PTR_ERR_OR_ZERO(sg_from_pages))
#endif
        {
                pr_err("Error allocating SG table from pages");

                // Clean up
                crono_kvfree(sgt);

                return ret;
        }
        pr_info("Done allocating SG Table");

#ifdef USE__sg_alloc_table_from_pages
        // Using `__sg_alloc_table_from_pages` results in `nents` number equal
        // to Pages Number; while using `sg_alloc_table_from_pages` will most
        // probably result in a different number of `nents`.
        if (sgt->nents != buff_wrapper->buff_info.pages_count) {
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
        pr_info("Done mapping SG");

        pr_debug(
            "SG Table is allocated of scatter lists total nents number = <%d>"
            ", Mapped buffers count <%d>",
            sgt->nents, mapped_buffers_count);

        // Success
        return CRONO_SUCCESS;
}

static int
_crono_release_buff_wrapper(CRONO_BUFFER_INFO_WRAPPER *passed_buff_wrapper) {

        CRONO_BUFFER_INFO_WRAPPER *temp_buff_wrapper = NULL;
        CRONO_BUFFER_INFO_WRAPPER *found_buff_wrapper = NULL;
        struct list_head *pos, *n;
#ifdef OLD_KERNEL_FOR_PIN
        int ipage;
#endif

        if (NULL == passed_buff_wrapper ||
            NULL == passed_buff_wrapper->kernel_pages) {

                pr_debug("Nothing to clean for the buffer");
                return CRONO_SUCCESS;
        }
        pr_debug(
            "Releasing buffer of wrapper id <%d>, address <%p>, size <%ld>",
            passed_buff_wrapper->buff_info.id,
            passed_buff_wrapper->buff_info.addr,
            passed_buff_wrapper->buff_info.size);

#ifndef OLD_KERNEL_FOR_PIN
        // Unpin pages
        pr_debug("Unpinning pages of address = <%p>, and number = <%d>...",
                 passed_buff_wrapper->kernel_pages,
                 passed_buff_wrapper->pinned_pages_nr);
        unpin_user_pages((struct page **)(passed_buff_wrapper->kernel_pages),
                         passed_buff_wrapper->pinned_pages_nr);
        pr_debug("Done unpinning pages");
#else
        pr_debug("Putting pages of address = <%p>, and number = <%d>...",
                 passed_buff_wrapper->kernel_pages,
                 passed_buff_wrapper->pinned_pages_nr);
        for (ipage = 0; ipage < passed_buff_wrapper->pinned_pages_nr; ipage++) {
                put_page(passed_buff_wrapper->kernel_pages[ipage]);
        }
        pr_debug("Done putting pages");
#endif

        if (NULL != passed_buff_wrapper->sgt) {
                // Unmap Scatter/Gather list
                dma_unmap_sg(
                    &(passed_buff_wrapper->devp->dev),
                    ((struct sg_table *)passed_buff_wrapper->sgt)->sgl,
                    ((struct sg_table *)passed_buff_wrapper->sgt)->nents,
                    DMA_BIDIRECTIONAL);

                // Clean allocated memory for Scatter/Gather list
                pr_debug("Wrapper<%d>: Cleanup SG Table <%p>...",
                         passed_buff_wrapper->buff_info.id,
                         passed_buff_wrapper->sgt);
                sg_free_table(passed_buff_wrapper->sgt);
                crono_kvfree(passed_buff_wrapper->sgt);
                pr_debug("Done cleanup wrapper <%d> SG Table",
                         passed_buff_wrapper->buff_info.id);
        }

        // Clean allocated memory for kernel pages
        pr_debug("Wrapper<%d>: Cleanup kernel_pages <%p>...",
                 passed_buff_wrapper->buff_info.id,
                 passed_buff_wrapper->kernel_pages);
        crono_kvfree(passed_buff_wrapper->kernel_pages);
        pr_debug("Done cleanup wrapper <%d> kernel_pages",
                 passed_buff_wrapper->buff_info.id);

        // Clean allocated memory for userspace pages
        pr_debug("Wrapper<%d>: Cleanup userspace_pages <%p>...",
                 passed_buff_wrapper->buff_info.id,
                 passed_buff_wrapper->userspace_pages);
        crono_kvfree(passed_buff_wrapper->userspace_pages);
        pr_debug("Done cleanup wrapper <%d> userspace_pages",
                 passed_buff_wrapper->buff_info.id);

        // Delete the wrapper from the list
        pr_debug("Wrapper<%d>: Deleting from list...",
                 passed_buff_wrapper->buff_info.id);
        crono_kvfree(passed_buff_wrapper->userspace_pages);
        list_for_each_safe(pos, n, &buff_wrappers_head) {
                temp_buff_wrapper =
                    list_entry(pos, CRONO_BUFFER_INFO_WRAPPER, list);
                if (temp_buff_wrapper->buff_info.id ==
                    passed_buff_wrapper->buff_info.id) {
                        found_buff_wrapper = temp_buff_wrapper;
                }
        }
        if (NULL != found_buff_wrapper) {
                list_del(&(temp_buff_wrapper->list));
                pr_debug("Done deleting wrapper <%d> from list",
                         passed_buff_wrapper->buff_info.id);
                // Don't free temp_buff_wrapper here, caller should free it.
                // kvfree(temp_buff_wrapper) crashes here.
        } else {
                pr_err("Wrapper<%d>: Not found in wrappers list",
                       passed_buff_wrapper->buff_info.id);
        }
        _crono_debug_list_wrappers();

        // Success
        pr_debug("Done releasing buffer of wrapper id <%d>",
                 passed_buff_wrapper->buff_info.id);
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
        pr_debug("Driver open() is called <%d> times", counter++);

        return CRONO_SUCCESS;
}

/* Called when a process closes the device file */
static int crono_miscdev_release(struct inode *inode, struct file *file) {

        Device_Open--; /* We're now ready for our next caller */

        // Decrement the usage count, or else once you opened the file, you'll
        //  never get rid of the module.

        pr_debug("Driver release() is called");

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

        pr_err("Error getting device information from file driver minor: <%d>",
               file_drv_minor);

        // Corresponding device is not found
        return -ENODATA;
}

static int
_crono_init_buff_wrapper(struct file *filp, unsigned long arg,
                         CRONO_BUFFER_INFO_WRAPPER **pp_buff_wrapper) {

        int ret = CRONO_SUCCESS;
        CRONO_BUFFER_INFO_WRAPPER
        *buff_wrapper = NULL; // To simplify pointer-to-pointer

        if (0 == arg) {
                pr_err("Invalid parameter `arg` initializing buffer wrapper");
                return -EINVAL;
        }

        // Allocate and initialize `buff_wrapper`
        *pp_buff_wrapper = buff_wrapper =
            kmalloc(sizeof(CRONO_BUFFER_INFO_WRAPPER), GFP_KERNEL);
        if (NULL == buff_wrapper) {
                pr_err("Error allocating DMA internal struct");
                return -ENOMEM;
        }
        buff_wrapper->kernel_pages = NULL;
        buff_wrapper->userspace_pages = NULL;
        buff_wrapper->pinned_pages_nr = 0;
        buff_wrapper->sgt = NULL;

        // Get device pointer
        ret = _crono_get_dev_from_filp(filp, &(buff_wrapper->devp));
        if (ret != CRONO_SUCCESS) {
                goto func_err;
        }

        // Lock the memeory from user space to kernel space
        if (copy_from_user(&(buff_wrapper->buff_info), (void __user *)arg,
                           sizeof(CRONO_BUFFER_INFO))) {
                pr_err("Error copying user data");
                ret = -EFAULT;
                goto func_err;
        }

        // Validate address
        LOGERR_RET_EINVAL_IF_NULL(buff_wrapper->buff_info.addr,
                                  "Invalid buffer to be locked");
        // Validate passed buffer size, and `pages_count` value is consistent
        // with the passed buffer size
        if (buff_wrapper->buff_info.size > ULONG_MAX) {
                pr_err("Size <%ld> of requested buffer to lock exceeds "
                       "the maximum size allowed",
                       buff_wrapper->buff_info.size);
                ret = -EINVAL;
                goto func_err;
        }
        if (buff_wrapper->buff_info.pages_count !=
            DIV_ROUND_UP(buff_wrapper->buff_info.size, PAGE_SIZE)) {
                pr_err(
                    "Error: incorrect passed pages count <%d>, expected <%ld>",
                    buff_wrapper->buff_info.pages_count,
                    DIV_ROUND_UP(buff_wrapper->buff_info.size, PAGE_SIZE));
                ret = -ENOMEM;
                goto func_err;
        }

        // Allocate memory in kernel space for pages addressess
        pr_debug("Allocating kernel pages structure of size <%ld>",
                 buff_wrapper->buff_info.pages_count * sizeof(DMA_ADDR));
        buff_wrapper->userspace_pages = (DMA_ADDR *)kvmalloc_array(
            buff_wrapper->buff_info.pages_count, sizeof(DMA_ADDR), GFP_KERNEL);
        if (NULL == buff_wrapper->userspace_pages) {
                pr_err("Error allocating memory");
                ret = -ENOMEM;
                goto func_err;
        }

        // Locking pages memory in user space. No real copy is needed. All
        // should be replaced when pinning.
        pr_debug("Copying kernel pages structure from address <0x%llx>",
                 buff_wrapper->buff_info.upages);
        if (copy_from_user(buff_wrapper->userspace_pages,
                           (void __user *)(buff_wrapper->buff_info.upages),
                           buff_wrapper->buff_info.pages_count *
                               sizeof(DMA_ADDR))) {
                ret = -EFAULT;
                goto func_err;
        }

        // Add the buffer to list
        buff_wrapper->buff_info.id = buff_wrappers_new_id;
        list_add(&(buff_wrapper->list), &buff_wrappers_head);
        pr_debug(
            "Added internal buffer wrapper. Address <%p>, size <%ld>, id <%d>",
            buff_wrapper->buff_info.addr, buff_wrapper->buff_info.size,
            buff_wrapper->buff_info.id);
        buff_wrappers_new_id++;
        _crono_debug_list_wrappers();

        return ret;

func_err:
        crono_kvfree(buff_wrapper->userspace_pages);
        crono_kvfree(buff_wrapper);
        return ret;
}

static void _crono_debug_list_wrappers(void) {

        CRONO_BUFFER_INFO_WRAPPER *temp_buff_wrapper = NULL;
        pr_debug("Listing wrappers...");
        // Find the related buffer_wrapper in the list
        list_for_each_entry(temp_buff_wrapper, &buff_wrappers_head, list) {
                pr_debug("- Wrapper<%d>: address <%p>, size<%ld>",
                         temp_buff_wrapper->buff_info.id,
                         temp_buff_wrapper->buff_info.addr,
                         temp_buff_wrapper->buff_info.size);
        }
}