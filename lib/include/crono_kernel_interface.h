/*++

Module Name:

    public.h

Abstract:

    This module contains the common declarations shared by driver
    and user applications.

Environment:

    user and kernel

--*/

#ifndef __CRONO_KERNEL_INTERFACE_H__
#define __CRONO_KERNEL_INTERFACE_H__ (1)

#if defined(__cplusplus)
	extern "C" {
#endif

#ifdef _WIN32
    #ifdef CRONO_KERNEL_DRIVER_EXPORTS
        #define CRONO_KERNEL_API __declspec(dllexport)
    #else
        #ifndef CRONO_KERNEL_DRIVER_STATIC
            #define CRONO_KERNEL_API __declspec(dllimport)
        #else
            #define CRONO_KERNEL_API
        #endif
    #endif
#elif __linux__
    #ifndef CRONO_KERNEL_MODE
        #include <stdint.h> 
        #include <string.h>    
        #define CRONO_KERNEL_API 
    #else
        #define CRONO_KERNEL_API export
    #endif 
    #include <stddef.h>
    #include "crono_driver.h"
    #define PCI_ANY_ID (~0)
    #define CRONO_SUCCESS   0
    #define CRONO_VENDOR_ID 0x1A13
    #ifndef FALSE
        #define FALSE   (0)
    #endif
    #ifndef TRUE
        #define TRUE    (!FALSE)
    #endif
#else
    #define CRONO_KERNEL_API 
#endif


#ifndef CRONO_KERNEL_MODE
    #ifdef _WIN32
        #include <windows.h>
    //	#include <winioctl.h>
    #elif  __linux__
        typedef int BOOL, *PBOOL, *LPBOOL;
        typedef char CHAR, *PCHAR;
        typedef void *PVOID;
        typedef unsigned char BYTE, *PBYTE, *LPBYTE;
        typedef unsigned long DWORD, *PDWORD, *LPDWORD;
        typedef uint64_t UINT64;
        typedef uint32_t UINT32;
        typedef unsigned short WORD, *PWORD, *LPWORD;
        typedef void* HANDLE ;
        typedef uint64_t pciaddr_t;
    #else // #ifndef CRONO_KERNEL_MODE
        /* extracted from kpstdlib.h */
        #if defined(WINNT) || defined(WINCE) || defined(WIN32)
            #if defined(_WIN64) && !defined(KERNEL_64BIT)
            #define KERNEL_64BIT
            #endif
            typedef unsigned long ULONG;
            typedef unsigned short USHORT;
            typedef unsigned char UCHAR;
            typedef long LONG;
            typedef short SHORT;
            typedef char CHAR;
            typedef ULONG DWORD;
            typedef DWORD *PDWORD;
            typedef unsigned char *PBYTE;
            typedef USHORT WORD;
            typedef void *PVOID;
            typedef char *PCHAR;
            typedef PVOID HANDLE;
            typedef ULONG BOOL;
            #ifndef WINAPI
                #define WINAPI
            #endif
        #elif defined(UNIX)
            #ifndef __cdecl
                #define __cdecl
            #endif
        #endif
            /* end kplstdlib.h */
    #endif
#endif
		
/*
	for use with CRONO_KERNEL_DMAContigBufLock
	implementation was extracted from
	http://stackoverflow.com/questions/29938573/accessing-kernel-memory-from-user-mode-windows
	and from
	http://www.osronline.com/showthread.cfm?link=251845
*/
/*typedef struct _MEMORY_ENTRY
{
	PVOID       pBuffer;
	DWORD		dmaBufSize;
} MEMORY_ENTRY, *PMEMORY_ENTRY*/;


//
// Define an Interface Guid so that app can find the device and talk to it.
//

#ifdef _WIN32
// crono_kernel0101
// {7246d083 - e458 - 4d01 - 8a7b - cb5d6ae839e8}
DEFINE_GUID(GUID_CRONO_KERNEL,
	0x7246d083, 0xe458, 0x4d01, 0x8a, 0x7b, 0xcb, 0x5d, 0x6a, 0xe8, 0x39, 0xe8);

// crono_kernel
// {cb67d8b4-bb8a-4a4f-af85-06fc9230bf09}
DEFINE_GUID(GUID_CRONO_KERNEL_old,
	0xcb67d8b4, 0xbb8a, 0x4a4f, 0xaf, 0x85, 0x06, 0xfc, 0x92, 0x30, 0xbf, 0x09);
#endif


/*  extracted from windrvr.h */ 
#if defined(WIN32)
    #define DLLCALLCONV __stdcall
#else
    #define DLLCALLCONV
#endif


/*
 * The KPTR is guaranteed to be the same size as a kernel-mode pointer
 * The UPTR is guaranteed to be the same size as a user-mode pointer
 */
#if defined(KERNEL_64BIT)
    typedef UINT64 KPTR;
#else
    typedef DWORD KPTR;
#endif

#if defined(__KERNEL_)
    typedef KPTR UPTR;
#else
    typedef size_t UPTR;
#endif

typedef UINT64 DMA_ADDR;
typedef KPTR PHYS_ADDR;

// IN WD_TRANSFER_CMD and WD_Transfer() DWORD stands for 32 bits and QWORD is 64
// bit.
typedef enum
{
    CMD_NONE = 0,       // No command
    CMD_END = 1,        // End command
    CMD_MASK = 2,       // Interrupt Mask

    RP_BYTE = 10,       // Read port byte
    RP_WORD = 11,       // Read port word
    RP_DWORD = 12,      // Read port dword
    WP_BYTE = 13,       // Write port byte
    WP_WORD = 14,       // Write port word
    WP_DWORD = 15,      // Write port dword
    RP_QWORD = 16,      // Read port qword
    WP_QWORD = 17,      // Write port qword

    RP_SBYTE = 20,      // Read port string byte
    RP_SWORD = 21,      // Read port string word
    RP_SDWORD = 22,     // Read port string dword
    WP_SBYTE = 23,      // Write port string byte
    WP_SWORD = 24,      // Write port string word
    WP_SDWORD = 25,     // Write port string dword
    RP_SQWORD = 26,     // Read port string qword
    WP_SQWORD = 27,     // Write port string qword

    RM_BYTE = 30,       // Read memory byte
    RM_WORD = 31,       // Read memory word
    RM_DWORD = 32,      // Read memory dword
    WM_BYTE = 33,       // Write memory byte
    WM_WORD = 34,       // Write memory word
    WM_DWORD = 35,      // Write memory dword
    RM_QWORD = 36,      // Read memory qword
    WM_QWORD = 37,      // Write memory qword

    RM_SBYTE = 40,      // Read memory string byte
    RM_SWORD = 41,      // Read memory string word
    RM_SDWORD = 42,     // Read memory string dword
    WM_SBYTE = 43,      // Write memory string byte
    WM_SWORD = 44,      // Write memory string word
    WM_SDWORD = 45,     // Write memory string dword
    RM_SQWORD = 46,     // Read memory string quad word
    WM_SQWORD = 47      // Write memory string quad word
} CRONO_KERNEL_TRANSFER_CMD;

enum { CRONO_KERNEL_DMA_PAGES = 256 };

#ifdef __linux__
// Use structure definitions in "crono_driver.h"
#else 
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
    CRONO_KERNEL_DMA_PAGE Page[CRONO_KERNEL_DMA_PAGES];
} CRONO_KERNEL_DMA;
#endif // #ifdef __linux__

