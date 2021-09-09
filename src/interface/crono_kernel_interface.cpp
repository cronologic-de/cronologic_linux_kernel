#include "crono_kernel_interface.h"
#include "crono_kernel_interface_internal.h"

/**
 * Defines `pDevice` and `pDev_handle`, and initialize them from `hDev`
 * Returns `CRONO_KERNEL_STATUS_INVALID_CRONO_KERNEL_HANDLE` if hDev is NULL
*/
#define CRONO_INIT_HDEV_FUNC(hDev)                  \
            PCRONO_KERNEL_DEVICE pDevice;           \
            CRONO_RET_ERR_CODE_IF_NULL(hDev, CRONO_KERNEL_STATUS_INVALID_CRONO_KERNEL_HANDLE);      \
            pDevice = (PCRONO_KERNEL_DEVICE)hDev;   \
            if (0 == pDevice->id.pciId.dwDeviceId){ \
                return CRONO_KERNEL_STATUS_INVALID_CRONO_KERNEL_HANDLE; }

/**
 * Validates that both `dwOffset` and `val` size are within the memory range.
 * Returns 'CRONO_KERNEL_INSUFFICIENT_RESOURCES' if not.
 * Is part of the functions in this file, a prerequisite to have all used variables
 * defined and initialized in the functions.
d*/
#define CRONO_VALIDATE_MEM_RANGE                                    \
    if ( (dwOffset + sizeof(val)) > pDevice->bar_addr.dwSize)  {    \
        return CRONO_KERNEL_INSUFFICIENT_RESOURCES ;    }           

// pci_system_linux_sysfs_create in 
// https://gitlab.freedesktop.org/xorg/lib/libpciaccess/-/blob/master/src/linux_sysfs.c
DWORD CRONO_KERNEL_PciScanDevices(DWORD dwVendorId, DWORD
    dwDeviceId, CRONO_KERNEL_PCI_SCAN_RESULT *pPciScanResult)
{
    struct stat st;
    DIR* dr = NULL;
    struct dirent* en;
    unsigned domain, bus, dev, func ;
    uint16_t vendor_id, device_id; 
    int index_in_result = 0 ;

    if (stat(SYS_BUS_PCIDEVS_PATH, &st) != 0) 
    {
        printf("Error %d: PCI FS is not found.\n", errno) ;
        return errno ;
    }
    if (((st.st_mode) & S_IFMT) != S_IFDIR)
    {
        printf("Error: PCI FS is not a directory.\n") ;
        return errno ;
    }
    dr = opendir(SYS_BUS_PCIDEVS_PATH); //open all or present directory
    if (!dr)
    {
        printf("Error %d: opening PCI directory.\n", errno) ;
        return errno ;
    }
    while ((en = readdir(dr)) != NULL)
    {
        if (en->d_type != DT_LNK)
        {
            continue;
        }

        // Get PCI Domain, Bus, Device, and Function 
        sscanf(en->d_name, "%04x:%02x:%02x.%1u", &domain, &bus, &dev, &func);
        
        // Get Vendor ID and Device ID
        crono_read_vendor_device(domain, bus, dev, func, (unsigned int*)&vendor_id, (unsigned int*)&device_id) ;

        // Check values & fill pPciScanResult if device matches the Vendor/Device
        if (    ((vendor_id == dwVendorId) || (((DWORD)PCI_ANY_ID) == dwVendorId))
            &&  ((device_id == dwDeviceId) || (((DWORD)PCI_ANY_ID) == dwDeviceId))
           )
        {
            pPciScanResult->deviceId[index_in_result].dwDeviceId = device_id ;
            pPciScanResult->deviceId[index_in_result].dwVendorId = vendor_id;
            pPciScanResult->deviceSlot[index_in_result].dwDomain = domain ;
            pPciScanResult->deviceSlot[index_in_result].dwBus = bus ;
            pPciScanResult->deviceSlot[index_in_result].dwSlot = dev ;
            pPciScanResult->deviceSlot[index_in_result].dwFunction = func ;
            index_in_result ++ ;
            pPciScanResult->dwNumDevices = index_in_result ;
        }
    }

    // Clean up
    closedir(dr); 

    // Successfully scanned
    return CRONO_KERNEL_STATUS_SUCCESS;
}

