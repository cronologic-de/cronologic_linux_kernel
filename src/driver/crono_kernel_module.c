#include "crono_miscdevice.h"
#include "kernel_module.h"
#include "crono_ioctl.h"
// _____________________________________________________________________________
// Globals
//
MODULE_DESCRIPTION("cronologic PCI driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.9");

// `CRONO_KERNEL_MODE` must be defined for historical reasons.
// #define CRONO_KERNEL_MODE, no need to define it here as it's passed in Makefile

/**
 * The device type entry in the array, has the index = Device ID. e.g. device 
 * CRONO_DEVICE_XTDC4 has entry at index 6 in the array. 
 * Size has an additional `1`, as Device Type ID starts at `1` 
 * (Device Type of ID=0 is undefined).
 */
static struct crono_device_type 
    crono_device_types_info[CRONO_DEVICE_DEV_ID_MAX_COUNT+1] ;

static const struct pci_device_id crono_pci_device_ids [] = { 
	// Get all devices of cronologic Vendor ID
    {
	.vendor = CRONO_VENDOR_ID,
	.device = PCI_ANY_ID,
	.subvendor = PCI_ANY_ID,
	.subdevice = PCI_ANY_ID,
	},
	{ /* end: all zeroes */ }
}; 
MODULE_DEVICE_TABLE (pci, crono_pci_device_ids);

static struct pci_driver crono_pci_driver = {
	.name = CRONO_PCI_DRIVER_NAME,
	.id_table = crono_pci_device_ids,
	.probe = crono_driver_probe,
};

// Miscellaneous Device Driver
static int Device_Open = 0;  // Is device open?  Used to prevent multiple

static struct file_operations crono_miscdev_fops = {
    .owner = THIS_MODULE,
    
    .open = crono_miscdev_open ,
    .release = crono_miscdev_release ,

    .unlocked_ioctl = crono_miscdev_ioctl,
};

// _____________________________________________________________________________
// init & exit
//
static int __init crono_driver_init(void)
{
    int registration_ret ;
    crono_pr_info("Registering Driver...");
    
    // Reset memory
    memset(crono_device_types_info, 0, sizeof(struct crono_device_type) * CRONO_DEVICE_DEV_ID_MAX_COUNT);

    registration_ret = pci_register_driver(&crono_pci_driver);
    if (registration_ret)
    {
        crono_pr_info("Error Registering PCI Driver, <%d>!!!", registration_ret);
    }
    crono_pr_info("Done");	

    return CRONO_SUCCESS;
}
module_init(crono_driver_init);

/*
** Module exit function
*/
static void __exit crono_driver_exit(void)
{
    int dt_index = -1 ;
    struct crono_device_type* dev_type = NULL ;

    // Unregister all miscdevs 
    // Loop on the device types have registerd devices
    iterate_active_device_types ;
    _crono_miscdev_type_exit(dev_type);
    end_iterate_active_device_types ;

    // Unregister the driver
    crono_pr_info("Removing Driver...");
    pci_unregister_driver(&crono_pci_driver);
    crono_pr_info("Done");
}

module_exit(crono_driver_exit);

// Probe
static int crono_driver_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
    int enable_ret ;
    int register_ret ;

	// Check the device to claim if concerned
    crono_pr_info("Probe Device, ID = <0x%02X>", dev->device);
    if (id->vendor != CRONO_VENDOR_ID)
        return -1 ;
    if ((dev->device > CRONO_DEVICE_DEV_ID_MAX_COUNT) || (dev->device < 0))
    {
		crono_pr_err("Error Device ID <0x%02x> not supported", dev->device);
		return -1 ;
    }

	// Enable the PCIe device
    enable_ret = pci_enable_device(dev) ;
    if (enable_ret < 0)
    {
        crono_pr_err("Error enabling the device in probe");
        return enable_ret ;
    }

	// Register a miscdev for this device
    crono_device_types_info[dev->device].device_id = dev->device ;
    register_ret = _crono_miscdev_type_init(&crono_device_types_info[dev->device], dev);
    if (register_ret) {
		goto error_miscdev ;
    }
	return CRONO_SUCCESS ;

error_miscdev:
    pci_disable_device(dev) ;
    return register_ret;
}