enum {
    DMA_KERNEL_BUFFER_ALLOC = 0x1, // The system allocates a contiguous buffer.
        // The user does not need to supply linear address.

    DMA_KBUF_BELOW_16M = 0x2, // If DMA_KERNEL_BUFFER_ALLOC is used,
        // this will make sure it is under 16M.

    DMA_LARGE_BUFFER = 0x4, // If DMA_LARGE_BUFFER is used,
        // the maximum number of pages are dwPages, and not
        // WD_DMA_PAGES. If you lock a user buffer (not a kernel
        // allocated buffer) that is larger than 1MB, then use this
        // option and allocate memory for pages.

    DMA_ALLOW_CACHE = 0x8,  // Allow caching of contiguous memory.

    DMA_KERNEL_ONLY_MAP = 0x10, // Only map to kernel, dont map to user-mode.
        // relevant with DMA_KERNEL_BUFFER_ALLOC flag only

    DMA_FROM_DEVICE = 0x20, // memory pages are locked to be written by device

    DMA_TO_DEVICE = 0x40, // memory pages are locked to be read by device

    DMA_TO_FROM_DEVICE = (DMA_FROM_DEVICE | DMA_TO_DEVICE), // memory pages are
        // locked for both read and write

    DMA_ALLOW_64BIT_ADDRESS = 0x80, // Use this value for devices that support
                                    // 64-bit DMA addressing.