/**
 * Deprecated
*/
CRONO_KERNEL_API DWORD CRONO_KERNEL_PciGetDeviceInfo(
    CRONO_KERNEL_PCI_CARD_INFO *pDeviceInfo)
{
    return CRONO_KERNEL_STATUS_SUCCESS;
}

CRONO_KERNEL_API DWORD CRONO_KERNEL_PciDeviceOpen(CRONO_KERNEL_DEVICE_HANDLE *phDev,
    const CRONO_KERNEL_PCI_CARD_INFO *pDeviceInfo, const PVOID pDevCtx,
    PVOID reserved, const CHAR *pcKPDriverName, PVOID pKPOpenData)
{
    DIR* dr = NULL;
    struct dirent* en;
    unsigned domain, bus, dev, func ;
    int err ;

    // Init variables and validate parameters
    CRONO_RET_INV_PARAM_IF_NULL(phDev);
    CRONO_RET_INV_PARAM_IF_NULL(pDeviceInfo) ;
    *phDev = NULL;

    dr = opendir(SYS_BUS_PCIDEVS_PATH); //open all or present directory
    if (!dr)
    {
        printf("Error %d: opening PCI directory.\n", errno) ;
        return errno ;
    }
    while ((en = readdir(dr)) != NULL)
    {
        if (en->d_type != DT_LNK)
        {
            continue;
        }

        // Get PCI Domain, Bus, Device, and Function 
        sscanf(en->d_name, "%04x:%02x:%02x.%1u", &domain, &bus, &dev, &func);
 
        // Check values & fill pDevice if device matches the Vendor/Device
        if (    (pDeviceInfo->pciSlot.dwDomain == domain) 
            &&  (pDeviceInfo->pciSlot.dwBus == bus)
            &&  (pDeviceInfo->pciSlot.dwSlot == dev)
            &&  (pDeviceInfo->pciSlot.dwFunction == func)
           )
        {
            // Allocate `pDevice` memory
            PCRONO_KERNEL_DEVICE pDevice;
            pDevice = (PCRONO_KERNEL_DEVICE)malloc(sizeof(CRONO_KERNEL_DEVICE));
            memset(pDevice, 0, sizeof(CRONO_KERNEL_DEVICE));

            // Set `pDevice` slot information
            pDevice->slot.pciSlot.dwDomain = pDeviceInfo->pciSlot.dwDomain ;
            pDevice->slot.pciSlot.dwBus = pDeviceInfo->pciSlot.dwBus ;
            pDevice->slot.pciSlot.dwSlot = pDeviceInfo->pciSlot.dwSlot ;
            pDevice->slot.pciSlot.dwFunction = pDeviceInfo->pciSlot.dwFunction ;

            // Get device `Vendor ID` and `Device ID` and set them to `pDevice`
            unsigned int vendor_id, device_id;
            crono_read_vendor_device(domain, bus, dev, func, &vendor_id, &device_id) ;
            pDevice->id.pciId.dwDeviceId = device_id ;
            pDevice->id.pciId.dwVendorId = vendor_id ;

            // Set bar_addr
            // Map BAR0 full memory starting @ offset 0 to bar_addr
            void* BAR_base_mem_address ;
            pciaddr_t dwSize = 0;
            if (CRONO_SUCCESS != (err = crono_get_BAR0_mem_addr(domain, bus, dev, func, 
                    0, &dwSize, &BAR_base_mem_address, NULL)) )
            {
                goto device_error ;
            }
            pDevice->bar_addr.pUserDirectMemAddr = (UPTR)BAR_base_mem_address ;
            pDevice->bar_addr.dwSize = dwSize ;

            // Get the device `miscdev` file name, and set it to `pDevice`
            struct crono_dev_DBDF dbdf = {domain, bus, dev, func} ;
            CRONO_CONSTRUCT_MISCDEV_NAME(pDevice->miscdev_name, device_id, dbdf);
            struct stat miscdev_stat;
            char miscdev_path[PATH_MAX] ;
            sprintf(miscdev_path, "/dev/%s", pDevice->miscdev_name);
            if (stat(miscdev_path, &miscdev_stat) != 0)
            {
                printf("Error: miscdev `%s` is not found.\n", miscdev_path) ;
                err = -EINVAL;
                goto device_error ;
            }

            // Set phDev
            *phDev = pDevice;
            break ;
        }
    }

    // Clean up
    closedir(dr); 

    // Successfully scanned
    return CRONO_SUCCESS;

device_error:
    if (dr) {
        closedir(dr); 
    }
    return err;
}