// _____________________________________________________________________________
// Miscellaneous Device Driver
static int _crono_miscdev_type_init(
                struct crono_device_type* dev_type, struct pci_dev *dev)
{
    char miscdev_name[CRONO_MAX_DEV_NAME_SIZE] ;
    int register_ret ;
    struct miscdevice *new_crono_miscdev_dev;

    // Validations
    if (dev_type == NULL)
    {
        crono_pr_err("_crono_miscdev_type_init Invalid Arguments");
        return -1 ;
    }
    if (dev_type->bound_drivers_count == CRONO_KERNEL_PCI_CARDS)
    {
        crono_pr_err("miscdev reached maximum count for type of id <%d>", dev_type->device_id);
        return -2 ;
    }

    // Generate the device name
    // `dev_type->bound_drivers_count` is the index of the new element
    new_crono_miscdev_dev = &(dev_type->miscdevs[dev_type->bound_drivers_count]);
    new_crono_miscdev_dev->minor = MISC_DYNAMIC_MINOR ;
    new_crono_miscdev_dev->fops = &crono_miscdev_fops ;
    new_crono_miscdev_dev->name = miscdev_name ;

    _crono_get_DBDF_from_dev(dev, &(dev_type->dev_DBDFs[dev_type->bound_drivers_count])) ;
    _crono_pr_info_DBDF(&(dev_type->dev_DBDFs[dev_type->bound_drivers_count]));

    CRONO_CONSTRUCT_MISCDEV_NAME(miscdev_name, dev_type->device_id, 
        dev_type->dev_DBDFs[dev_type->bound_drivers_count]) ;

    new_crono_miscdev_dev->name = miscdev_name ;
    crono_pr_info("Initializing cronologic miscdev driver: <%s>...", miscdev_name);

    // Register the device driver
    register_ret = misc_register(new_crono_miscdev_dev);
    if (register_ret) {
        crono_pr_err("Can't register misdev: <%s>, error: <%d>", miscdev_name, register_ret);
        return register_ret ;
    }

    // Set the driver internal information
    strcpy(dev_type->dev_names[dev_type->bound_drivers_count], miscdev_name) ;
    dev_type->miscdevs[dev_type->bound_drivers_count].name = 
        dev_type->dev_names[dev_type->bound_drivers_count] ;
    dev_type->devs[dev_type->bound_drivers_count] = dev ;

    dev_type->bound_drivers_count ++ ;

    // Log and return
    crono_pr_info("Done with minor: <%d>", new_crono_miscdev_dev->minor);
    return CRONO_SUCCESS ;
}

static void _crono_miscdev_type_exit(struct crono_device_type* dev_type)
{
    int device_index ;
    // Loop on the registered devices of the this device type.
    for (device_index = 0; device_index < dev_type->bound_drivers_count; device_index ++)
    {
        crono_pr_info("Exiting cronologic miscdev driver: <%s>, minor: <%d>...", 
                dev_type->miscdevs[device_index].name, 
                dev_type->miscdevs[device_index].minor);
        misc_deregister(&(dev_type->miscdevs[device_index]));
        crono_pr_info("Done");  // Needed to log it didn't crash
    }
}

static long crono_miscdev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    int ret = CRONO_SUCCESS;

    switch(cmd) {
        case IOCTL_CRONO_LOCK_BUFFER:
            ret = _crono_miscdev_ioctl_lock_buffer(filp, arg) ;
            break ;
        case IOCTL_CRONO_UNLOCK_BUFFER:
            ret = _crono_miscdev_ioctl_unlock_buffer(filp, arg) ;
            break ;
		default:
            BUG_ON(1);
			crono_pr_err("Error, unsupported ioctl command <%d>", cmd);
            ret = -EFAULT;
			break;
    }
    return ret ; 
}