    DMA_ALLOW_NO_HCARD = 0x100, // allow memory lock without hCard
};

/* Macros for backward compatibility */
#define DMA_READ_FROM_DEVICE DMA_FROM_DEVICE
#define DMA_WRITE_TO_DEVICE DMA_TO_DEVICE

typedef struct
{
    KPTR  pPort;       // I/O port for transfer or kernel memory address.
    DWORD cmdTrans;    // Transfer command WD_TRANSFER_CMD.

    // Parameters used for string transfers:
    DWORD dwBytes;     // For string transfer.
    DWORD fAutoinc;    // Transfer from one port/address
                       // or use incremental range of addresses.
    DWORD dwOptions;   // Must be 0.
    union
    {
        BYTE Byte;     // Use for 8 bit transfer.
        WORD Word;     // Use for 16 bit transfer.
        UINT32 Dword;  // Use for 32 bit transfer.
        UINT64 Qword;  // Use for 64 bit transfer.
        PVOID pBuffer; // Use for string transfer.
    } Data;
} CRONO_KERNEL_TRANSFER;


//typedef struct
//{
//    DWORD item;         /* ITEM_TYPE */
//    DWORD fNotSharable;
//    union
//    {
//        /* ITEM_MEMORY */
//        struct
//        {
//            PHYS_ADDR pPhysicalAddr; /* Physical address on card */
//            UINT64 qwBytes;          /* Address range */
//            KPTR pTransAddr;         /* Kernel-mode mapping of the physical base
//                                        address, to be used for transfer
//                                        commands; returned by WD_CardRegister()
//                                      */
//            UPTR pUserDirectAddr;    /* User-mode mapping of the physical base
//                                        address, for direct user read/write
//                                        transfers; returned by WD_CardRegister()
//                                      */
//            DWORD dwBar;             /* PCI Base Address Register number */
//            DWORD dwOptions;         /* Bitmask of WD_ITEM_MEM_OPTIONS flags */
//            KPTR pReserved;          /* Reserved for internal use */
//        } Mem;
//
//        /* ITEM_IO */
//        struct
//        {
//            KPTR  pAddr;    /* Beginning of I/O address */
//            DWORD dwBytes;  /* I/O range */
//            DWORD dwBar;    /* PCI Base Address Register number */
//        } IO;
//
//    } I;
//} CRONO_KERNEL_ITEMS;

//enum { CRONO_KERNEL_CARD_ITEMS = 20 };

//typedef struct
//{
//    DWORD dwItems;
//    CRONO_KERNEL_ITEMS Item[CRONO_KERNEL_CARD_ITEMS];
//} CRONO_KERNEL_CARD;

//typedef struct
//{
//    CRONO_KERNEL_CARD Card;           // Card to register.
//    DWORD fCheckLockOnly;   // Only check if card is lockable, return hCard=1 if
//                            // OK.
//    DWORD hCard;            // Handle of card.
//    DWORD dwOptions;        // Should be zero.
//    CHAR cName[32];         // Name of card.
//    CHAR cDescription[100]; // Description.
//} WD_CARD_REGISTER, WD_CARD_REGISTER_V118;

enum { CRONO_KERNEL_PCI_CARDS = 100 }; // Slots max X Functions max

typedef struct
{
#ifdef __linux__
    DWORD dwDomain; 
#endif
    DWORD dwBus;
    DWORD dwSlot;
    DWORD dwFunction;
} CRONO_KERNEL_PCI_SLOT;

typedef struct
{
    DWORD dwVendorId;
    DWORD dwDeviceId;
} CRONO_KERNEL_PCI_ID;

typedef struct
{
    CRONO_KERNEL_PCI_SLOT pciSlot;    /* PCI slot information */
    //CRONO_KERNEL_CARD Card;           /* Card information */
} CRONO_KERNEL_PCI_CARD_INFO;

