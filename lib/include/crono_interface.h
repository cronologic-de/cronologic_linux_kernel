//
// Header file containing structs and #defines commonly used by header files of derived CRONO device classes
// The current driver version for Ndigo devices is 1.4.3.0
// The current driver version for xTDC4/TimeTagger4 devices is 1.4.1
// The current driver version for xHPTDC8 devices is 0.18.0.x
// The current driver version for Ndigo6G devices is 0.1.0.x
//

#ifndef CRONO_COMMON_INTERFACE_H
#define CRONO_COMMON_INTERFACE_H

#include "stdint.h"

/** All ERRORS must be positive integers, because the upper byte is used by crono_tools */
#define CRONO_OK							0
#define CRONO_WINDRIVER_NOT_FOUND			1
#define CRONO_DEVICE_NOT_FOUND				2
#define CRONO_NOT_INITIALIZED				3
// when a capture on a closed card is called
#define CRONO_WRONG_STATE					4
// The pointer given to a xtdc4 driver function was not a valid pointer
#define CRONO_INVALID_DEVICE				5
#define CRONO_BUFFER_ALLOC_FAILED			6
#define CRONO_TDC_NO_EDGE_FOUND				7
#define CRONO_INVALID_BUFFER_PARAMETERS		8
#define CRONO_INVALID_CONFIG_PARAMETERS		9
#define CRONO_WINDOW_CALIBRATION_FAILED		10
#define CRONO_HARDWARE_FAILURE				11
#define CRONO_INVALID_ADC_MODE				12
#define CRONO_SYNCHRONIZATION_FAILED		13
#define CRONO_DEVICE_OPEN_FAILED			14
#define CRONO_INTERNAL_ERROR				15
#define CRONO_CALIBRATION_FAILURE			16
#define CRONO_INVALID_ARGUMENTS             17

/**
* Packet types supported by different cronologic boards
*/
#define CRONO_PACKET_TYPE_8_BIT_SIGNED 0
#define CRONO_PACKET_TYPE_16_BIT_SIGNED 1
#define CRONO_PACKET_TYPE_32_BIT_SIGNED 2
#define CRONO_PACKET_TYPE_64_BIT_SIGNED 3
#define CRONO_PACKET_TYPE_8_BIT_UNSIGNED 4
#define CRONO_PACKET_TYPE_16_BIT_UNSIGNED 5
#define CRONO_PACKET_TYPE_32_BIT_UNSIGNED 6
#define CRONO_PACKET_TYPE_64_BIT_UNSIGNED 7
#define CRONO_PACKET_TYPE_TDC_DATA 8
#define CRONO_PACKET_TYPE_TIMESTAMP_ONLY 128
#define CRONO_PACKET_TYPE_END_OF_BUFFER 129
#define CRONO_PACKET_TYPE_TRIGGER_PATTERN 130

/**
* Errors concering the data of a packets or its processing.
*/
#define CRONO_PACKET_FLAG_SHORTENED 1
#define CRONO_PACKET_FLAG_PACKETS_LOST 2
#define CRONO_PACKET_FLAG_OVERFLOW 4
#define CRONO_PACKET_FLAG_TRIGGER_MISSED 8
#define CRONO_PACKET_FLAG_DMA_FIFO_FULL 16
#define CRONO_PACKET_FLAG_HOST_BUFFER_FULL 32
#define CRONO_PACKET_FLAG_TDC_NO_EDGE 64

/**
* Internal driver device ID of cronologic devices based on PCI Device ID
*/
#define CRONO_DEVICE_UNKNOWN		0x0
#define CRONO_DEVICE_HPTDC			0x1
#define CRONO_DEVICE_NDIGO5G		0x2
#define CRONO_DEVICE_NDIGO250M		0x4
#define CRONO_DEVICE_NDIGO_AVRG		0x3
#define CRONO_DEVICE_XTDC4			0x6
#define CRONO_DEVICE_FMC_TDC10		0x7
#define CRONO_DEVICE_TIMETAGGER4	0x8
#define CRONO_DEVICE_D_AVE12		0x9
#define CRONO_DEVICE_D_AVE14		0xa
#define CRONO_DEVICE_NDIGO2G14		0xb
#define CRONO_DEVICE_XHPTDC8		0xc
#define CRONO_DEVICE_NDIGO6G12		0xd