static int _crono_miscdev_ioctl_lock_buffer(struct file *filp, unsigned long arg)
{
    int ret ;
    DMASGBufLock_parameters params;
    CRONO_KERNEL_DMA* pDma = NULL; // Temp for internal handling
    CRONO_KERNEL_DMA_PAGE *Page_orig = NULL; // Keep it till after the `copy_to_user`   
    CRONO_KERNEL_DMA_PAGE *ppage = NULL ;
    CRONO_KERNEL_DMA** ppDma_orig;
    size_t pages_size = 0;
    struct pci_dev* devp ;
#ifdef CRONO_DEBUG_ENABLED
    int ipage, loop_count ;
#endif
    // ___________________
    // Set DMA Mask
    // Since crono devices can all handle full 64 bit address as DMA source and 
    // destination, we need to set 64-bit mask to avoid using `swiotlb` by linux
    // when calling `dma_map_sg`.
    //
    ret = _crono_get_dev_from_filp(filp, &devp);
    if (ret != CRONO_SUCCESS)
    {
        if (-ENODATA == ret)
        {
            crono_pr_err("Error getting device information from file descriptor");
        }
        return ret ;
    }
    ret = pci_set_dma_mask(devp, DMA_BIT_MASK(64));
    if (ret != CRONO_SUCCESS)
    {
        crono_pr_err("Device cannot perform DMA properly on this platform, error <%d>", ret);
        return ret ;
    }

    // ________________________________________
    // Lock and copy data from userspace memory
    // 
    IOCTL_DECLARE_VALIDATE_LOCK_PARAMS(lock_err) ;
    IOCTL_COPY_KERNEL_DMA_FROM_USER(lock_err) ;
    IOCTL_COPY_KERNEL_PAGES_FROM_USER(lock_err) ;
    IOCTL_COPY_CRONO_PAGES_FROM_USER(lock_err) ;
    CRONO_KERNEL_DEBUG("Allocated `ppage` of size <%ld>\n", pages_size);

    // ___________________
    // Validate parameters
    //
    LOGERR_RET_EINVAL_IF_NULL(params.pBuf, "Invalid buffer to be locked");
 	if (params.dwDMABufSize > ULONG_MAX)
    {
        crono_pr_err("Size <%ld> of requested buffer to lock exceeds the maximum size allowed", 
            params.dwDMABufSize);

        IOCTL_CLEAN_KERNEL_DMA(lock_err) ;
        IOCTL_CLEAN_KERNEL_PAGES(lock_err) ;
		
        return -EINVAL;
    }

    crono_pr_info("Locking buffer of size = <%ld>, address = <%p>",
        ((DMASGBufLock_parameters*)arg)->dwDMABufSize, ((DMASGBufLock_parameters*)arg)->pBuf) ;

    // Pin the buffer, allocate and fill `params.kernel_pages`.
    ret = _crono_miscdev_ioctl_pin_buffer(filp, &params, GUP_NR_PER_CALL) ;
    if ( CRONO_SUCCESS != ret ) 
    {
        IOCTL_CLEAN_CRONO_PAGES(lock_err) ;
        IOCTL_CLEAN_KERNEL_PAGES(lock_err) ;
        IOCTL_CLEAN_KERNEL_DMA(lock_err) ;
        return -EFAULT ;
    }

    // Fill the Scatter/Gather list
    ret = _crono_miscdev_ioctl_generate_sg(filp, &params) ;
    if ( CRONO_SUCCESS != ret ) 
    {
        IOCTL_CLEAN_CRONO_PAGES(lock_err) ;
        IOCTL_CLEAN_KERNEL_PAGES(lock_err) ;
        IOCTL_CLEAN_KERNEL_DMA(lock_err) ;
        return -EFAULT ;
    }

    // Copy back all data to userspace memory
    IOCTL_DONE_WITH_LOCK_PARAMS(lock_err) ;
    IOCTL_COPY_KERNEL_DMA_TO_USER(lock_err) ;
    IOCTL_COPY_KERNEL_PAGES_TO_USER(lock_err) ;
    IOCTL_COPY_CRONO_PAGES_TO_USER(lock_err) ;

#ifdef CRONO_DEBUG_ENABLED
    // Log only first 5 pages
    // `IOCTL_DONE_WITH_LOCK_PARAMS` should have copied the values of `pPhysicalAddr`
    // in `arg`.
    loop_count = ((DMASGBufLock_parameters*)arg)->ppDma[0]->dwPages < 5 ? 
        ((DMASGBufLock_parameters*)arg)->ppDma[0]->dwPages : 5 ;
    for (ipage = 0 ; ipage < loop_count ; ipage++ )
    {
        CRONO_KERNEL_DEBUG("ioctl_lock: Userpsace Buffer Page <%d> Physical Address is <%p>\n", 
            ipage, (void*)(((DMASGBufLock_parameters*)arg)->ppDma[0]->Page[ipage].pPhysicalAddr));
    }
#endif 

	return CRONO_SUCCESS;

lock_err:
    return ret ;
}