//typedef struct
//{
//    DWORD hKernelPlugIn;
//    PCHAR pcDriverName;
//    PCHAR pcDriverPath; // Should be NULL (exists for backward compatibility).
//                        // The driver will be searched in the operating
//                        // system's drivers/modules directory.
//    PVOID pOpenData;
//} CRONO_KERNEL_KERNEL_PLUGIN;

typedef enum
{
    EVENT_STATUS_OK = 0
} EVENT_STATUS;

typedef enum {
    CRONO_KERNEL_STATUS_SUCCESS = 0x0L,
    CRONO_KERNEL_STATUS_INVALID_CRONO_KERNEL_HANDLE = 0xffffffff,

    CRONO_KERNEL_CRONO_KERNEL_STATUS_ERROR = 0x20000000L,

    CRONO_KERNEL_INVALID_HANDLE = 0x20000001L,
    CRONO_KERNEL_READ_WRITE_CONFLICT = 0x20000003L, /* Request to read from an OUT (write)
                                           *  pipe or request to write to an IN
                                           *  (read) pipe */
    CRONO_KERNEL_ZERO_PACKET_SIZE = 0x20000004L, /* Maximum packet size is zero */
    CRONO_KERNEL_INSUFFICIENT_RESOURCES = 0x20000005L,
    CRONO_KERNEL_SYSTEM_INTERNAL_ERROR = 0x20000007L,
    CRONO_KERNEL_DATA_MISMATCH = 0x20000008L,
    CRONO_KERNEL_NOT_IMPLEMENTED = 0x2000000aL,
    CRONO_KERNEL_KERPLUG_FAILURE = 0x2000000bL,
    CRONO_KERNEL_RESOURCE_OVERLAP = 0x2000000eL,
    CRONO_KERNEL_DEVICE_NOT_FOUND = 0x2000000fL,
    CRONO_KERNEL_WRONG_UNIQUE_ID = 0x20000010L,
    CRONO_KERNEL_OPERATION_ALREADY_DONE = 0x20000011L,
    CRONO_KERNEL_SET_CONFIGURATION_FAILED = 0x20000013L,
    CRONO_KERNEL_CANT_OBTAIN_PDO = 0x20000014L,
    CRONO_KERNEL_TIME_OUT_EXPIRED = 0x20000015L,
    CRONO_KERNEL_IRP_CANCELED = 0x20000016L,
    CRONO_KERNEL_FAILED_USER_MAPPING = 0x20000017L,
    CRONO_KERNEL_FAILED_KERNEL_MAPPING = 0x20000018L,
    CRONO_KERNEL_NO_RESOURCES_ON_DEVICE = 0x20000019L,
    CRONO_KERNEL_NO_EVENTS = 0x2000001aL,
    CRONO_KERNEL_INVALID_PARAMETER = 0x2000001bL,
    CRONO_KERNEL_INCORRECT_VERSION = 0x2000001cL,
    CRONO_KERNEL_TRY_AGAIN = 0x2000001dL,
    CRONO_KERNEL_CRONO_KERNEL_NOT_FOUND = 0x2000001eL,
    CRONO_KERNEL_INVALID_IOCTL = 0x2000001fL,
    CRONO_KERNEL_OPERATION_FAILED = 0x20000020L,
    CRONO_KERNEL_TOO_MANY_HANDLES = 0x20000022L,
    CRONO_KERNEL_NO_DEVICE_OBJECT = 0x20000023L,
	CRONO_KERNEL_OS_PLATFORM_MISMATCH = 0x20000024L,

} CRONO_KERNEL_ERROR_CODES;

typedef enum
{
    CRONO_KERNEL_INSERT                  = 0x1,
    CRONO_KERNEL_REMOVE                  = 0x2,
    CRONO_KERNEL_CPCI_REENUM             = 0x8,
    CRONO_KERNEL_POWER_CHANGED_D0        = 0x10, // Power states for the power management
    CRONO_KERNEL_POWER_CHANGED_D1        = 0x20,
    CRONO_KERNEL_POWER_CHANGED_D2        = 0x40,
    CRONO_KERNEL_POWER_CHANGED_D3        = 0x80,
    CRONO_KERNEL_POWER_SYSTEM_WORKING    = 0x100,
    CRONO_KERNEL_POWER_SYSTEM_SLEEPING1  = 0x200,
    CRONO_KERNEL_POWER_SYSTEM_SLEEPING2  = 0x400,
    CRONO_KERNEL_POWER_SYSTEM_SLEEPING3  = 0x800,
    CRONO_KERNEL_POWER_SYSTEM_HIBERNATE  = 0x1000,
    CRONO_KERNEL_POWER_SYSTEM_SHUTDOWN   = 0x2000
} CRONO_KERNEL_EVENT_ACTION;