CRONO_KERNEL_API DWORD CRONO_KERNEL_PciDeviceClose(
    CRONO_KERNEL_DEVICE_HANDLE hDev)
{
    // Init variables and validate parameters
    CRONO_INIT_HDEV_FUNC(hDev);

    // Free memory allocated in CRONO_KERNEL_PciDeviceOpen
    free(pDevice);

    return CRONO_KERNEL_STATUS_SUCCESS;
}

CRONO_KERNEL_API DWORD CRONO_KERNEL_PciReadCfg32(
    CRONO_KERNEL_DEVICE_HANDLE hDev, DWORD dwOffset, UINT32 *val)
{
    pciaddr_t bytes_read ;
    int err = CRONO_SUCCESS;
    pciaddr_t config_space_size = 0 ;

    // Init variables and validate parameters
    CRONO_INIT_HDEV_FUNC(hDev);
    CRONO_RET_INV_PARAM_IF_NULL(val) ;

    // Validate offset and data size are within configuration spcae size 
    err = crono_get_config_space_size(pDevice->slot.pciSlot.dwDomain, pDevice->slot.pciSlot.dwBus,
        pDevice->slot.pciSlot.dwSlot, pDevice->slot.pciSlot.dwFunction, &config_space_size);
    if (CRONO_SUCCESS != err)
    {
        return err ;
    }
    if (config_space_size < (dwOffset + sizeof(*val))) 
    {
        return CRONO_KERNEL_INVALID_PARAMETER ;
    }

    // Read the configurtion
    err = crono_read_config(pDevice->slot.pciSlot.dwDomain, pDevice->slot.pciSlot.dwBus, 
        pDevice->slot.pciSlot.dwSlot, pDevice->slot.pciSlot.dwFunction, 
        val, dwOffset, sizeof(*val), &bytes_read) ;
    if ((bytes_read != 4) || err) 
    {
        return err ;
    }

    // Success
    return CRONO_KERNEL_STATUS_SUCCESS;
}

CRONO_KERNEL_API DWORD CRONO_KERNEL_PciWriteCfg32(
    CRONO_KERNEL_DEVICE_HANDLE hDev, DWORD dwOffset, UINT32 val)
{
    pciaddr_t bytes_written ;
    int err = CRONO_SUCCESS;
    pciaddr_t config_space_size = 0 ;

    // Init variables and validate parameters
    CRONO_INIT_HDEV_FUNC(hDev);

    // Validate offset and data size are within configuration spcae size 
    err = crono_get_config_space_size(pDevice->slot.pciSlot.dwDomain, 
            pDevice->slot.pciSlot.dwBus, pDevice->slot.pciSlot.dwSlot, 
            pDevice->slot.pciSlot.dwFunction, &config_space_size);
    if (CRONO_SUCCESS != err)
    {
        return err ;
    }
    if (config_space_size < (dwOffset + sizeof(val))) 
    {
        return CRONO_KERNEL_INVALID_PARAMETER ;
    }

    // Write the configuration
    err = crono_write_config(pDevice->slot.pciSlot.dwDomain, 
            pDevice->slot.pciSlot.dwBus, pDevice->slot.pciSlot.dwSlot,
            pDevice->slot.pciSlot.dwFunction, &val, dwOffset, sizeof(val), &bytes_written) ;
    if ((bytes_written != 4) || err) 
    {
        return err ;
    }

    // Success
    return CRONO_KERNEL_STATUS_SUCCESS;
}

