
//
// This header file contains all declarations shared between driver and user
// applications.
//

#ifndef __CRONO_IOCTL_H__
#define __CRONO_IOCTL_H__ (1)

//
// The following value is arbitrarily chosen from the space defined by Microsoft
// as being "for non-Microsoft use"
//
#define FILE_DEVICE_CRONOMEMDRV 0xCF53

#ifdef __linux__
/**
 * Command value passed to miscdev ioctl() to lock a memory buffer.
 * 'c' is for `cronologic`.
*/
#define IOCTL_CRONO_LOCK_BUFFER		_IOWR('c', 0, DMASGBufLock_parameters*)
/**
 * Command value passed to miscdev ioctl() to unlock a memory buffer
 * 'c' is for `cronologic`.
*/
#define IOCTL_CRONO_UNLOCK_BUFFER	_IOWR('c', 1, DMASGBufLock_parameters*)	
#else
//
// Device control codes - values between 2048 and 4095 arbitrarily chosen
//
#define IOCTL_CRONO_LOCK_BUFFER				CTL_CODE(FILE_DEVICE_CRONOMEMDRV, 2050, METHOD_OUT_DIRECT, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_CRONO_GETMDL					CTL_CODE(FILE_DEVICE_CRONOMEMDRV, 2051, METHOD_OUT_DIRECT, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_CRONO_LOCK_CONTIG_BUFFER		CTL_CODE(FILE_DEVICE_CRONOMEMDRV, 2060, METHOD_OUT_DIRECT, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_CRONO_UNLOCK_BUFFER			CTL_CODE(FILE_DEVICE_CRONOMEMDRV, 2061, METHOD_OUT_DIRECT, FILE_READ_ACCESS | FILE_WRITE_ACCESS)	
#define IOCTL_CRONO_CLEANUP_SETUP			CTL_CODE(FILE_DEVICE_CRONOMEMDRV, 2070, METHOD_OUT_DIRECT, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_CRONO_DEVICE_MEM_ADDRESS		CTL_CODE(FILE_DEVICE_CRONOMEMDRV, 2071, METHOD_OUT_DIRECT, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_CRONO_CFG_READ				CTL_CODE(FILE_DEVICE_CRONOMEMDRV, 2080, METHOD_OUT_DIRECT, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_CRONO_CFG_WRITE				CTL_CODE(FILE_DEVICE_CRONOMEMDRV, 2081, METHOD_OUT_DIRECT, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#endif // #ifndef __linux__