typedef enum
{
    CRONO_KERNEL_ACKNOWLEDGE              = 0x1,
    CRONO_KERNEL_ACCEPT_CONTROL           = 0x2  // used in WD_EVENT_SEND (acknowledge)
} CRONO_KERNEL_EVENT_OPTION;

typedef struct
{
    DWORD handle;
    DWORD dwAction; // WD_EVENT_ACTION
    DWORD dwStatus; // EVENT_STATUS
    DWORD dwEventId;
    DWORD hKernelPlugIn;
    DWORD dwOptions; // WD_EVENT_OPTION
    union
    {
        struct
        {
            CRONO_KERNEL_PCI_ID cardId;
            CRONO_KERNEL_PCI_SLOT pciSlot;
        } Pci;
    } u;
    DWORD dwEventVer;
} CRONO_KERNEL_EVENT;

#ifndef BZERO
    #define BZERO(buf) memset(&(buf), 0, sizeof(buf))
#endif

/* *end* extracted from windrvr.h */

CRONO_KERNEL_API const char * DLLCALLCONV Stat2Str(DWORD dwStatus);

#include "crono_ioctl.h"

/* extracted from wdc_lib.h */
/**************************************************************
  General definitions
 **************************************************************/
#define MAX_NAME 128
#define MAX_DESC 128
#define MAX_NAME_DISPLAY 22

/* Handle to device information struct */
typedef void *CRONO_KERNEL_DEVICE_HANDLE;

/* PCI/PCMCIA slot */
typedef union {
    CRONO_KERNEL_PCI_SLOT    pciSlot;
} CRONO_KERNEL_SLOT_U;

/* PCI scan results */
typedef struct {
    DWORD       dwNumDevices;             /* Number of matching devices */
    CRONO_KERNEL_PCI_ID   deviceId[CRONO_KERNEL_PCI_CARDS];   /* Array of matching device IDs */
    CRONO_KERNEL_PCI_SLOT deviceSlot[CRONO_KERNEL_PCI_CARDS]; /* Array of matching device locations
                                           */
} CRONO_KERNEL_PCI_SCAN_RESULT;

/* Driver open options */
/* Basic driver open flags */
#define CRONO_KERNEL_DRV_OPEN_CHECK_VER 0x1 /* Compare source files WinDriver version
                                    * with that of the running WinDriver kernel
                                    */
#define CRONO_KERNEL_DRV_OPEN_REG_LIC   0x2 /* Register WinDriver license */
/* Convenient driver open options */
#define CRONO_KERNEL_DRV_OPEN_BASIC     0x0 /* No option -> perform only the basic open
                                      driver tasks, which are always performed
                                      by WDC_DriverOpen (mainly - open a handle
                                      to WinDriver) */
#define CRONO_KERNEL_DRV_OPEN_KP CRONO_KERNEL_DRV_OPEN_BASIC /* Kernel PlugIn driver open
                                            * options <=> basic */
#define CRONO_KERNEL_DRV_OPEN_ALL (CRONO_KERNEL_DRV_OPEN_CHECK_VER | CRONO_KERNEL_DRV_OPEN_REG_LIC)
#if defined(__KERNEL__)
#define CRONO_KERNEL_DRV_OPEN_DEFAULT CRONO_KERNEL_DRV_OPEN_KP
#else
#define CRONO_KERNEL_DRV_OPEN_DEFAULT CRONO_KERNEL_DRV_OPEN_ALL
#endif

typedef DWORD CRONO_KERNEL_DRV_OPEN_OPTIONS;

/**************************************************************
  Function Prototypes
 **************************************************************/
/* -----------------------------------------------
    General
   ----------------------------------------------- */
/* Get a device's user context */
CRONO_KERNEL_API PVOID DLLCALLCONV CRONO_KERNEL_GetDevContext(CRONO_KERNEL_DEVICE_HANDLE hDev);

/* -----------------------------------------------
    Open/close driver and init/uninit CRONO_KERNEL library
   ----------------------------------------------- */