CRONO_KERNEL_API DWORD CRONO_KERNEL_ReadAddr8(
    CRONO_KERNEL_DEVICE_HANDLE hDev, DWORD dwAddrSpace, KPTR dwOffset, BYTE *val)
{
    // Init variables and validate parameters
    CRONO_INIT_HDEV_FUNC(hDev);
    CRONO_RET_ERR_CODE_IF_NULL(val, CRONO_KERNEL_INSUFFICIENT_RESOURCES);
    CRONO_VALIDATE_MEM_RANGE ;

    // Copy memory
    *val = *((volatile BYTE*)(((BYTE*)(pDevice->bar_addr.pUserDirectMemAddr)) + dwOffset));

    // Success
    return CRONO_KERNEL_STATUS_SUCCESS;
}

CRONO_KERNEL_API DWORD CRONO_KERNEL_ReadAddr16(
    CRONO_KERNEL_DEVICE_HANDLE hDev, DWORD dwAddrSpace, KPTR dwOffset, WORD *val)
{
    // Init variables and validate parameters
    CRONO_INIT_HDEV_FUNC(hDev);
    CRONO_RET_ERR_CODE_IF_NULL(val, CRONO_KERNEL_INSUFFICIENT_RESOURCES);
    CRONO_VALIDATE_MEM_RANGE ;

    // Copy memory
    *val = *((volatile WORD*)(((BYTE*)(pDevice->bar_addr.pUserDirectMemAddr)) + dwOffset));

    // Success
    return CRONO_KERNEL_STATUS_SUCCESS; 
}

CRONO_KERNEL_API DWORD CRONO_KERNEL_ReadAddr32(
    CRONO_KERNEL_DEVICE_HANDLE hDev, DWORD dwAddrSpace, KPTR dwOffset, UINT32 *val)
{
    // Init variables and validate parameters
    CRONO_INIT_HDEV_FUNC(hDev);
    CRONO_RET_ERR_CODE_IF_NULL(val, CRONO_KERNEL_INSUFFICIENT_RESOURCES);
    CRONO_VALIDATE_MEM_RANGE ;

    // Type casting to both DWORD or UINT32 works successfully
    *val = *((volatile UINT32*)(((BYTE*)(pDevice->bar_addr.pUserDirectMemAddr)) + dwOffset));

    // Success
    return CRONO_KERNEL_STATUS_SUCCESS;
}

CRONO_KERNEL_API DWORD CRONO_KERNEL_ReadAddr64(
    CRONO_KERNEL_DEVICE_HANDLE hDev, DWORD dwAddrSpace, KPTR dwOffset, UINT64 *val)
{
    // Init variables and validate parameters
    CRONO_INIT_HDEV_FUNC(hDev);
    CRONO_RET_ERR_CODE_IF_NULL(val, CRONO_KERNEL_INSUFFICIENT_RESOURCES);
    CRONO_VALIDATE_MEM_RANGE ;

    // Copy memory
    *val = *((volatile UINT64*)(((BYTE*)(pDevice->bar_addr.pUserDirectMemAddr)) + dwOffset));

    // Success
    return CRONO_KERNEL_STATUS_SUCCESS;
}

CRONO_KERNEL_API DWORD CRONO_KERNEL_WriteAddr8(
    CRONO_KERNEL_DEVICE_HANDLE hDev, DWORD dwAddrSpace, KPTR dwOffset, BYTE val)
{
    // Init variables and validate parameters
    CRONO_INIT_HDEV_FUNC(hDev);
    CRONO_VALIDATE_MEM_RANGE ;

    // Copy memory
    *((volatile BYTE*)(((BYTE*)(pDevice->bar_addr.pUserDirectMemAddr)) + dwOffset)) = val;

    // Success
    return CRONO_KERNEL_STATUS_SUCCESS;
}