/**
* Device states
* A device must be configured before data capturing is started.
*/
// device is created but not yet initialized
#define CRONO_DEVICE_STATE_CREATED		0
// device is initialized but not yet configured for data capture
#define CRONO_DEVICE_STATE_INITIALIZED	1
// device is ready to capture data
#define CRONO_DEVICE_STATE_CONFIGURED	2
// device has started data capture
#define CRONO_DEVICE_STATE_CAPTURING	3
// data capture is paused
#define CRONO_DEVICE_STATE_PAUSED		4
// device is closed
#define CRONO_DEVICE_STATE_CLOSED		5

// reading packets from the device was successful
#define CRONO_READ_OK 0
// trying to read packets does not yield data
#define CRONO_READ_NO_DATA 1
// some unhandled error occured a device reinit is required
#define CRONO_READ_INTERNAL_ERROR 2
// trying to read packets does not yield data in the given amount of time
#define CRONO_READ_TIMEOUT 3

#ifdef __cplusplus
extern "C" {
#endif

	// data type used for boolean values in data structures
	typedef uint8_t crono_bool_t;

	/**
	* Basic device data structure for synchronizing cronologic Ndigo5G and HPTDC8 devices
	*/
	typedef struct {
		/* CRONO_DEVICE_*/
		int device_type;
		/** For HPTDC use this board id, Ndigo uses configured board id*/
		int board_id;
		void * device;
	} crono_device;

	/**
	* Packet data structure in ring buffer for packets carrying varying amounts of data.
	*
	* The size of the data[] array is given in the length field.
	*/
	typedef struct {
		// number of the source channel of the data
		uint8_t channel;
		// id of the card
		uint8_t card;
		// type of packet. One of CRONO_PACKET_TYPE_* 
		uint8_t type;
		// Bit field of CRONO_PACKET_FLAG_* bits
		uint8_t flags;
		// length of data array in multiples of 8 bytes
		uint32_t length;
		// timestamp of packet creation, may be start or end of data depending on packet source.
		int64_t timestamp;
		// payload of the packet. Data type must be cast according to CRONO_PACKET_TYPE_*
		uint64_t data[1];
	} crono_packet;

	/**
	* Packet data structure in ring buffer for packets carrying only the header.
	*/
	typedef struct {
		// number of the source channel
		uint8_t channel;
		// id of the card
		uint8_t card;
		// type of packet. Must be CRONO_PACKET_TYPE_TIMESTAMP_ONLY
		uint8_t type;
		// Bit field of CRONO_PACKET_FLAG_* bits
		uint8_t flags;
		// either 0 or a bit field carrying data
		uint32_t length;
		// timestamp of packet creation
		int64_t timestamp;
	} crono_packet_only_timestamp;

	/**
	* Returns the legnth of the data array of the packet in multiples of 8 bytes.
	*/
#define crono_packet_data_length(current) ((current)->type &128?0:(current)->length)

	/**
	* Returns the legnth of the of the packet including its header in multiples of eight bytes.
	*/
#define crono_packet_bytes(current) ((crono_packet_data_length(current) + 2) * 8)

	/**
	* Returns a crono_packet pointer pointing to the next packet in the ring buffer.
	* Must be checked befor use to not point beyond the last packet in the buffer.
	*/
#define crono_next_packet(current) ((volatile crono_packet*) (((int64_t) (current)) +( ((current)->type&128?0:(current)->length) + 2) * 8))

#ifdef __cplusplus
}
#endif

#endif