/*
#define IOCTL_WD_DMA_LOCK                 WD_CTL_CODE(0x9be)
#define IOCTL_WD_DMA_UNLOCK               WD_CTL_CODE(0x902)
#define IOCTL_WD_TRANSFER                 WD_CTL_CODE(0x98c)
#define IOCTL_WD_MULTI_TRANSFER           WD_CTL_CODE(0x98d)
#define IOCTL_WD_PCI_SCAN_CARDS           WD_CTL_CODE(0x9a3)
#define IOCTL_WD_PCI_GET_CARD_INFO        WD_CTL_CODE(0x9e8)
#define IOCTL_WD_VERSION                  WD_CTL_CODE(0x910)
#define IOCTL_WD_PCI_CONFIG_DUMP          WD_CTL_CODE(0x91a)
#define IOCTL_WD_KERNEL_PLUGIN_OPEN       WD_CTL_CODE(0x91b)
#define IOCTL_WD_KERNEL_PLUGIN_CLOSE      WD_CTL_CODE(0x91c)
#define IOCTL_WD_KERNEL_PLUGIN_CALL       WD_CTL_CODE(0x91d)
#define IOCTL_WD_INT_ENABLE               WD_CTL_CODE(0x9b6)
#define IOCTL_WD_INT_DISABLE              WD_CTL_CODE(0x9bb)
#define IOCTL_WD_INT_COUNT                WD_CTL_CODE(0x9ba)
#define IOCTL_WD_ISAPNP_SCAN_CARDS        WD_CTL_CODE(0x924)
#define IOCTL_WD_ISAPNP_CONFIG_DUMP       WD_CTL_CODE(0x926)
#define IOCTL_WD_SLEEP                    WD_CTL_CODE(0x927)
#define IOCTL_WD_DEBUG                    WD_CTL_CODE(0x928)
#define IOCTL_WD_DEBUG_DUMP               WD_CTL_CODE(0x929)
#define IOCTL_WD_CARD_UNREGISTER          WD_CTL_CODE(0x9e7)
#define IOCTL_WD_ISAPNP_GET_CARD_INFO     WD_CTL_CODE(0x9e9)
#define IOCTL_WD_PCMCIA_SCAN_CARDS        WD_CTL_CODE(0x996)
#define IOCTL_WD_PCMCIA_GET_CARD_INFO     WD_CTL_CODE(0x9ea)
#define IOCTL_WD_PCMCIA_CONFIG_DUMP       WD_CTL_CODE(0x998)
#define IOCTL_WD_CARD_REGISTER            WD_CTL_CODE(0x9e6)
#define IOCTL_WD_INT_WAIT                 WD_CTL_CODE(0x9b9)
#define IOCTL_WD_LICENSE                  WD_CTL_CODE(0x9e4)
#define IOCTL_WD_EVENT_REGISTER           WD_CTL_CODE(0x9e2)
#define IOCTL_WD_EVENT_UNREGISTER         WD_CTL_CODE(0x987)
#define IOCTL_WD_EVENT_PULL               WD_CTL_CODE(0x988)
#define IOCTL_WD_EVENT_SEND               WD_CTL_CODE(0x989)
#define IOCTL_WD_DEBUG_ADD                WD_CTL_CODE(0x964)
#define IOCTL_WD_USAGE                    WD_CTL_CODE(0x976)
#define IOCTL_WDU_GET_DEVICE_DATA         WD_CTL_CODE(0x9a7)
#define IOCTL_WDU_SET_INTERFACE           WD_CTL_CODE(0x981)
#define IOCTL_WDU_RESET_PIPE              WD_CTL_CODE(0x982)
#define IOCTL_WDU_TRANSFER                WD_CTL_CODE(0x983)
#define IOCTL_WDU_HALT_TRANSFER           WD_CTL_CODE(0x985)
#define IOCTL_WD_WATCH_PCI_START          WD_CTL_CODE(0x9d6)
#define IOCTL_WD_WATCH_PCI_STOP           WD_CTL_CODE(0x99a)
#define IOCTL_WDU_WAKEUP                  WD_CTL_CODE(0x98a)
#define IOCTL_WDU_RESET_DEVICE            WD_CTL_CODE(0x98b)
#define IOCTL_WD_GET_DEVICE_PROPERTY      WD_CTL_CODE(0x990)
#define IOCTL_WD_CARD_CLEANUP_SETUP       WD_CTL_CODE(0x995)
#define IOCTL_WD_PCMCIA_CONTROL           WD_CTL_CODE(0x99b)
#define IOCTL_WD_DMA_SYNC_CPU             WD_CTL_CODE(0x99f)
#define IOCTL_WD_DMA_SYNC_IO              WD_CTL_CODE(0x9a0)
#define IOCTL_WDU_STREAM_OPEN             WD_CTL_CODE(0x9a8)
#define IOCTL_WDU_STREAM_CLOSE            WD_CTL_CODE(0x9a9)
#define IOCTL_WDU_STREAM_START            WD_CTL_CODE(0x9af)
#define IOCTL_WDU_STREAM_STOP             WD_CTL_CODE(0x9b0)
#define IOCTL_WDU_STREAM_FLUSH            WD_CTL_CODE(0x9aa)
#define IOCTL_WDU_STREAM_GET_STATUS       WD_CTL_CODE(0x9b5)
#define IOCTL_WDU_SELECTIVE_SUSPEND       WD_CTL_CODE(0x9ae)
#define IOCTL_WD_INTERRUPT_DONE_CE        WD_CTL_CODE(0x9bc)
#define IOCTL_WD_PCI_SCAN_CAPS            WD_CTL_CODE(0x9e5)
*/
#endif /* __CRONO_IOCTL_H__ */