static int _crono_miscdev_ioctl_pin_buffer(
    struct file* filp, DMASGBufLock_parameters *params, unsigned long nr_per_call)
{
    unsigned long start_addr_to_pin ;   // Start address in pBuf to be pinned by `pin_user_pages`.
    unsigned long next_pages_addr ;     // Next address in pBuf to be pinned by `pin_user_pages`.
    int           buffer_pages_nr ;     // `params` buffer number of pages, calculated based on its size.
    long          actual_pinned_nr_of_call ;    // Never unsigned, as it might contain error returned
    int           ret = CRONO_SUCCESS ;
    int           page_index ;

    // Validate parameters
    LOGERR_RET_EINVAL_IF_NULL(params, "Invalid lock buffer parameters");
    LOGERR_RET_EINVAL_IF_NULL(params->ppDma[0], "Invalid dma struct parameters");

    // Allocate struct pages pointer array memory to be filled by `pin_user_pages`
    // params->kernel_pages contains virtual address, however, you may DMA to/from
    // that memory using the addresses returned from it  
    buffer_pages_nr = DIV_ROUND_UP(params->dwDMABufSize, PAGE_SIZE);
    crono_pr_info("Allocating kernel pages. Buffer size = <%ld>, pages number = <%d>", 
        params->dwDMABufSize, buffer_pages_nr) ;
    // `kvmalloc_array` may return memory that is not physically contiguous.
    params->ppDma[0]->kernel_pages = kvmalloc_array(    
        buffer_pages_nr, sizeof(struct page*), GFP_KERNEL);
    LOGERR_RET_ERRNO_IF_NULL(params->ppDma[0]->kernel_pages, 
        "Error allocating pages memory", -ENOMEM);
    CRONO_KERNEL_DEBUG("Allocated `kernel_pages` of size <%ld>\n", buffer_pages_nr * sizeof(void*));
    
    // Pin buffer blocks, each of size = (nr_per_call * PAGE_SIZE) in every iteration
    // to its corresponding page address in params->ppDma[0]->kernel_pages
	for (	params->ppDma[0]->pinned_pages_nr = 0, start_addr_to_pin = (__u64)params->pBuf; 
			start_addr_to_pin < (__u64)params->pBuf + params->dwDMABufSize; 
			start_addr_to_pin = next_pages_addr	) 
	{
		// Next pages chunk starts after `nr_per_call` pages bytes 
        next_pages_addr = start_addr_to_pin + nr_per_call * PAGE_SIZE;
		if (next_pages_addr > ((__u64)params->pBuf + params->dwDMABufSize)) 
		// Exceeded the buffer size 
		{
			next_pages_addr = (__u64)params->pBuf + params->dwDMABufSize;
			nr_per_call = (next_pages_addr - start_addr_to_pin) / PAGE_SIZE;
		}
		actual_pinned_nr_of_call = pin_user_pages(start_addr_to_pin, nr_per_call, FOLL_WRITE,  
				(struct page**)(params->ppDma[0]->kernel_pages) + params->ppDma[0]->pinned_pages_nr, 
				params->vmas == NULL ? NULL : params->vmas + params->ppDma[0]->pinned_pages_nr);
        
		if (actual_pinned_nr_of_call < 0)
		{
            // Error 
            crono_pr_err("Error pinning user pages: <%ld>", actual_pinned_nr_of_call) ;
            ret = actual_pinned_nr_of_call ;
			break ;
		}
		if (actual_pinned_nr_of_call == 0)
		{
            // Success, end of mapping
            CRONO_KERNEL_DEBUG("ioctl_pin: Successful end of pinning");
			break ;
		}

        CRONO_KERNEL_DEBUG("Pin Iteration: Count <%ld>, 1st kernel_pages <%p>, last kernel_page <%p>",
            actual_pinned_nr_of_call ,
            (params->ppDma[0]->kernel_pages + params->ppDma[0]->pinned_pages_nr)[0], 
            (params->ppDma[0]->kernel_pages + params->ppDma[0]->pinned_pages_nr)[nr_per_call - 1]);

		params->ppDma[0]->pinned_pages_nr += actual_pinned_nr_of_call;
	}

