
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

#endif /* __CRONO_IOCTL_H__ */
