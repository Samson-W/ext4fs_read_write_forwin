/**
 * Ext2read
 * File: platform.h
 **/

#ifndef __PLATFORM_H
#define __PLATFORM_H

#include <windows.h>
#include <stdint.h>

 typedef HANDLE FileHandle;

 typedef unsigned char uint8_t;
 typedef unsigned short uint16_t;
 typedef unsigned int uint32_t;

 
#define FILE_DELIM		"\\"

typedef uint64_t lloff_t;

typedef unsigned int uint;
typedef unsigned long ulong;

#define SECTOR_SIZE             512

#define PACKED

#ifndef INLINE
   #define INLINE __inline
#endif /* INLINE */

#ifdef __cplusplus
extern "C"{
#endif


FileHandle open_disk(const char *, int *);
int get_ndisks();
void close_disk(FileHandle handle);
int read_disk(FileHandle hnd, void *ptr, lloff_t sector, int nsects, int sectorsize);
int write_disk(FileHandle hnd, void *ptr, lloff_t sector, int nsects, int sectorsize);
int get_nthdevice(char *path, int ndisks);

#ifdef __cplusplus
}
#endif


#endif // __PLATFORM_H
