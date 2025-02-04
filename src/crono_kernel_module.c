#include "crono_kernel_module.h"
#include "crono_miscdevice.h"

// _____________________________________________________________________________
// Globals
//
MODULE_DESCRIPTION("cronologic PCI driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.4.1");

#ifndef CRONO_KERNEL_MODE
#pragma message("CRONO_KERNEL_MODE must be defined in the kernel module")
#endif

#define PR_DEBUG_BW_INFO(prefix, bw)                                           \
        pr_debug("%s wrapper id: <%d>, address <0x%p>, size <%ld>, PID <%d>",  \
                 prefix, bw->buff_info.id, (void *)bw->buff_info.addr,         \
                 bw->buff_info.size, bw->ntrn.app_pid);

// `CRONO_KERNEL_MODE` must be defined indicating the code runs for driver.
// #define CRONO_KERNEL_MODE, no need to define it here as it's passed in
// Makefile

/**
 * Static array holds all the registered miscdevs.
 * Dynamically allocating the miscdev objects has problems with `misc_register`
 * in static functions in the module.
 * It's not likely that a crono miscdev is unregistered in the middle of the
 * array.
 */
static struct crono_miscdev crono_miscdev_pool[CRONO_MAX_MSCDEV_COUNT];
static uint32_t crono_miscdev_pool_new_index = 0;
#define RESET_CRONO_MISCDEV(pcrono_miscdev)                                    \
        memset(pcrono_miscdev, 0, sizeof(struct crono_miscdev));
static int crono_mmap_contig(struct file *file, struct vm_area_struct *vma);
static int get_bw(int bw_id, CRONO_CONTIG_BUFFER_INFO_WRAPPER **ppBW);

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

static struct file_operations crono_miscdev_fops = {
    .owner = THIS_MODULE,

    .open = crono_miscdev_open,
    .release = crono_miscdev_release,

    .unlocked_ioctl = crono_miscdev_ioctl,

    .mmap = crono_mmap_contig,
};

// DMA Buffer Information Wrappers List Variables and Functions
/**
 * @brief Head of list of all locked Buffer Wrappers
 * Two lists, one for each buffer type as every type has its own size and specs.
 */
struct list_head sg_buff_wrappers_head;
struct list_head contig_buff_wrappers_head;

/**
 * @brief Count of Buffer Wrappers in buffer wrappers list
 */
static int sg_buff_wrappers_new_id = 0;
static int contig_buff_wrappers_new_id = 0;

