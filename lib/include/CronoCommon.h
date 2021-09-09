#ifndef CRONO_COMMON_H
#define CRONO_COMMON_H
#include <vector>
#include "assert.h"

#if defined(DEBUG) || defined(_DEBUG)
#define assertmsg(_Expression,_msg) (void)( (!!(_Expression)) || (_wassert(_CRT_WIDE(#_msg), _CRT_WIDE(__FILE__), __LINE__), 0) )
#else
#define assertmsg(_Expression,_msg) 
#endif

#define DebugAssert(x,y) assert(x)

#ifdef __linux__
#include <unistd.h>
#include <string>
#define __stdcall
#define _snprintf_s(a,b,c,...) snprintf(a,b,__VA_ARGS__)
#endif  //#ifdef __linux__

#include <stdint.h>
typedef uint8_t uint8;
typedef int16_t int16;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

//computes the minimum of the array
template <class T>
T amin(T* arr, int size) {
	int res = arr[0];
	for (int i = 1; i < size; i++) {
		if (arr[i] < res) {
			res = arr[i];
		}
	}
	return res;
}

// computes the max of the array
template <class T>
T amax(T* arr, int size) {
	int res = arr[0];
	for (int i = 1; i < size; i++) {
		if (arr[i] > res) {
			res = arr[i];
		}
	}
	return res;
}

class crono_exception : public std::exception
{
private:
	std::string text;
public:
	crono_exception(const char* st) { text = st; }
	virtual ~crono_exception() throw() {};
	virtual const char* what() { return text.c_str(); };
};

#define crono_sleep(x) usleep(1000*x)

#endif