#ifdef _WIN32
DWORD DLLCALLCONV CRONO_KERNEL_DriverOpen(CRONO_KERNEL_DRV_OPEN_OPTIONS openOptions,
    const CHAR *sLicense);
#elif __linux__
DWORD DLLCALLCONV CRONO_KERNEL_DriverOpen(CRONO_KERNEL_DRV_OPEN_OPTIONS openOptions);
#endif
DWORD DLLCALLCONV CRONO_KERNEL_DriverClose(void);

/* -----------------------------------------------
    Scan bus (PCI/PCMCIA)
   ----------------------------------------------- */
CRONO_KERNEL_API DWORD DLLCALLCONV CRONO_KERNEL_PciScanDevices(DWORD dwVendorId, DWORD dwDeviceId,
    CRONO_KERNEL_PCI_SCAN_RESULT *pPciScanResult);

/* -------------------------------------------------
    Get device's resources information (PCI/PCMCIA)
   ------------------------------------------------- */
[[deprecated]]
CRONO_KERNEL_API DWORD DLLCALLCONV CRONO_KERNEL_PciGetDeviceInfo(CRONO_KERNEL_PCI_CARD_INFO *pDeviceInfo);

/* -----------------------------------------------
    Open/Close device
   ----------------------------------------------- */
#if !defined(__KERNEL__)
CRONO_KERNEL_API DWORD DLLCALLCONV CRONO_KERNEL_PciDeviceOpen(CRONO_KERNEL_DEVICE_HANDLE *phDev,
    const CRONO_KERNEL_PCI_CARD_INFO *pDeviceInfo, const PVOID pDevCtx,
    PVOID reserved, const CHAR *pcKPDriverName, PVOID pKPOpenData);

CRONO_KERNEL_API DWORD DLLCALLCONV CRONO_KERNEL_PciDeviceClose(CRONO_KERNEL_DEVICE_HANDLE hDev);

#endif

/* -----------------------------------------------
    Set card cleanup commands
   ----------------------------------------------- */
CRONO_KERNEL_API DWORD CRONO_KERNEL_CardCleanupSetup(CRONO_KERNEL_DEVICE_HANDLE hDev, CRONO_KERNEL_TRANSFER *Cmd,
    DWORD dwCmds, BOOL bForceCleanup);

/* -----------------------------------------------
    Read/Write memory and I/O addresses
   ----------------------------------------------- */

/* Read/write a device's address space (8/16/32/64 bits) */
CRONO_KERNEL_API DWORD DLLCALLCONV CRONO_KERNEL_ReadAddr8(CRONO_KERNEL_DEVICE_HANDLE hDev, DWORD dwAddrSpace,
    KPTR dwOffset, BYTE *val);
CRONO_KERNEL_API DWORD DLLCALLCONV CRONO_KERNEL_ReadAddr16(CRONO_KERNEL_DEVICE_HANDLE hDev, DWORD dwAddrSpace,
    KPTR dwOffset, WORD *val);
CRONO_KERNEL_API DWORD DLLCALLCONV CRONO_KERNEL_ReadAddr32(CRONO_KERNEL_DEVICE_HANDLE hDev, DWORD dwAddrSpace,
    KPTR dwOffset, UINT32 *val);
CRONO_KERNEL_API DWORD DLLCALLCONV CRONO_KERNEL_ReadAddr64(CRONO_KERNEL_DEVICE_HANDLE hDev, DWORD dwAddrSpace,
    KPTR dwOffset, UINT64 *val);

CRONO_KERNEL_API DWORD DLLCALLCONV CRONO_KERNEL_WriteAddr8(CRONO_KERNEL_DEVICE_HANDLE hDev, DWORD dwAddrSpace,
    KPTR dwOffset, BYTE val);
CRONO_KERNEL_API DWORD DLLCALLCONV CRONO_KERNEL_WriteAddr16(CRONO_KERNEL_DEVICE_HANDLE hDev, DWORD dwAddrSpace,
    KPTR dwOffset, WORD val);
CRONO_KERNEL_API DWORD DLLCALLCONV CRONO_KERNEL_WriteAddr32(CRONO_KERNEL_DEVICE_HANDLE hDev, DWORD dwAddrSpace,
    KPTR dwOffset, UINT32 val);