CRONO_KERNEL_API DWORD CRONO_KERNEL_WriteAddr16(
    CRONO_KERNEL_DEVICE_HANDLE hDev, DWORD dwAddrSpace, KPTR dwOffset, WORD val)
{
    // Init variables and validate parameters
    CRONO_INIT_HDEV_FUNC(hDev);
    CRONO_VALIDATE_MEM_RANGE ;

    // Copy memory
    *((volatile WORD*)(((BYTE*)(pDevice->bar_addr.pUserDirectMemAddr)) + dwOffset)) = val;

    // Success
    return CRONO_KERNEL_STATUS_SUCCESS;
}

CRONO_KERNEL_API DWORD CRONO_KERNEL_WriteAddr32(
    CRONO_KERNEL_DEVICE_HANDLE hDev, DWORD dwAddrSpace, KPTR dwOffset, UINT32 val)
{
    // Init variables and validate parameters
    CRONO_INIT_HDEV_FUNC(hDev);
    CRONO_VALIDATE_MEM_RANGE ;

    // Copy memory
    // Type cast DWORD, as casting to UINT32 reads wrong values
    *((volatile UINT32*)(((BYTE*)(pDevice->bar_addr.pUserDirectMemAddr)) + dwOffset)) = val;

    // Success
    return CRONO_KERNEL_STATUS_SUCCESS;
}

CRONO_KERNEL_API DWORD CRONO_KERNEL_WriteAddr64(
    CRONO_KERNEL_DEVICE_HANDLE hDev, DWORD dwAddrSpace, KPTR dwOffset, UINT64 val)
{
    // Init variables and validate parameters
    CRONO_INIT_HDEV_FUNC(hDev);
    CRONO_VALIDATE_MEM_RANGE ;

    // Copy memory
    *((volatile UINT64*)(((BYTE*)(pDevice->bar_addr.pUserDirectMemAddr)) + dwOffset)) = val;

    // Success
    return CRONO_KERNEL_STATUS_SUCCESS;
}

CRONO_KERNEL_API DWORD CRONO_KERNEL_GetBarPointer(
        CRONO_KERNEL_DEVICE_HANDLE hDev, DWORD* barPointer)
{
    // Init variables and validate parameters
    CRONO_INIT_HDEV_FUNC(hDev);
    CRONO_RET_INV_PARAM_IF_NULL(barPointer);

    barPointer = (DWORD*)(pDevice->bar_addr.pUserDirectMemAddr) ;

    // Success
    return CRONO_KERNEL_STATUS_SUCCESS;
}

CRONO_KERNEL_API DWORD CRONO_KERNEL_DriverOpen(
        CRONO_KERNEL_DRV_OPEN_OPTIONS openOptions)
{
    return CRONO_KERNEL_STATUS_SUCCESS;
}        

CRONO_KERNEL_API DWORD CRONO_KERNEL_DriverClose()
{
    return CRONO_KERNEL_STATUS_SUCCESS;
}

CRONO_KERNEL_API PVOID CRONO_KERNEL_GetDevContext(
        CRONO_KERNEL_DEVICE_HANDLE hDev)
{
    // Init variables and validate parameters
    PCRONO_KERNEL_DEVICE pDevice;           \
    if(NULL == hDev)
        return NULL ;
    pDevice = (PCRONO_KERNEL_DEVICE)hDev;   
    return pDevice->pCtx ;
}