// _____________________________________________________________________________
// init & exit
//
static int __init crono_driver_init(void) {

        int ret = CRONO_SUCCESS;

        pr_info("Registering PCI Driver...");

        // Initialize linked lists and global variables
        // Should be done before calling `pci_register_driver` which will probe
        // the devices and calls `crono_driver_probe` that uses those global
        // variables.
        INIT_LIST_HEAD(&sg_buff_wrappers_head);
        INIT_LIST_HEAD(&contig_buff_wrappers_head);
        memset(crono_miscdev_pool, 0,
               sizeof(struct crono_miscdev) * CRONO_MAX_MSCDEV_COUNT);

        // Register the driver, and start probing
        ret = pci_register_driver(&crono_pci_driver);
        if (ret) {
                pr_err("Error Registering PCI Driver, <%d>!!!", ret);
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

        int icrono_miscdev;

        // Unregister all registered devices miscdevs
        if (crono_miscdev_pool_new_index)
                pr_info("Unregistering <%d> miscellaneous devices...",
                        crono_miscdev_pool_new_index);

        for (icrono_miscdev = 0; icrono_miscdev < crono_miscdev_pool_new_index;
             icrono_miscdev++) {

                if (0 == crono_miscdev_pool[icrono_miscdev].miscdev.minor) {
                        // Invalid item
                        pr_debug(
                            "Invalid item in crono_miscdev_pool, index <%d>",
                            icrono_miscdev);
                        continue;
                }
                // Deregister the miscdev
                pr_info(
                    "Exiting cronologic miscdev driver: <%s>, minor: <%d>...",
                    crono_miscdev_pool[icrono_miscdev].miscdev.name,
                    crono_miscdev_pool[icrono_miscdev].miscdev.minor);
                misc_deregister(&(crono_miscdev_pool[icrono_miscdev].miscdev));
                // Log to inform deregisteration didn't crash
                pr_info("Done exiting miscdev driver: <%s>",
                        crono_miscdev_pool[icrono_miscdev].miscdev.name);

                // Reset the record
                RESET_CRONO_MISCDEV(&(crono_miscdev_pool[icrono_miscdev]));
        }
        if (crono_miscdev_pool_new_index) {
                pr_info("Done unregistering miscellaneous devices");
                crono_miscdev_pool_new_index = 0;
        }

        // Release all buffer wrappers, assuming their applications are
        // terminated
        _crono_release_buffer_wrappers();

        // Unregister the driver
        pr_info("Removing Driver...");
        pci_unregister_driver(&crono_pci_driver);
        pr_info("Done removing cronologic PCI driver");
}
module_exit(crono_driver_exit);

// Probe
static int crono_driver_probe(struct pci_dev *dev,
                              const struct pci_device_id *id) {

        int ret = CRONO_SUCCESS;
        struct crono_miscdev *new_crono_miscdev = NULL;

        // Check the device to claim if concerned
        pr_info("Probe Device, ID <0x%02X>", dev->device);
        if (id->vendor != CRONO_VENDOR_ID)
                return -EINVAL;
        if ((dev->device > CRONO_DEVICE_DEV_ID_MAX_COUNT) ||
            (dev->device < 0)) {
                pr_err("Error Device ID <0x%02x> not supported", dev->device);
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
        if (CRONO_SUCCESS !=
            (ret = _crono_miscdev_init(dev, id, &new_crono_miscdev))) {
                goto error_miscdev;
        }

        // Set DMA Mask
        // Since SG crono devices can all handle full 64 bit address as DMA
        // source and destination, we need to set 64-bit mask to avoid using
        // `swiotlb` by linux when calling `dma_map_sg`.
        ret = dma_set_mask(&dev->dev, DMA_BIT_MASK(64));
        if (ret != CRONO_SUCCESS) {
                pr_err("Device cannot perform DMA properly on this platform, "
                       "error <%d>",
                       ret);

                goto error_miscdev;
        }

        // Log and return
        if (NULL != new_crono_miscdev)
                pr_info("Done probing with minor: <%d>",
                        new_crono_miscdev->miscdev.minor);
        else
                pr_err("Invalid crono_miscdev object of initialized miscdev");
        return ret;

error_miscdev:
        pci_disable_device(dev);
        // Reset the record
        if (NULL != new_crono_miscdev)
                RESET_CRONO_MISCDEV(new_crono_miscdev);
        return ret;
}

// _____________________________________________________________________________
// Miscellaneous Device Driver
char testval[20] = "testval";
static int _crono_miscdev_init(struct pci_dev *dev,
                               const struct pci_device_id *id,
                               struct crono_miscdev **crono_dev) {

        int ret = CRONO_SUCCESS;
        struct crono_miscdev *new_crono_miscdev = NULL;

        // Validations
        if (dev == NULL) {
                pr_err("Invalid miscdev_type_init argument `dev`");
                return -EINVAL;
        }
        if (id == NULL) {
                pr_err("Invalid miscdev_type_init argument `id`");
                return -EINVAL;
        }
        if (crono_dev == NULL) {
                pr_err("Invalid miscdev_type_init argument `crono_dev`");
                return -EINVAL;
        }

        // Initialize crono_miscdev and generate the device name
        new_crono_miscdev = &(crono_miscdev_pool[crono_miscdev_pool_new_index]);
        new_crono_miscdev->dev = dev;
        new_crono_miscdev->device_id = dev->device;
        if (CRONO_SUCCESS !=
            (ret = _crono_get_DBDF_from_dev(dev, &(new_crono_miscdev->dbdf)))) {
                goto init_err;
        }
        pr_info("Probed device BDBF: <%04X:%02X:%02X.%01X>",
                new_crono_miscdev->dbdf.domain, new_crono_miscdev->dbdf.bus,
                new_crono_miscdev->dbdf.dev, new_crono_miscdev->dbdf.func);

        CRONO_CONSTRUCT_MISCDEV_NAME(new_crono_miscdev->name,
                                     new_crono_miscdev->device_id,
                                     new_crono_miscdev->dbdf);

        // Initialize the miscdevice object
        new_crono_miscdev->miscdev.minor = MISC_DYNAMIC_MINOR;
        new_crono_miscdev->miscdev.fops = &crono_miscdev_fops;
        new_crono_miscdev->miscdev.name = new_crono_miscdev->name;

        pr_info("Initializing cronologic miscdev driver: <%s>...",
                new_crono_miscdev->name);

        // Register the device driver
        ret = misc_register(&(new_crono_miscdev->miscdev));
        if (ret) {
                pr_err("Can't register misdev: <%s>, error: <%d>",
                       new_crono_miscdev->miscdev.name, ret);
                goto init_err;
        }

        // Increments the new index in the pool for the new device
        crono_miscdev_pool_new_index++;

        // Log and return
        *crono_dev = new_crono_miscdev;
        return ret;

init_err:
        // Reset object
        if (NULL != new_crono_miscdev) {
                RESET_CRONO_MISCDEV(new_crono_miscdev);
        }
        return ret;
}

static long crono_miscdev_ioctl(struct file *filp, unsigned int cmd,
                                unsigned long arg) {
        int ret = CRONO_SUCCESS;

        pr_debug("ioctl is called for command <0x%x>, PID <%d>", cmd,
                 task_pid_nr(current));

        switch (cmd) {
        case IOCTL_CRONO_LOCK_BUFFER: // 0xc0086300
                ret = _crono_miscdev_ioctl_lock_sg_buffer(filp, arg);
                break;
        case IOCTL_CRONO_UNLOCK_BUFFER: // 0xc0086301
                ret = _crono_miscdev_ioctl_unlock_sg_buffer(filp, arg);
                break;
        case IOCTL_CRONO_CLEANUP_SETUP: // 0xc0086302
                ret = _crono_miscdev_ioctl_cleanup_setup(filp, arg);
                break;
        case IOCTL_CRONO_LOCK_CONTIG_BUFFER: // 0xc0086303
                ret = _crono_miscdev_ioctl_lock_contig_buffer(filp, arg);
                break;
        case IOCTL_CRONO_UNLOCK_CONTIG_BUFFER: // 0xc0086304
                ret = _crono_miscdev_ioctl_unlock_contig_buffer(filp, arg);
                break;
        default:
                pr_err("Error, unsupported ioctl command <%d>", cmd);
                ret = -ENOTTY;
                break;
        }
        return ret;
}

/**
 * @brief
 * - Allocate memory, pin it.
 * - Create a wrapper (`CRONO_SG_BUFFER_INFO_WRAPPER`), fill its `buff_info`
 *   from `arg` (`CRONO_SG_BUFFER_INFO`) and set its `id` (unique in module),
 *   then, add wrapper to list `sg_buff_wrappers_head`.
 *
 * Unlock function receives `id` of `buff_info` to unlock it.
 *
 * @param filp
 * Of the device
 * @param arg
 * `CRONO_SG_BUFFER_INFO`
 * @return int
 * `CRONO_SUCCESS` or error code.
 */
static int _crono_miscdev_ioctl_lock_sg_buffer(struct file *filp,
                                               unsigned long arg) {
        int ret;
        CRONO_SG_BUFFER_INFO_WRAPPER *buff_wrapper = NULL;
#ifdef CRONO_DEBUG_ENABLED
        int ipage, loop_count;
#endif
        pr_debug("Locking buffer...");

        // Validate, initialize, and lock variables
        if (CRONO_SUCCESS !=
            (ret = _crono_init_sg_buff_wrapper(filp, arg, &buff_wrapper))) {
                return ret;
        }

        // Pin the buffer, allocate and fill `buff_wrapper.kernel_pages`.
        pr_debug("Buffer: address <0x%p>, size <%ld>, PID <%d>",
                 buff_wrapper->buff_info.addr, buff_wrapper->buff_info.size,
                 task_pid_nr(current));
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
                pr_err("Error copying pages addresses back to user space");
                ret = -EFAULT;
                goto lock_err;
        }

        // Copy back all data to userspace memory
        if (copy_to_user((void __user *)arg, &(buff_wrapper->buff_info),
                         sizeof(CRONO_SG_BUFFER_INFO))) {
                pr_err("Error copying buffer information back to user space");
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
        pr_info("Done locking buffer: wrapper id <%d>",
                buff_wrapper->buff_info.id);

        // Cleanup
        return CRONO_SUCCESS;

lock_err:
        _crono_release_buff_wrapper(buff_wrapper);
        return ret;
}

static int
_crono_miscdev_ioctl_pin_buffer(struct file *filp,
                                CRONO_SG_BUFFER_INFO_WRAPPER *buff_wrapper,
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
        // int page_index;

        pr_debug("Pinning buffer...");

        // Validate parameters
        LOGERR_RET_EINVAL_IF_NULL(buff_wrapper,
                                  "Invalid lock buffer parameters");

        // Allocate struct pages pointer array memory to be filled by
        // `pin_user_pages`. `kernel_pages` contains virtual address,
        // however, you may DMA to/from that memory using the addresses returned
        // from it
        pr_debug("Allocating kernel pages. Buffer size = <%ld>, pages number = "
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
#ifndef KERNEL_6_5_OR_LATER
#pragma message("Kernel version is older than 6.5 but newer than 5.5")
                actual_pinned_nr_of_call = pin_user_pages(
                    start_addr_to_pin, nr_per_call, FOLL_WRITE,
                    (struct page **)(buff_wrapper->kernel_pages) +
                        buff_wrapper->pinned_pages_nr, NULL);
#else                        
                actual_pinned_nr_of_call = pin_user_pages(
                    start_addr_to_pin, nr_per_call, FOLL_WRITE,
                    (struct page **)(buff_wrapper->kernel_pages) +
                        buff_wrapper->pinned_pages_nr);
#endif                        


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
                _crono_release_buff_wrapper(buff_wrapper);
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

        // Old way to fill in the `userspace_pages` Page Array, don't fill it
        // now it will be filled later on from sg
        // for (page_index = 0; page_index <
        // buff_wrapper->buff_info.pages_count;
        //      page_index++) {
        //         buff_wrapper->userspace_pages[page_index] =
        //             PFN_PHYS(page_to_pfn(
        //                 (struct page
        //                 *)buff_wrapper->kernel_pages[page_index]));
        // #ifdef DEBUG
        //                 if (page_index < 5)
        //                         // Log only first 5 pages
        //                         pr_debug("ioctl_pin: Kernel Page <%d> is of "
        //                                  "physical address <0x%llx>",
        //                                  page_index,
        //                                  buff_wrapper->userspace_pages[page_index]);
        // #endif
        // }

#ifndef OLD_KERNEL_FOR_PIN
        // Calculate actual size in case pinned memory is larger than buffer
        // size
        buff_wrapper->pinned_size =
            start_addr_to_pin - (__u64)buff_wrapper->buff_info.addr;
#else
        buff_wrapper->pinned_size = buff_wrapper->buff_info.size;
#endif

        pr_debug(
            "Successfully Pinned buffer: size = <%ld>, number of pages = <%d>",
            buff_wrapper->buff_info.size, buff_wrapper->buff_info.pages_count);

        return ret;
}

static int _crono_miscdev_ioctl_unlock_sg_buffer(struct file *filp,
                                                 unsigned long arg) {
        int ret = CRONO_SUCCESS;
        int wrapper_id = -1;
        CRONO_SG_BUFFER_INFO_WRAPPER *found_buff_wrapper = NULL;
        CRONO_SG_BUFFER_INFO_WRAPPER *temp_buff_wrapper = NULL;
        struct list_head *pos, *n;

        // Lock the memory from user space to kernel space
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
        list_for_each_safe(pos, n, &sg_buff_wrappers_head) {
                temp_buff_wrapper =
                    list_entry(pos, CRONO_SG_BUFFER_INFO_WRAPPER, ntrn.list);
                if (temp_buff_wrapper->buff_info.id == wrapper_id)
                        found_buff_wrapper = temp_buff_wrapper;
        }
        if (NULL == found_buff_wrapper) {
                pr_warn("Buffer Wrapper of id <%d> is not found in "
                        "internal list",
                        wrapper_id);

                // Returning error will cause any coming open to fail returning
                // error EFAULT 14
                // Case might happen when closing after multiple opens
                return CRONO_SUCCESS;
        } else {
                pr_debug("Found wrapper of id <%d> in the internal list",
                         found_buff_wrapper->buff_info.id);
        }

        // Clean up buffer memory allocated in the kernel module
        ret = _crono_release_buff_wrapper(found_buff_wrapper);

        // Copy back just to obey DMA APIs rules
        if (copy_to_user((void __user *)arg, &wrapper_id, sizeof(int))) {
                ret = -EFAULT;
        }

        // Free the wrapper after all members cleanup is done
        kvfree(found_buff_wrapper);

        return ret;
}

static int _crono_miscdev_ioctl_cleanup_setup(struct file *filp,
                                              unsigned long arg) {
        int ret = CRONO_SUCCESS;
        struct crono_miscdev *crono_miscdev = NULL;
        CRONO_KERNEL_CMDS_INFO cmds_info;

        pr_debug("Setup cleanup commands...");

        // Get crono device pointer in internal structure
        if (CRONO_SUCCESS !=
            (ret = _crono_get_crono_dev_from_filp(filp, &crono_miscdev))) {
                return ret;
        }

        // Lock the memory from user space to kernel space
        if (copy_from_user(&cmds_info, (void __user *)arg,
                           sizeof(CRONO_KERNEL_CMDS_INFO))) {
                pr_err("Error copying user data");
                return -EFAULT;
        }

        // Get the tranaction commands count and copy them
        pr_debug("Cleanup commands: count <%d>", crono_miscdev->cmds_count);
        if (cmds_info.count > CLEANUP_CMD_COUNT) {
                pr_err("Transaction objects count <%d> is greater than the "
                       "maximum <%d>",
                       crono_miscdev->cmds_count, CLEANUP_CMD_COUNT);
                crono_miscdev->cmds_count = CLEANUP_CMD_COUNT;
        } else {
                crono_miscdev->cmds_count = cmds_info.count;
        }
        if (copy_from_user(
                crono_miscdev->cmds, (void __user *)(cmds_info.ucmds),
                sizeof(CRONO_KERNEL_CMD) * crono_miscdev->cmds_count)) {
                pr_err("Error copying user data");
                return -EFAULT;
        }
        // Copy back all data to userspace memory
        if (copy_to_user((void __user *)(cmds_info.ucmds), crono_miscdev->cmds,
                         sizeof(CRONO_KERNEL_CMD) *
                             crono_miscdev->cmds_count)) {
                pr_err("Error copying user data");
                return -EFAULT;
        }
        if (copy_to_user((void __user *)arg, &cmds_info,
                         sizeof(CRONO_KERNEL_CMDS_INFO))) {
                pr_err("Error copying back buffer information");
                return -EFAULT;
        }

        pr_debug("Done setup cleanup commands");
        return ret;
}

static int
_crono_miscdev_ioctl_generate_sg(struct file *filp,
                                 CRONO_SG_BUFFER_INFO_WRAPPER *buff_wrapper) {

        struct pci_dev *devp;
        int ret;
        struct sg_table *sgt = NULL;
        int mapped_buffers_count = 0;
        struct scatterlist *sg;
#ifdef USE__sg_alloc_table_from_pages
        int page_index;
        struct scatterlist *sg_from_pages = NULL;
        DWORD dw_mapped_pages_size = 0;
#endif
        int page_nr = 0;
        int i = 0;

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

        // Get the corresponding device structure to be used in
        // `dma_map_sg`
        ret = _crono_get_dev_from_filp(filp, &devp);
        if (ret != CRONO_SUCCESS) {
                // Clean up
                crono_kvfree(sgt);
                return ret;
        }

        // Allocate sg_table to hold the scatter/gather segments array.
        // With scatter lists, we map a region gathered from several
        // regions If passed_pages_count > SG_MAX_SINGLE_ALLOC (much
        // smaller than 4096), then a chained sg table will be setup
        pr_debug("Allocating SG Table for buffer size = <%ld>, number of "
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
            0, // Offset from start of the first page to the start of a
               // buffer
            buff_wrapper->buff_info.size, // Number of valid bytes in
                                          // the buffer (after offset)
            GFP_KERNEL);
        if (ret)
#else
        // If the physical address got by `PFN_PHYS`, didn't work, then,
        // `__sg_alloc_table_from_pages` should be called with
        // `PAGE_SIZE` parameter, then `sg_dma_address` should be used
        // to fill in the pages addresses, however, it's not guaranteed
        // that the buffers coming out of `dma_map_sg` are 1-PAGE-ONLY
        // SIZE.
        sg_from_pages = __sg_alloc_table_from_pages(
            sgt, // The sg table header to use
            (struct page **)buff_wrapper
                ->kernel_pages, // Pointer to an array of page pointers
            buff_wrapper->buff_info
                .pages_count, // Number of pages in the pages array
            0, // Offset from start of the first page to the start of a
               // buffer
            buff_wrapper->buff_info.size, // Number of valid bytes in
                                          // the buffer (after offset)
            PAGE_SIZE, NULL, 0, GFP_KERNEL);
        if (PTR_ERR_OR_ZERO(sg_from_pages))
#endif
        {
                pr_err("Error allocating SG table from pages");

                // Clean up
                crono_kvfree(sgt);

                return ret;
        }
        pr_debug("Done allocating SG Table");

#ifdef USE__sg_alloc_table_from_pages
        // Using `__sg_alloc_table_from_pages` results in `nents` number
        // equal to Pages Number; while using
        // `sg_alloc_table_from_pages` will most probably result in a
        // different number of `nents`.
        if (sgt->nents != buff_wrapper->buff_info.pages_count) {
                pr_err("Inconsistent SG table elements number with "
                       "number of pages");
                return -EFAULT;
        }
#endif

        pr_debug("Mapping SG...");
        mapped_buffers_count =
            dma_map_sg(&devp->dev, sgt->sgl, sgt->nents, DMA_BIDIRECTIONAL);
        // `ret` is the number of DMA buffers to transfer. `dma_map_sg`
        // coalesces buffers that are adjacent to each other in memory,
        // so `ret` may be less than nents.
        if (mapped_buffers_count < 0) {
                pr_err("Error mapping SG: <%d>", ret);

                // Clean up
                sg_free_table(sgt); // Free sgl, even if it's chained
                crono_kvfree(sgt);

                return ret;
        }
        pr_debug("Done mapping SG");

        pr_debug("SG Table is allocated of scatter lists total nents "
                 "number <%d>"
                 ", Mapped buffers count <%d>",
                 sgt->nents, mapped_buffers_count);

        pr_debug("Filling DMA physical addresses ...");
        for_each_sg(sgt->sgl, sg, mapped_buffers_count, i) {
                int len = sg_dma_len(sg);
                dma_addr_t addr = sg_dma_address(sg);
                while (len > 0) {
                        buff_wrapper->userspace_pages[page_nr] = addr;
                        page_nr++;
                        if (page_nr > buff_wrapper->buff_info.pages_count) {
                                pr_err("Inconsistent number of pages between "
                                       "sg and buffer, "
                                       "sg pages count is <%d>, buffer pages "
                                       "count is <%d>",
                                       page_nr,
                                       buff_wrapper->buff_info.pages_count);
                        }
                        len -= 4096;  // Replace by PAGE_SIZE
                        addr += 4096; // Replace by PAGE_SIZE
                }
        }
        if (page_nr != buff_wrapper->buff_info.pages_count) {
                pr_err("Inconsistent number of pages between sg and buffer, "
                       "sg pages count is <%d>, buffer pages count is <%d>",
                       page_nr, buff_wrapper->buff_info.pages_count);
        }
        pr_debug("Done mapping SG");

        // Success
        return CRONO_SUCCESS;
}

/**
 * @brief
 * - Delete the wrapper from the list
 * - Free the wrapper object
 *
 * @param bw
 * Buffer Wrapper, is not valid upon exit, it's freed
 *
 * @return int
 */
static int _crono_release_sg_buff_wrapper(CRONO_SG_BUFFER_INFO_WRAPPER *bw) {
#ifdef OLD_KERNEL_FOR_PIN
        int ipage;
#endif
        if (NULL == bw || NULL == bw->kernel_pages) {
                pr_debug("Nothing to clean for the buffer");
                return CRONO_SUCCESS;
        }
        _crono_debug_list_wrappers();
        PR_DEBUG_BW_INFO("Releasing buffer:", bw);

#ifndef OLD_KERNEL_FOR_PIN
        // Unpin pages
        pr_debug("Wrapper<%d>: Unpinning pages of address <0x%p>, number = "
                 "<%d>...",
                 bw->buff_info.id, bw->kernel_pages, bw->pinned_pages_nr);
        unpin_user_pages((struct page **)(bw->kernel_pages),
                         bw->pinned_pages_nr);
        pr_debug("Done unpinning pages");
#else
        pr_debug("Putting pages of address = <%p>, and number = <%d>...",
                 bw->kernel_pages, bw->pinned_pages_nr);
        for (ipage = 0; ipage < bw->pinned_pages_nr; ipage++) {
                put_page(bw->kernel_pages[ipage]);
        }
        pr_debug("Done putting pages");
#endif

        if (NULL != bw->sgt) {
                // Unmap Scatter/Gather list
                dma_unmap_sg(
                    &(bw->ntrn.devp->dev), ((struct sg_table *)bw->sgt)->sgl,
                    ((struct sg_table *)bw->sgt)->nents, DMA_BIDIRECTIONAL);

                // Clean allocated memory for Scatter/Gather list
                pr_debug("Wrapper<%d>: Cleanup SG Table <%p>...",
                         bw->buff_info.id, bw->sgt);
                sg_free_table(bw->sgt);
                crono_kvfree(bw->sgt);
                pr_debug("Done cleanup wrapper <%d> SG Table",
                         bw->buff_info.id);
        }

        // Clean allocated memory for kernel pages
        pr_debug("Wrapper<%d>: Cleanup kernel pages <%p>...", bw->buff_info.id,
                 bw->kernel_pages);
        crono_kvfree(bw->kernel_pages);
        pr_debug("Done cleanup wrapper <%d> kernel pages", bw->buff_info.id);

        // Clean allocated memory for userspace pages
        pr_debug("Wrapper<%d>: Cleanup userspace pages <%p>...",
                 bw->buff_info.id, bw->userspace_pages);
        crono_kvfree(bw->userspace_pages);
        pr_debug("Done cleanup wrapper <%d> userspace pages", bw->buff_info.id);

        // Delete the wrapper from the list
        pr_debug("Wrapper<%d>: Deleting from list...", bw->buff_info.id);
        list_del(&(bw->ntrn.list));
        pr_debug("Done deleting wrapper <%d> from list", bw->buff_info.id);
        // Don't free `bw` here, caller should free it.
        // kvfree(bw) crashes here.

        // Success
        pr_info("Done releasing buffer: wrapper id <%d>", bw->buff_info.id);
        _crono_debug_list_wrappers();
        return CRONO_SUCCESS;
}

/**
 * @brief
 * - Delete the wrapper from the list
 * - Free the wrapper object
 *
 * @param bw
 * Is not valid upon exit, it's freed
 *
 * @return int
 */
static int
_crono_release_contig_buff_wrapper(CRONO_CONTIG_BUFFER_INFO_WRAPPER *bw) {
        int ret = CRONO_SUCCESS;

        if (NULL == bw) {
                pr_debug("Nothing to clean for the buffer");
                return CRONO_SUCCESS;
        }
        _crono_debug_list_wrappers();
        PR_DEBUG_BW_INFO("Releasing contiguous buffer:", bw);

        pr_debug("Wrapper<%d>: Cleanup kernel memory...", bw->buff_info.id);
        dma_free_coherent(&bw->ntrn.devp->dev, bw->buff_info.size,
                          bw->buff_info.addr /*buff*/,
                          bw->dma_handle /*dma_handle*/);
        pr_debug("Done cleanup Wrapper<%d> kernel memory.", bw->buff_info.id);

        // Delete the wrapper from the list
        pr_debug("Wrapper<%d>: Deleting from list...", bw->buff_info.id);
        list_del(&(bw->ntrn.list));
        pr_debug("Done deleting wrapper <%d> from list", bw->buff_info.id);
        // Don't free `bw` here, caller should free it.
        // kvfree(bw) crashes here.

        return ret;
}

static int _crono_release_buff_wrapper(void *buff_wrapper) {
        if (*((int *)buff_wrapper) == BWT_SG) {
                return _crono_release_sg_buff_wrapper(buff_wrapper);
        } else if (*((int *)buff_wrapper) == BWT_CONTIG) {
                return _crono_release_contig_buff_wrapper(buff_wrapper);
        } else {
                return -EINVAL;
        }
}

// _____________________________________________________________________________
// Methods
//
static int crono_miscdev_open(struct inode *inode, struct file *filp) {
        int icrono_miscdev, passed_iminor;
        pr_debug("Opening device file: minor <%d>, PID <%d>...", iminor(inode),
                 task_pid_nr(current));

        // Check the file is not opened before
        for (icrono_miscdev = 0, passed_iminor = iminor(inode);
             icrono_miscdev < crono_miscdev_pool_new_index; icrono_miscdev++) {
                // Check the array element is the underlying miscdev
                if (passed_iminor !=
                    crono_miscdev_pool[icrono_miscdev].miscdev.minor)
                        continue;

                // miscdev is found
                // Check if it's the first time the miscdev is opened
                if (0 == crono_miscdev_pool[icrono_miscdev].open_count) {
                        // First time to open the miscdev
                        crono_miscdev_pool[icrono_miscdev].open_count = 1;
                        pr_debug("Device of minor <%d> opened successfully",
                                 passed_iminor);
                        return CRONO_SUCCESS;
                }

                // miscdev is already opened
                pr_warn("Opening an already opened miscdev device of minor "
                        "<%d> is not supported",
                        passed_iminor);

                // Multiple open of the same device is not supported by device.
                // Uncomment the following line and adjust code if you need
                // to support multiple open of the same device.
                // crono_miscdev_pool[icrono_miscdev].open_count++;
                return -EINVAL;
        }
        // Internal error
        pr_err("Trying to open a device of minor <%d> while not found in "
               "crono_miscdev_pool",
               passed_iminor);
        return -ENODEV;
}

static int crono_miscdev_release(struct inode *inode, struct file *filp) {

        int icrono_miscdev, passed_iminor;
        pr_debug("Releasing device file: minor <%d>, PID <%d>", iminor(inode),
                 task_pid_nr(current));

        // Decrement the file open counter
        passed_iminor = iminor(inode);
        for (icrono_miscdev = 0; icrono_miscdev < crono_miscdev_pool_new_index;
             icrono_miscdev++) {
                // Check the array element is the underlying miscdev
                if (passed_iminor !=
                    crono_miscdev_pool[icrono_miscdev].miscdev.minor)
                        continue;

                // miscdev is found
                // Check it's already opened before (at least once)
                if (0 == crono_miscdev_pool[icrono_miscdev].open_count) {
                        // Not opened before. Invalid open count value = 0
                        pr_err("Calling release for an un-open device, or "
                               "inconsistent calls of close() and open() ");
                        return -ENODATA; // No data found for open
                }
                // miscdev is opened (at least once)
                _crono_release_buffer_wrappers_cur_proc();
                _crono_apply_cleanup_commands(inode);

                // Releasing the device will make all "opened instances" invalid
                // so reset open_count as nothing is open after release. Caller
                // can use `ioctl` using `IOCTL_CRONO_GET_DEV_INFO` at any time
                // to get the open_count before releasing.
                crono_miscdev_pool[icrono_miscdev].open_count = 0;
                return CRONO_SUCCESS;
        }

        // Internal error
        pr_err("Trying to release a device of minor <%d> while not found in "
               "crono_miscdev_pool",
               passed_iminor);
        return -ENODEV;
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
                dbdf->domain = pci_domain_nr(dev->bus);
        }
        dbdf->dev = PCI_SLOT(dev->devfn);
        dbdf->func = PCI_FUNC(dev->devfn);

        // Success
        return CRONO_SUCCESS;
}

static int _crono_get_dev_from_filp(struct file *filp, struct pci_dev **devpp) {

        int ret = CRONO_SUCCESS;
        struct crono_miscdev *crono_dev = NULL;

        if (CRONO_SUCCESS !=
            (ret = _crono_get_crono_dev_from_filp(filp, &crono_dev))) {
                return ret;
        }
        *devpp = crono_dev->dev;
        return CRONO_SUCCESS;
}

static int _crono_get_crono_dev_from_filp(struct file *filp,
                                          struct crono_miscdev **crono_devpp) {
        // Validate parameters
        LOGERR_RET_EINVAL_IF_NULL(filp, "Invalid file to get dev for");
        LOGERR_RET_EINVAL_IF_NULL(crono_devpp, "Invalid device pointer");

        // Get the file descriptor minor to match it with the device
        return _crono_get_crono_dev_from_inode(filp->f_path.dentry->d_inode,
                                               crono_devpp);
}

static int
_crono_get_crono_dev_from_inode(struct inode *miscdev_inode,
                                struct crono_miscdev **ppcrono_miscdev) {

        int passed_drv_minor, icrono_miscdev;

        // Validate parameters
        LOGERR_RET_EINVAL_IF_NULL(miscdev_inode,
                                  "Invalid inode to get dev for");
        LOGERR_RET_EINVAL_IF_NULL(ppcrono_miscdev, "Invalid device pointer");

        // Get the file descriptor minor to match it with the device
        passed_drv_minor = iminor(miscdev_inode);

        // Loop on the registered devices of every device type
        for (icrono_miscdev = 0; icrono_miscdev < crono_miscdev_pool_new_index;
             icrono_miscdev++) {
                if (crono_miscdev_pool[icrono_miscdev].miscdev.minor !=
                    passed_drv_minor) {
                        continue;
                }
                // Found a device with the same minor of the file
                *ppcrono_miscdev = &(crono_miscdev_pool[icrono_miscdev]);
                return CRONO_SUCCESS;
        }
        pr_err("Miscdev is not found in internal list: minor: <%d>",
               passed_drv_minor);

        // Corresponding device is not found
        return -ENODATA;
}

static int
_crono_init_sg_buff_wrapper(struct file *filp, unsigned long arg,
                            CRONO_SG_BUFFER_INFO_WRAPPER **pp_buff_wrapper) {

        int ret = CRONO_SUCCESS;
        CRONO_SG_BUFFER_INFO_WRAPPER *buff_wrapper =
            NULL; // To simplify pointer-to-pointer

        if (0 == arg) {
                pr_err("Invalid parameter `arg` initializing buffer "
                       "wrapper");
                return -EINVAL;
        }

        // Allocate and initialize `buff_wrapper`
        // Should be freed using 'crono_kvfree', e.g. after any call to
        // `_crono_release_buffxyz` family.
        *pp_buff_wrapper = buff_wrapper =
            kmalloc(sizeof(CRONO_SG_BUFFER_INFO_WRAPPER), GFP_KERNEL);
        if (NULL == buff_wrapper) {
                pr_err("Error allocating DMA internal struct");
                return -ENOMEM;
        }
        buff_wrapper->ntrn.bwt = BWT_SG;
        buff_wrapper->kernel_pages = NULL;
        buff_wrapper->userspace_pages = NULL;
        buff_wrapper->pinned_pages_nr = 0;
        buff_wrapper->sgt = NULL;
        buff_wrapper->ntrn.app_pid = task_pid_nr(current);

        // Get device pointer in internal structure
        ret = _crono_get_dev_from_filp(filp, &(buff_wrapper->ntrn.devp));
        if (ret != CRONO_SUCCESS) {
                goto func_err;
        }

        // Copy buffer information from user space to kernel space
        if (copy_from_user(&(buff_wrapper->buff_info), (void __user *)arg,
                           sizeof(CRONO_SG_BUFFER_INFO))) {
                pr_err("Error copying user data");
                ret = -EFAULT;
                goto func_err;
        }

        // Validate address
        LOGERR_RET_EINVAL_IF_NULL(buff_wrapper->buff_info.addr,
                                  "Invalid buffer to be locked");
        // Validate passed buffer size, and `pages_count` value is
        // consistent with the passed buffer size
        if (buff_wrapper->buff_info.size > ULONG_MAX) {
                pr_err("Size <%ld> of requested buffer to lock exceeds "
                       "the maximum size allowed",
                       buff_wrapper->buff_info.size);
                ret = -EINVAL;
                goto func_err;
        }
        if (buff_wrapper->buff_info.pages_count !=
            DIV_ROUND_UP(buff_wrapper->buff_info.size, PAGE_SIZE)) {
                pr_err("Error: incorrect passed pages count <%d>, "
                       "expected <%ld>",
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

        // Locking pages memory in user space. No real copy is needed.
        // All should be replaced when pinning.
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
        buff_wrapper->buff_info.id = sg_buff_wrappers_new_id;
        list_add(&(buff_wrapper->ntrn.list), &sg_buff_wrappers_head);
        PR_DEBUG_BW_INFO("Added buffer wrapper to internal list: ",
                         buff_wrapper);
        sg_buff_wrappers_new_id++;
        _crono_debug_list_wrappers();

        return ret;

func_err:
        crono_kvfree(buff_wrapper->userspace_pages);
        crono_kvfree(buff_wrapper);
        return ret;
}

static void _crono_debug_list_wrappers(void) {
#ifdef DEBUG
        CRONO_SG_BUFFER_INFO_WRAPPER *temp_sg_buff_wrapper = NULL;
        CRONO_CONTIG_BUFFER_INFO_WRAPPER *temp_contig_buff_wrapper = NULL;
        bool wrapper_list_is_empty = true;
        struct list_head *pos = NULL, *n = NULL;

        pr_debug("Listing wrappers...");
        // List the wrappers in the list
        list_for_each_safe(pos, n, &sg_buff_wrappers_head) {
                wrapper_list_is_empty = false; // Set the flag
                temp_sg_buff_wrapper =
                    list_entry(pos, CRONO_SG_BUFFER_INFO_WRAPPER, ntrn.list);
                PR_DEBUG_BW_INFO("- Wrapper: ", temp_sg_buff_wrapper);
        }
        list_for_each_safe(pos, n, &contig_buff_wrappers_head) {
                wrapper_list_is_empty = false; // Set the flag
                temp_contig_buff_wrapper = list_entry(
                    pos, CRONO_CONTIG_BUFFER_INFO_WRAPPER, ntrn.list);
                PR_DEBUG_BW_INFO("- Wrapper: ", temp_contig_buff_wrapper);
        }
        if (wrapper_list_is_empty) {
                pr_debug("Wrappers list is empty");
        }
#endif
}

static int _crono_release_buffer_wrappers() {
        struct list_head *pos = NULL, *n = NULL;

        CRONO_SG_BUFFER_INFO_WRAPPER *temp_sg_buff_wrapper = NULL;
        CRONO_CONTIG_BUFFER_INFO_WRAPPER *temp_contig_buff_wrapper = NULL;

        // Clean up all buffer information wrappers and list
        pr_info("Cleanup wrappers list...");

        // SG Buffer Wrappers
        list_for_each_safe(pos, n, &sg_buff_wrappers_head) {
                temp_sg_buff_wrapper =
                    list_entry(pos, CRONO_SG_BUFFER_INFO_WRAPPER, ntrn.list);
                _crono_release_buff_wrapper(temp_sg_buff_wrapper);
                crono_kvfree(temp_sg_buff_wrapper);
                // Don't list_del(pos); it's deleted in
                // `_crono_release_buff_wrapper`
        }

        // Contiguous Buffer Wrappers
        list_for_each_safe(pos, n, &contig_buff_wrappers_head) {
                temp_contig_buff_wrapper = list_entry(
                    pos, CRONO_CONTIG_BUFFER_INFO_WRAPPER, ntrn.list);
                _crono_release_buff_wrapper(temp_contig_buff_wrapper);
                crono_kvfree(temp_contig_buff_wrapper);
                // Don't list_del(pos); it's deleted in
                // `_crono_release_buff_wrapper`
        }

        pr_info("Done cleanup wrappers list");
        _crono_debug_list_wrappers();

        return CRONO_SUCCESS;
}

static int _crono_release_buffer_wrappers_cur_proc() {
        struct list_head *pos = NULL, *n = NULL;
        CRONO_SG_BUFFER_INFO_WRAPPER *temp_sg_buff_wrapper = NULL;
        CRONO_CONTIG_BUFFER_INFO_WRAPPER *temp_contig_buff_wrapper = NULL;
        bool no_wrappers_found = true;
        int app_pid = task_pid_nr(current);

        // Clean up all buffer information wrappers and list
        pr_debug("Cleanup process PID <%d> buffers wrappers...", app_pid);
        _crono_debug_list_wrappers();

        // SG Buffer Wrappers
        list_for_each_safe(pos, n, &sg_buff_wrappers_head) {
                temp_sg_buff_wrapper =
                    list_entry(pos, CRONO_SG_BUFFER_INFO_WRAPPER, ntrn.list);
                // Check if the buffer is allocated from the underlying
                // process
                if (temp_sg_buff_wrapper->ntrn.app_pid == app_pid) {
                        no_wrappers_found = false;
                        _crono_release_buff_wrapper(temp_sg_buff_wrapper);
                        crono_kvfree(temp_sg_buff_wrapper);
                }
        }

        // Contiguous Buffer Wrappers
        list_for_each_safe(pos, n, &contig_buff_wrappers_head) {
                temp_contig_buff_wrapper = list_entry(
                    pos, CRONO_CONTIG_BUFFER_INFO_WRAPPER, ntrn.list);
                // Check if the buffer is allocated from the underlying
                // process
                if (temp_contig_buff_wrapper->ntrn.app_pid == app_pid) {
                        no_wrappers_found = false;
                        _crono_release_buff_wrapper(temp_contig_buff_wrapper);
                        crono_kvfree(temp_contig_buff_wrapper);
                }
        }

        if (no_wrappers_found) {
                pr_debug("No buffer wrappers found");
        }
        pr_info("Done cleanup process PID <%d> buffer wrappers", app_pid);

        return CRONO_SUCCESS;
}

static int _crono_apply_cleanup_commands(struct inode *miscdev_inode) {
        int ret = CRONO_SUCCESS, bar, icmd;
        unsigned long bar_base, bar_len;
        uint8_t __iomem *hwmem = NULL; // Memory pointer for the I/O operations
        struct crono_miscdev *crono_dev = NULL;

        // Validate parameters
        LOGERR_RET_EINVAL_IF_NULL(miscdev_inode, "Invalid miscdev_inode value");

        // Get `crono_dev` of the passed inode
        if (CRONO_SUCCESS != (ret = _crono_get_crono_dev_from_inode(
                                  miscdev_inode, &crono_dev))) {
                pr_err("Can't find internal information about device "
                       "of inode: "
                       "minor <%d>",
                       iminor(miscdev_inode));
                return ret;
        }

        // Validate device has cleanup commands
        pr_debug("Applying cleanup commands: device <%s>, commands "
                 "count <%d>...",
                 crono_dev->miscdev.name, crono_dev->cmds_count);
        if (0 == crono_dev->cmds_count) {
                pr_debug("No cleanup commands are found for device <%s>",
                         crono_dev->miscdev.name);
                return CRONO_SUCCESS;
        }

        // Get BAR memory information, to write cleanup commands on its
        // regiters
        bar_base = pci_resource_start(crono_dev->dev, DEVICE_BAR_INDEX);
        if (0 == bar_base) {
                pr_err("Error getting start address of BAR <%d> of device <%s>",
                       DEVICE_BAR_INDEX, crono_dev->miscdev.name);
                return -EFAULT;
        }
        bar_len = pci_resource_len(crono_dev->dev, DEVICE_BAR_INDEX);
        pr_debug("BAR <%d> of device <%s>: Base <0x%lx>, Length <%ld>",
                 DEVICE_BAR_INDEX, crono_dev->miscdev.name, bar_base, bar_len);

        // Request the BAR (I/O resource)
        bar = pci_select_bars(crono_dev->dev, IORESOURCE_MEM);

        // pci_enable_device_mem(pdev);
        // Request the memory region
        ret = pci_request_region(crono_dev->dev, bar, "crono_pci_drvmod");
        if (ret != 0) {
                pr_err("Error requesting device <%s> region",
                       crono_dev->miscdev.name);
                return ret;
        }

        // Map provided resource to the local memory pointer
        hwmem = ioremap(bar_base, bar_len);
        if (NULL == hwmem) {
                pr_err("Error mapping BAR <%d> memory", DEVICE_BAR_INDEX);
                pci_release_region(crono_dev->dev, bar);
        }
        pr_debug("BAR <%d> memory is mapped to <0x%p>", DEVICE_BAR_INDEX,
                 hwmem);

        // Write the commands to the registers
        for (icmd = 0; icmd < crono_dev->cmds_count; icmd++) {
                iowrite32(crono_dev->cmds[icmd].data,
                          hwmem + crono_dev->cmds[icmd].addr);
        }

#ifdef DEBUG
        // Log outside the registers writing loop
        for (icmd = 0; icmd < crono_dev->cmds_count; icmd++) {
                pr_debug("Applied cleanup command: data<0x%x>, "
                         "offset<0x%x>",
                         crono_dev->cmds[icmd].data,
                         crono_dev->cmds[icmd].addr);
        }
#endif
        // Cleanup function data and actions
        pci_release_region(crono_dev->dev, bar);
        pr_debug("Done applying cleanup commands of device <%s>",
                 crono_dev->miscdev.name);

        // Return
        return ret;
}

/**
 * @brief
 * Allocate and fill `pp_buff_wrapper` object.
 * Allocate memory as per `arg.size` (CRONO_CONTIG_BUFFER_INFO*).
 * Caller needs to `copy_to_user` to `arg` the buffer info
 * (pp_buff_wrapper.buff_info).
 * `pp_buff_wrapper` should be freed using 'crono_kvfree' to clean memory .
 *
 * @param filp
 * @param arg
 * `CRONO_CONTIG_BUFFER_INFO*`, to copy the buffer info from and address to.
 * `.addr` has the user space memory address.
 * @param pp_buff_wrapper
 * @return int
 */
static int _crono_init_contig_buff_wrapper(
    struct file *filp, unsigned long arg,
    CRONO_CONTIG_BUFFER_INFO_WRAPPER **pp_buff_wrapper) {

        int ret = CRONO_SUCCESS;
        CRONO_CONTIG_BUFFER_INFO_WRAPPER *buff_wrapper =
            NULL; // To simplify pointer-to-pointer

        if (0 == arg) {
                pr_err("Invalid parameter `arg` initializing buffer "
                       "wrapper");
                return -EINVAL;
        }

        // Allocate and initialize `buff_wrapper`
        // Should be freed using 'crono_kvfree', e.g. after any call to
        // `_crono_release_buffxyz` family.
        *pp_buff_wrapper = buff_wrapper =
            kmalloc(sizeof(CRONO_CONTIG_BUFFER_INFO_WRAPPER), GFP_KERNEL);
        if (NULL == buff_wrapper) {
                pr_err("Error allocating DMA internal struct");
                return -ENOMEM;
        }
        buff_wrapper->ntrn.bwt = BWT_CONTIG;
        buff_wrapper->ntrn.app_pid = task_pid_nr(current);

        // Lock the memory from user space to kernel space to get the needed
        // memory size
        if (copy_from_user(&(buff_wrapper->buff_info), (void __user *)arg,
                           sizeof(CRONO_CONTIG_BUFFER_INFO))) {
                pr_err("Error copying user data");
                ret = -EFAULT;
                goto func_err;
        }

        // Get device pointer in internal structure
        ret = _crono_get_dev_from_filp(filp, &(buff_wrapper->ntrn.devp));
        if (ret != CRONO_SUCCESS) {
                pr_err("Error getting dev");
                ret = -EIO;
                goto func_err;
        }

        ret = dma_set_mask_and_coherent(&buff_wrapper->ntrn.devp->dev,
                                        DMA_BIT_MASK(32));
        if (ret) {
                pr_err("Error setting mask: %d", ret);
                ret = -EIO;
                goto func_err;
        }

        // Allocate contiguous memory in kernel space
        pr_debug("Allocating contiguous buffer of size <%ld>",
                 buff_wrapper->buff_info.size);
        buff_wrapper->buff_info.addr = dma_alloc_coherent(
            &(buff_wrapper->ntrn.devp->dev), buff_wrapper->buff_info.size,
            &(buff_wrapper->dma_handle), GFP_KERNEL);
        buff_wrapper->buff_info.dma_handle = buff_wrapper->dma_handle;
        if (buff_wrapper->buff_info.addr == NULL) {
                // Just null, no global error setting, check `dmsg` if you
                // need any details
                pr_err("Error allocating memory of size: %zu, check dmsg",
                       buff_wrapper->buff_info.size);
                ret = -ENOMEM; // Or appropriate error
                goto func_err;
        }
        pr_debug("Allocated buffer address: <0x%p>, handle: <%llu>",
                 buff_wrapper->buff_info.addr, buff_wrapper->dma_handle);
        // Sample: Allocated buffer address: <0x00000000b065d96f>, handle:
        // <0x117000000>

        // Since the buffer is kernel-space address returned by
        // dma_alloc_coherent. copy_to_user takes care of copying the data from
        // the kernel space to the user space safely, dealing with the user
        // space memory as needed (including any necessary fault handling).
        // Caller to call `copy_to_user` for `buff_info` (including `.addr`
        // set).

        // Add the buffer to list
        buff_wrapper->buff_info.id = contig_buff_wrappers_new_id;
        list_add(&(buff_wrapper->ntrn.list), &contig_buff_wrappers_head);
        pr_debug("Added contiguous buffer wrapper to internal list. "
                 "Address <%p>, size <%ld>, id <%d>",
                 buff_wrapper->buff_info.addr, buff_wrapper->buff_info.size,
                 buff_wrapper->buff_info.id);
        contig_buff_wrappers_new_id++;
        _crono_debug_list_wrappers();

        return ret;

func_err:
        crono_kvfree(buff_wrapper);
        return ret;
}

static int _crono_miscdev_ioctl_lock_contig_buffer(struct file *filp,
                                                   unsigned long arg) {
        int ret;
        CRONO_CONTIG_BUFFER_INFO_WRAPPER *bw = NULL;

        pr_debug("Locking contiguous buffer...");

        // Validate, initialize, and lock variables
        if (CRONO_SUCCESS !=
            (ret = _crono_init_contig_buff_wrapper(filp, arg, &bw))) {
                return ret;
        }

        // Copy back all data to userspace memory, including address
        // of the allocated memory
        if (copy_to_user((void __user *)arg, &(bw->buff_info),
                         sizeof(CRONO_CONTIG_BUFFER_INFO))) {
                pr_err("Error copying buffer information back to user space");
                ret = -EFAULT;
                goto lock_err;
        }

        // Cleanup
        pr_debug("Done locking contiguous buffer");
        return CRONO_SUCCESS;

lock_err:
        _crono_release_buff_wrapper(bw);
        crono_kvfree(bw);
        return ret;
}

static int _crono_miscdev_ioctl_unlock_contig_buffer(struct file *filp,
                                                     unsigned long arg) {
        int ret = CRONO_SUCCESS;
        int wrapper_id = -1;
        CRONO_CONTIG_BUFFER_INFO_WRAPPER *found_buff_wrapper = NULL;

        // Lock the memory from user space to kernel space
        if (0 == arg) {
                pr_err("Invalid parameter `arg` unlocking buffer");
                return -EINVAL;
        }
        if (copy_from_user(&wrapper_id, (void __user *)arg, sizeof(int))) {
                pr_err("Error copying user data");
                return -EFAULT;
        }
        pr_debug("Unlocking buffer of wrapper id <%d>...", wrapper_id);

        if (CRONO_SUCCESS != get_bw(wrapper_id, &found_buff_wrapper)) {
                pr_err("Buffer wrapper <%d> is not found", wrapper_id);
                return -EINVAL;
        }

        // Clean up buffer memory allocated in the kernel module
        ret = _crono_release_buff_wrapper(found_buff_wrapper);

        // Free the wrapper after all members cleanup is done
        kvfree(found_buff_wrapper);

        // Copy back just to obey DMA APIs rules
        if (copy_to_user((void __user *)arg, &wrapper_id, sizeof(int))) {
                ret = -EFAULT;
        }

        return ret;
}

static int crono_mmap_contig(struct file *file, struct vm_area_struct *vma) {
        // `mmap` `offset` (last) argument should be aligned on a page boundary,
        // so the buffer id is sent to `mmap` multiplied by PAGE_SIZE, however,
        // it's recieved here divided by PATE_SIZE already
        int bw_id = vma->vm_pgoff;
        int ret = CRONO_SUCCESS;
        phys_addr_t virttophys;
        CRONO_CONTIG_BUFFER_INFO_WRAPPER *found_buff_wrapper = NULL;

        pr_debug("Mapping Buffer Wrapper <%d>, offset: <%lu>", bw_id,
                 vma->vm_pgoff);

        if (CRONO_SUCCESS != get_bw(bw_id, &found_buff_wrapper)) {
                pr_err("Buffer wrapper <%d> is not found", bw_id);
                return -EINVAL;
        }

        virttophys = virt_to_phys(found_buff_wrapper->buff_info.addr);
        pr_debug("virt_to_phys from 0x%p to 0x%llx",
                 found_buff_wrapper->buff_info.addr, virttophys);

        // we are using pgoff as a buffer index only
        vma->vm_pgoff = 0;
        ret = remap_pfn_range(vma, vma->vm_start, virttophys >> PAGE_SHIFT,
                              found_buff_wrapper->buff_info.size,
                              vma->vm_page_prot);

        pr_debug("Mapping Buffer Wrapper <%d> returned code <%d>", bw_id, ret);
        return ret;
}

static int get_bw(int bw_id, CRONO_CONTIG_BUFFER_INFO_WRAPPER **ppBW) {
        int ret = CRONO_SUCCESS;
        CRONO_CONTIG_BUFFER_INFO_WRAPPER *found_buff_wrapper = NULL;
        CRONO_CONTIG_BUFFER_INFO_WRAPPER *temp_buff_wrapper = NULL;
        struct list_head *pos, *n;

        // Find the related buffer_wrapper in the list
        _crono_debug_list_wrappers();
        list_for_each_safe(pos, n, &contig_buff_wrappers_head) {
                temp_buff_wrapper = list_entry(
                    pos, CRONO_CONTIG_BUFFER_INFO_WRAPPER, ntrn.list);
                if (temp_buff_wrapper->buff_info.id == bw_id)
                        found_buff_wrapper = temp_buff_wrapper;
        }
        if (NULL == found_buff_wrapper) {
                // Not found, just display a warning but operation is
                // considered successful
                pr_warn("Buffer Wrapper of id <%d> is not found in "
                        "internal list",
                        bw_id);

                // Returning error will cause any coming opn to fail returning
                // error EFAULT 14
                // Case might happen when closing after multiple opens
                return CRONO_SUCCESS;
        } else {
                pr_debug("Found wrapper of id <%d> in the internal list",
                         found_buff_wrapper->buff_info.id);
        }
        *ppBW = found_buff_wrapper;
        return ret;
}