CRONO_KERNEL_API DWORD DLLCALLCONV CRONO_KERNEL_WriteAddr64(CRONO_KERNEL_DEVICE_HANDLE hDev, DWORD dwAddrSpace,
    KPTR dwOffset, UINT64 val);

/* Is address space active */
CRONO_KERNEL_API BOOL DLLCALLCONV CRONO_KERNEL_AddrSpaceIsActive(CRONO_KERNEL_DEVICE_HANDLE hDev,
    DWORD dwAddrSpace);

/* -----------------------------------------------
    Access PCI configuration space
   ----------------------------------------------- */
/* Read/write 8/16/32/64 bits from the PCI configuration space.
   Identify device by handle */
CRONO_KERNEL_API DWORD DLLCALLCONV CRONO_KERNEL_PciReadCfg32(CRONO_KERNEL_DEVICE_HANDLE hDev, DWORD dwOffset,
    UINT32 *val);

CRONO_KERNEL_API DWORD DLLCALLCONV CRONO_KERNEL_PciWriteCfg32(CRONO_KERNEL_DEVICE_HANDLE hDev, DWORD dwOffset,
	UINT32 val);

/* -----------------------------------------------
    DMA (Direct Memory Access)
   ----------------------------------------------- */
/* Allocate and lock a contiguous DMA buffer */
CRONO_KERNEL_API DWORD DLLCALLCONV CRONO_KERNEL_DMAContigBufLock(CRONO_KERNEL_DEVICE_HANDLE hDev, PVOID *ppBuf,
	DWORD dwOptions, DWORD dwDMABufSize, CRONO_KERNEL_DMA **ppDma);
   
/**
 * Lock a Scatter/Gather DMA buffer.
 * This function calls the user mode process to  create a mapping of virtual space 
 * to a list of chunked physical addresses (mdl). 
 * The kernel mode process also locks the mapping.
 * 
 * @return DMASGBufLock_parameters, that are now completely filled. 
 * The relevant part (MDL etc.) is stored in the CRONO_KERNEL_DMA struct.
 * Any error that appear will be stored in the item value.
*/
CRONO_KERNEL_API DWORD DLLCALLCONV CRONO_KERNEL_DMASGBufLock(CRONO_KERNEL_DEVICE_HANDLE hDev, PVOID pBuf,
    DWORD dwOptions, DWORD dwDMABufSize, CRONO_KERNEL_DMA **ppDma);

/* Unlock a DMA buffer */
CRONO_KERNEL_API DWORD DLLCALLCONV CRONO_KERNEL_DMABufUnlock(CRONO_KERNEL_DEVICE_HANDLE hDev, CRONO_KERNEL_DMA *pDma);

/* Synchronize cache of all CPUs with the DMA buffer,
 * should be called before DMA transfer */
[[deprecated]]
CRONO_KERNEL_API DWORD DLLCALLCONV CRONO_KERNEL_DMASyncCpu(CRONO_KERNEL_DMA *pDma);

/* Flush the data from I/O cache and update the CPU caches.
 * Should be called after DMA transfer. */
[[deprecated]]
CRONO_KERNEL_API DWORD DLLCALLCONV CRONO_KERNEL_DMASyncIo(CRONO_KERNEL_DMA *pDma);



/* *end* extracted from wdc_lib.h */

/* extracted from wd_def.h */
/* Address space information struct */
typedef struct {
    DWORD dwAddrSpace;         /* Address space number */
    BOOL  fIsMemory;           /* TRUE: memory address space; FALSE: I/O */
    DWORD dwItemIndex;         /* Index of address space in the
                                  pDev->cardReg.Card.Item array */
    UINT64 qwBytes;            /* Size of address space */
//    KPTR   pAddr;              /* I/O / Memory kernel mapped address -- for
//                                  WD_Transfer(), WD_MultiTransfer(), or direct
//                                  kernel access */
    UPTR pUserDirectMemAddr;   /* Memory address for direct user-mode access */
	KPTR pPhysMemAddr;			/* Physical memeory address */
#ifdef __linux__
    size_t dwSize ;              // Added for Linux, to be used with munmap
#endif
} CRONO_KERNEL_ADDR_DESC;

/* -----------------------------------------------
    General
   ----------------------------------------------- */