// Needed code for Windows compatibility
const char * Stat2Str(DWORD dwStatus)
{
    int size = 256;
    char* message = (char*)malloc(sizeof(char)*size);

    if (message != nullptr)
    {
        switch (dwStatus)
        {
/*      $$ definitions not found
		case ERROR_IO_PENDING:
			sprintf_s(message, sizeof(char)*size, "997 (0x3E5) - Overlapped I / O operation is in progress.");
			break;
		case ERROR_ACCESS_DENIED:
			sprintf_s(message, sizeof(char)*size, "5 (0x5) - Access is denied.");
			break;
		case ERROR_NOACCESS:
			sprintf_s(message, sizeof(char)*size, "998 (0x3E6) - Invalid access to memory location.");
			break;
*/      default:
            sprintf_s(message, sizeof(char)*size, "%ld - see windows error codes.", dwStatus);
            break;
        }
    }
    return message;
}

CRONO_KERNEL_API DWORD CRONO_KERNEL_CardCleanupSetup(CRONO_KERNEL_DEVICE_HANDLE hDev, CRONO_KERNEL_TRANSFER *Cmd,
    DWORD dwCmds, BOOL bForceCleanup)
{
    // $$ not implemented
    // Init variables and validate parameters
    CRONO_INIT_HDEV_FUNC(hDev);

    printf("CRONO_KERNEL_CardCleanupSetup is not implemented yet.\n") ;
    return CRONO_KERNEL_STATUS_SUCCESS;
}

CRONO_KERNEL_API BOOL CRONO_KERNEL_AddrSpaceIsActive(CRONO_KERNEL_DEVICE_HANDLE hDev,
    DWORD dwAddrSpace)
{
    // $$ not implemented
    printf("CRONO_KERNEL_AddrSpaceIsActive is not implemented yet.\n") ;
    return CRONO_KERNEL_STATUS_SUCCESS;
}

CRONO_KERNEL_API DWORD CRONO_KERNEL_DMASGBufLock(CRONO_KERNEL_DEVICE_HANDLE hDev, PVOID pBuf,
    DWORD dwOptions, DWORD dwDMABufSize, CRONO_KERNEL_DMA **ppDma)
{
    int ret = CRONO_KERNEL_STATUS_SUCCESS ;
    DMASGBufLock_parameters params ;
    int miscdev_fd = 0 ;
    char miscdev_path[PATH_MAX] ;
    CRONO_KERNEL_DMA *dmaInfo ;

    // ______________________________________
    // Init variables and validate parameters
    //
    CRONO_INIT_HDEV_FUNC(hDev);
    CRONO_RET_INV_PARAM_IF_NULL(pBuf);
    CRONO_RET_INV_PARAM_IF_NULL(ppDma);
    CRONO_RET_INV_PARAM_IF_ZERO(dwDMABufSize);

    // Construct DMASGBufLock_parameters
    params.pBuf = pBuf ;
    params.dwDMABufSize = dwDMABufSize ;
    params.dwOptions = dwOptions ;
    params.vmas = NULL ;
    
    // Allocate the DMA Pages Memory
    dmaInfo = (CRONO_KERNEL_DMA*)malloc(sizeof(CRONO_KERNEL_DMA));
    if ( NULL == dmaInfo)
    {
        printf("Error allocating DMA struct memory");
        return -ENOMEM;
    }
    memset(dmaInfo, 0, sizeof(CRONO_KERNEL_DMA));
#define DIV_ROUND_UP(n, d)   (((n) + (d) - 1) / (d))
    dmaInfo->dwPages = DIV_ROUND_UP(dwDMABufSize, PAGE_SIZE) ;
    dmaInfo->Page = (CRONO_KERNEL_DMA_PAGE*)
        malloc(sizeof(CRONO_KERNEL_DMA_PAGE) * dmaInfo->dwPages);
    if ( NULL == dmaInfo->Page)
    {
        printf("Error allocating Page memory");
        free(dmaInfo) ;
        return -ENOMEM;
    }
    CRONO_DEBUG("Allocated `ppDma[0]->Page` of size <%ld>\n", 
        sizeof(CRONO_KERNEL_DMA_PAGE) * dmaInfo->dwPages);             
    *ppDma = dmaInfo ;      // Do not ppDma = &dmaInfo
    params.ppDma = ppDma ;

    // ________________
    // Open device file
    //
    // Get device file name & open it
    sprintf(miscdev_path, "/dev/%s", pDevice->miscdev_name);
    miscdev_fd = open(miscdev_path, O_RDWR);
    if(miscdev_fd < 0) {
        printf("Error %d: cannot open device file <%s>...\n", errno, miscdev_path);
        free(dmaInfo->Page) ;
        free(dmaInfo) ;
        return errno;
    }    
    
    // ___________
    // Lock Buffer
    // 
    ret = ioctl(miscdev_fd, IOCTL_CRONO_LOCK_BUFFER, &params) ;
    if ( CRONO_SUCCESS != ret)
    {
        printf("Driver module error %d\n", ret) ;
        goto alloc_err ;
    }

#ifdef CRONO_DEBUG_ENABLED
    for (int ipage = 0 ; ipage < (dmaInfo->dwPages < 5 ? dmaInfo->dwPages : 5) ; ipage++ )
    {
        fprintf(stdout, "Buffer Page <%d> Physical Address is <%p>\n", 
            ipage, (void*)(dmaInfo->Page[ipage].pPhysicalAddr));
    }
#endif 

    // _______
    // Cleanup
    //
    // Close device file
    close(miscdev_fd) ;
    
    return ret;

alloc_err:
    if (miscdev_fd >0)
        close(miscdev_fd) ;
    
    return ret ;
}