    // Validate the number of pinned pages 
    if ( params->ppDma[0]->pinned_pages_nr < buffer_pages_nr )
    {
        // Apparently not enough memory to pin the whole buffer
        crono_pr_err("Error insufficient available pages to pin");
        _crono_miscdev_ioctl_cleanup_buffer(filp, params) ;
        if (CRONO_SUCCESS == ret) return -EFAULT ; else return ret;
    }

    // Fill in the ppDma Page Array
    for ( page_index = 0 ; page_index < buffer_pages_nr ; page_index++ )
    {
        params->ppDma[0]->Page[page_index].pPhysicalAddr = 
            PFN_PHYS(page_to_pfn((struct page*)params->ppDma[0]->kernel_pages[page_index])) ;
        params->ppDma[0]->Page[page_index].dwBytes = PAGE_SIZE ; 
        
        if (page_index < 5)
        // Log only first 5 pages
            CRONO_KERNEL_DEBUG("ioctl_pin: Kernel Page <%d> is of physical address <%p>", 
                page_index, (void*)params->ppDma[0]->Page[page_index].pPhysicalAddr); 
    }

    // Set the actual buffer size, and actual count of pages pinned
    params->ppDma[0]->pUserAddr = params->pBuf ;
	params->ppDma[0]->dwPages = params->ppDma[0]->pinned_pages_nr ;   // Do we really need them to be the same?
	params->ppDma[0]->dwBytes = start_addr_to_pin - (__u64)params->pBuf;

    crono_pr_info("Successfully Pinned buffer: size = <%ld>, number of pages = <%ld>",
        params->ppDma[0]->dwBytes, params->ppDma[0]->dwPages) ;

	return ret;
}

static int _crono_miscdev_ioctl_unlock_buffer(struct file *filp, unsigned long arg)
{
    int ret ;
    DMASGBufLock_parameters params;
    CRONO_KERNEL_DMA* pDma = NULL; // Temp for internal handling
    CRONO_KERNEL_DMA_PAGE *Page_orig = NULL; // Keep it till after the `copy_to_user`   
    CRONO_KERNEL_DMA** ppDma_orig;

    IOCTL_DECLARE_VALIDATE_LOCK_PARAMS(unlock_err) ;
    IOCTL_COPY_KERNEL_PAGES_FROM_USER(unlock_err) ;
    IOCTL_COPY_KERNEL_DMA_FROM_USER(unlock_err) ;

    _crono_miscdev_ioctl_cleanup_buffer(filp, &params);

    IOCTL_COPY_KERNEL_DMA_TO_USER(unlock_err) ;
    IOCTL_COPY_KERNEL_PAGES_TO_USER(unlock_err) ;
    IOCTL_DONE_WITH_LOCK_PARAMS(unlock_err) ;

    return CRONO_SUCCESS;

unlock_err :
    return ret ;
}

