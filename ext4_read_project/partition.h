
#ifndef __PARTITION_H
#define __PARTITION_H

#include "platform_win32.h"
#include "ext2read.h"

struct MBRpartition {
	unsigned char boot_ind;         /* 0x80 - active */
	unsigned char head;             /* starting head */
	unsigned char sector;           /* starting sector */
	unsigned char cyl;              /* starting cylinder */
	unsigned char sys_ind;          /* What partition type */
	unsigned char end_head;         /* end head */
	unsigned char end_sector;       /* end sector */
	unsigned char end_cyl;          /* end cylinder */
	unsigned char start4[4];        /* starting sector counting from 0 */
	unsigned char size4[4];         /* nr of sectors in partition */
};

#define SECTOR_SIZE	512

#define pt_offset(b, n)	((struct MBRpartition *)((b) + 0x1be + (n) * sizeof(struct MBRpartition)))

/* A valid partition table sector ends in 0x55 0xaa */
static INLINE unsigned int 
part_table_flag(unsigned char *b) 
{
	return ((uint) b[510]) + (((uint) b[511]) << 8);
}

static INLINE int 
valid_part_table_flag(unsigned char *b) 
{
	return (b[510] == 0x55 && b[511] == 0xaa);
}

/* start_sect and nr_sects are stored little endian on all machines */
/* moreover, they are not aligned correctly */
static INLINE void 
store4_little_endian(unsigned char *cp, unsigned int val) 
{
	cp[0] = (val & 0xff);
	cp[1] = ((val >> 8) & 0xff);
	cp[2] = ((val >> 16) & 0xff);
	cp[3] = ((val >> 24) & 0xff);
}

static INLINE unsigned int 
read4_little_endian(unsigned char *cp) 
{
	return (uint)(cp[0]) + ((uint)(cp[1]) << 8) \
		+ ((uint)(cp[2]) << 16) + ((uint)(cp[3]) << 24);
}

static INLINE void 
set_start_sect(struct MBRpartition *p, unsigned int start_sect) 
{
	store4_little_endian(p->start4, start_sect);
}

static INLINE unsigned int 
get_start_sect(struct MBRpartition *p) 
{
	return read4_little_endian(p->start4);
}

static INLINE void 
set_nr_sects(struct MBRpartition *p, unsigned int nr_sects) 
{
	store4_little_endian(p->size4, nr_sects);
}

static INLINE unsigned int 
get_nr_sects(struct MBRpartition *p) 
{
	return read4_little_endian(p->size4);
}

#endif //__PARTITION_H