CRONO_KERNEL_API DWORD CRONO_KERNEL_DMABufUnlock(CRONO_KERNEL_DEVICE_HANDLE hDev,
    CRONO_KERNEL_DMA *pDma)
{
    int ret = CRONO_KERNEL_STATUS_SUCCESS ;
    char miscdev_path[PATH_MAX] ;
    DMASGBufLock_parameters params ;
    int miscdev_fd ;

    // ______________________________________
    // Init variables and validate parameters
    //
    CRONO_INIT_HDEV_FUNC(hDev);
    CRONO_RET_INV_PARAM_IF_NULL(pDma);
    params.ppDma = NULL ;
    miscdev_fd = 0 ;
    params.ppDma = (CRONO_KERNEL_DMA**)malloc(sizeof(CRONO_KERNEL_DMA**)) ;
    params.ppDma[0] = pDma ;
    params.vmas = NULL ;

    // ________________
    // Open device file
    //
    // Get device file name & open it
    sprintf(miscdev_path, "/dev/%s", pDevice->miscdev_name);
    miscdev_fd = open(miscdev_path, O_RDWR);
    if(miscdev_fd < 0) {
        printf("Error %d: cannot open device file...\n", errno);
        return errno;
    }    

    // _____________
    // Unlock Buffer
    // 
    // Call ioctl() to unlock the buffer and cleanup
    if ( CRONO_SUCCESS != ioctl(miscdev_fd, IOCTL_CRONO_UNLOCK_BUFFER, &params))
    {
        goto ioctl_err ;
    }

    // _______
    // Cleanup
    //
    // Free memory
    if ( NULL != params.ppDma[0]->Page)
    {
        free(params.ppDma[0]->Page) ;
        params.ppDma[0]->Page = NULL ;
    }
    if ( NULL != params.ppDma)
    {
        free(params.ppDma) ;
    }

    // Close device file
    close(miscdev_fd) ;

    return ret;

ioctl_err:
    if (NULL != params.ppDma) 
        free(params.ppDma) ;
    if (miscdev_fd > 0)
        close(miscdev_fd) ;
    
    return ret ;
}

#include <sys/sysinfo.h> 
void printFreeMemInfoDebug(const char* msg) 
{ 
#ifdef CRONO_DEBUG_ENABLED
    struct sysinfo info; 
    sysinfo(&info); 
    printf("%s: %ld in bytes / %ld in KB / %ld in MB / %ld in GB\n", 
        msg, info.freeram, info.freeram/1024, 
        (info.freeram/1024)/1024, ((info.freeram/1024)/1024)/1024); 
#endif
}