static int _crono_miscdev_ioctl_generate_sg(struct file* filp, DMASGBufLock_parameters* params)
{
    struct pci_dev* devp ;
    int ret;  
    struct sg_table* sgt = NULL ;
    int mapped_buffers_count = 0 ;
#ifdef USE__sg_alloc_table_from_pages    
    int     page_index ;
    struct  scatterlist *sg ;
    struct  scatterlist *sg_from_pages = NULL ;
    DWORD   dw_mapped_pages_size = 0 ;
#endif

    // Validate parameters
	LOGERR_RET_EINVAL_IF_NULL(params->ppDma[0]->kernel_pages, "Invalid pages to get addresses for");
    if(params->ppDma[0]->sgt != NULL)
    {
        crono_pr_err("Invalid DMA SG address, already allocated");
        return -EFAULT ;
    }
    
    // Allocate sgt, as `allocate_sg_table` does not allocate it.
    sgt = params->ppDma[0]->sgt = (struct sg_table*)kvmalloc(sizeof(struct sg_table), GFP_KERNEL);
    if (NULL == sgt) 
    { 
        crono_pr_err("Error allocating memory"); 
        return -ENOMEM; 
    }
    memset(sgt, 0, sizeof(struct sg_table));

    // Get the corresponding device structure to be used in `dma_map_sg`
    ret = _crono_get_dev_from_filp(filp, &devp);
    if (ret != CRONO_SUCCESS)
    {
        if (-ENODATA == ret)
        {
            crono_pr_err("Error getting device information from file descriptor");
        }
        // Clean up
        crono_kvfree(sgt) ;
    
        // Other errors occurred, probably invalid parameters, `_crono_get_dev_from_filp`
        // should have displayed the corresponding error message.
        return ret ;
    }

    // Allocate sg_table to hold the scatter/gather segments array. 
    // With scatter lists, we map a region gathered from several regions
    // If passed_pages_count > SG_MAX_SINGLE_ALLOC (much smaller than 4096), 
    // then a chained sg table will be setup
    crono_pr_info("Allocating SG Table for buffer size = <%ld>, number of pages = <%ld> ...",
        params->ppDma[0]->dwBytes, params->ppDma[0]->dwPages);
#ifndef USE__sg_alloc_table_from_pages    
    ret = sg_alloc_table_from_pages(
            sgt,                            // The sg table header to use 
            (struct page **)params->ppDma[0]->kernel_pages, // Pointer to an array of page pointers      
            params->ppDma[0]->dwPages,      // Number of pages in the pages array
            0,                              // Offset from start of the first page to the start of a buffer
            params->ppDma[0]->dwBytes,      // Number of valid bytes in the buffer (after offset) 
            GFP_KERNEL);
    if (ret)
#else
    // If the physical address got by `PFN_PHYS`, didn't work, then, 
    // `__sg_alloc_table_from_pages` should be called with `PAGE_SIZE` parameter, 
    // then `sg_dma_address` should be used to fill in the pages addresses, however, 
    // it's not guaranteed that the buffers coming out of `dma_map_sg` are 1-PAGE-ONLY 
    // SIZE.
    sg_from_pages = __sg_alloc_table_from_pages(
            sgt,                            // The sg table header to use 
            (struct page **)params->ppDma[0]->kernel_pages, // Pointer to an array of page pointers      
            params->ppDma[0]->dwPages,      // Number of pages in the pages array
            0,                              // Offset from start of the first page to the start of a buffer
            params->ppDma[0]->dwBytes,      // Number of valid bytes in the buffer (after offset) 
            PAGE_SIZE,
            NULL, 
            0, 
            GFP_KERNEL);
    if (PTR_ERR_OR_ZERO(sg_from_pages))              
#endif 
    {
        crono_pr_err("Error allocating SG table from pages");

        // Clean up
        crono_kvfree(sgt) ;

        return ret ;
    }
    crono_pr_info("Done");

#ifdef USE__sg_alloc_table_from_pages    
    // Using `__sg_alloc_table_from_pages` results in `nents` number equal to
    // Pages Number; while using `sg_alloc_table_from_pages` will most probably 
    // result in a different number of `nents`. 
    if (sgt->nents != params->ppDma[0]->dwPages)
    {
        crono_pr_err("Inconsistent SG table elements number with number of pages");
        return -EFAULT ;
    }
#endif 

    crono_pr_info("Mapping SG") ;
    mapped_buffers_count = dma_map_sg(&devp->dev, sgt->sgl, sgt->nents, DMA_BIDIRECTIONAL) ;
    // `ret` is the number of DMA buffers to transfer. `dma_map_sg` coalesces 
    // buffers that are adjacent to each other in memory, so `ret` may be 
    // less than nents.
    if (mapped_buffers_count < 0)
    {
        crono_pr_err("Error mapping SG: <%d>", ret);
        
        // Clean up
        sg_free_table(sgt); // Free sgl, even if it's chained
        crono_kvfree(sgt) ;

        return ret ;
    }

    CRONO_KERNEL_DEBUG("SG Table is allocated of scatter lists total nents number = <%d>"
                       ", Mapped buffers count <%d>", sgt->nents, mapped_buffers_count) ;

    // Success
    return CRONO_SUCCESS;
}