/* PCI/PCMCIA device ID */
typedef union {
    CRONO_KERNEL_PCI_ID    pciId;
} CRONO_KERNEL_ID_U;

/* Device information struct */
typedef struct CRONO_KERNEL_DEVICE {
    CRONO_KERNEL_ID_U                id;              /* PCI/PCMCIA device ID */
    CRONO_KERNEL_SLOT_U              slot;            /* PCI/PCMCIA device slot location
				                                      * information */
    DWORD							dwNumAddrSpaces; /* Total number of device's address
		                                              * spaces */
    CRONO_KERNEL_ADDR_DESC          *pAddrDesc;       /* Array of device's address spaces
						                              * information */
//    WD_CARD_REGISTER				cardReg;         /* Device's resources information */

//    CRONO_KERNEL_KERNEL_PLUGIN      kerPlug;         /* Kernel PlugIn information */

    CRONO_KERNEL_EVENT              Event;           /* Event information */
    HANDLE							hEvent;

    PVOID							pCtx;            /* User-specific context */

	HANDLE							hDevice;		/* For device ioctl calls  */
    CRONO_KERNEL_ADDR_DESC          bar_addr;      // BAR0 userspace mapped address

#ifdef __linux__
    /**
     * The name of the corresponding `miscdev` file, found under /dev
    */
    char miscdev_name[CRONO_MAX_DEV_NAME_SIZE]; 
#endif
} CRONO_KERNEL_DEVICE, *PCRONO_KERNEL_DEVICE;

/*************************************************************
  General utility macros
 *************************************************************/
/* -----------------------------------------------
    Memory / I/O / Registers
   ----------------------------------------------- */
/* NOTE: pAddrDesc param should be of type CRONO_KERNEL_ADDR_DESC* */

/* Check if memory or I/O address */
#define CRONO_KERNEL_ADDR_IS_MEM(pAddrDesc) (pAddrDesc)->fIsMemory


/* end wd_def.h */

/**
 * Return Error Code `error_code` if condition `cond_to_validate` is TRUE
 * 
 * @param cond_to_validate[in]: logical conditino to be validated. 
 * @param error_code[in]: error code to be returned.
*/
#define CRONO_RET_ERR_CODE_IF_TRUE(cond_to_validate, error_code) \
            if (cond_to_validate) { return error_code ; }
/**
 * Return Error Code `error_code` if value `value_to_validate` is NULL
 * 
 * @param value_to_validate[in]: value to be validated, mostly a pointer. 
 * @param error_code[in]: error code to be returned.
*/
#define CRONO_RET_ERR_CODE_IF_NULL(value_to_validate, error_code) \
            if (NULL ==(value_to_validate)) { return error_code ; }
/**
 * Return Error Code `CRONO_KERNEL_INVALID_PARAMETER` 
 * if value `value_to_validate` is NULL
 * 
 * @param value_to_validate[in]: value to be validated, mostly a pointer.
*/
#define CRONO_RET_INV_PARAM_IF_NULL(value_to_validate) \
            if (NULL ==(value_to_validate)) { return CRONO_KERNEL_INVALID_PARAMETER ; }

/**
 * Return Error Code `CRONO_KERNEL_INVALID_PARAMETER` 
 * if value `value_to_validate` is 0
 * 
 * @param value_to_validate[in]: value to be validated, mostly a pointer.
*/
#define CRONO_RET_INV_PARAM_IF_ZERO(value_to_validate) \
            if (0 ==(value_to_validate)) { return CRONO_KERNEL_INVALID_PARAMETER ; }

#ifdef __linux__
#define sprintf_s snprintf
void printFreeMemInfoDebug(const char* msg);
#ifdef CRONO_DEBUG_ENABLED
    #define CRONO_DEBUG(...)                fprintf(stdout, __VA_ARGS__) ;
    #define CRONO_DEBUG_MEM_MSG(fmt, ...)   fprintf(stdout, "crono Memory Debug Info: "fmt, ...) 
#else   // #ifdef CRONO_DEBUG
    #define CRONO_DEBUG(...)
    #define CRONO_DEBUG_MEM_MSG(fmt, ...)   
#endif  // #ifdef CRONO_DEBUG
#endif  // #ifdef __linux__

#ifdef __cplusplus
}
#endif

#endif /* __CRONO_KERNEL_INTERFACE_H__ */