static int _crono_miscdev_ioctl_cleanup_buffer(
        struct file* filp, DMASGBufLock_parameters* params)
{
    if (    NULL != params 
        &&  NULL != params->ppDma[0] 
        &&  NULL != params->ppDma[0]->kernel_pages  )
    {
        struct pci_dev* devp ;
        int ret ;

        // Unpin pages
        CRONO_KERNEL_DEBUG("Unpinning pages of address = <%p>, and number = <%ld>", 
            params->ppDma[0]->kernel_pages, params->ppDma[0]->pinned_pages_nr);
        unpin_user_pages((struct page**)(params->ppDma[0]->kernel_pages), 
            params->ppDma[0]->pinned_pages_nr);

        ret = _crono_get_dev_from_filp(filp, &devp);
        if (ret != CRONO_SUCCESS)
        {
            crono_pr_err("Error getting device information from file descriptor: <%d>", ret);
        }

        dma_unmap_sg(&devp->dev, ((struct sg_table*)params->ppDma[0]->sgt)->sgl, 
            ((struct sg_table*)params->ppDma[0]->sgt)->nents, DMA_BIDIRECTIONAL);

        // Clean allocated memory
        if( NULL != params->ppDma[0]->kernel_pages ) 
        {
            crono_kvfree(params->ppDma[0]->kernel_pages);
        }
        if (NULL != params->ppDma[0]->sgt)
        {
            sg_free_table(params->ppDma[0]->sgt);
            crono_kvfree(params->ppDma[0]->sgt) ;
            params->ppDma[0]->sgt = NULL ;
        }
    }
    return CRONO_SUCCESS;
}

// _____________________________________________________________________________
// Methods
//
/* Called when a process tries to open the device file, like
 * "cat /dev/mycharfile"
 */ 
static int crono_miscdev_open(struct inode *inode, struct file *file)
{
    static int counter = 0;
    if (Device_Open) 
    {
        return -EBUSY;
    }

    Device_Open ++;
    crono_pr_info("Driver open() is called <%d> times", counter++);
    __module_get(THIS_MODULE);

   return CRONO_SUCCESS;
}


/* Called when a process closes the device file */
static int crono_miscdev_release(struct inode *inode, struct file *file)
{
    Device_Open --;     /* We're now ready for our next caller */

    // Decrement the usage count, or else once you opened the file, you'll
    //  never get get rid of the module.
    module_put(THIS_MODULE);
    crono_pr_info("Driver release() is called");

    return CRONO_SUCCESS;
}

// _____________________________________________________________________________

static int _crono_get_DBDF_from_dev(struct pci_dev *dev, struct crono_dev_DBDF* dbdf)
{
    if (NULL == dev)
        return -EINVAL;
    if (NULL == dbdf)
        return -EINVAL;

    // Reset all to zeros
    memset(dbdf, 0, sizeof(struct crono_dev_DBDF));

    if (dev->bus != NULL)
    {
        dbdf->bus = dev->bus->number;
        if (dev->bus->parent != NULL)
        {
            dbdf->domain = dev->bus->parent->number;
        }
    }
    dbdf->dev = PCI_SLOT(dev->devfn);
    dbdf->func = PCI_FUNC(dev->devfn);

    // Success
    return CRONO_SUCCESS;
}

static int _crono_pr_info_DBDF(struct crono_dev_DBDF *dbdf) 
{
    if (NULL == dbdf)
        return -EINVAL;

    crono_pr_info("Device BDBF: <%04X:%02X:%02X.%01X>",
        dbdf->domain, dbdf->bus, dbdf->dev, dbdf->func);

    return CRONO_SUCCESS;
}

static int _crono_get_dev_from_filp(struct file* filp, struct pci_dev** devpp) 
{
    unsigned int file_drv_minor ;
    int dt_index = -1 ;
    struct crono_device_type* dev_type = NULL ;
    int dev_index = -1 ;

    // Validate parameters
    LOGERR_RET_EINVAL_IF_NULL(filp, "Invalid file to get dev for");
    LOGERR_RET_EINVAL_IF_NULL(devpp, "Invalid device pointer");

    // Get the file descriptor minor to match it with the device
    file_drv_minor = iminor(filp->f_path.dentry->d_inode);

    // Loop on the registered devices of every device type
    iterate_devs_of_active_device_types ;
    if (dev_type->miscdevs[dev_index].minor != file_drv_minor) 
        continue ; 
    // Found a device with the same minor of the file
    *devpp = dev_type->devs[dev_index] ;
    return CRONO_SUCCESS;
    end_iterate_devs_of_active_device_types ;

    // Corresponding device is not found
    return -ENODATA;
}
