#include "stdafx.h"
#include "ext4_read.h"
#include "platform_win32.h"
#include "partition.h"
#include "parttypes.h"

Ext2Read::Ext2Read()
{
    scan_system();
}

Ext2Read::~Ext2Read()
{
    
}

void Ext2Read::scan_system()
{
    char pathname[20];
    int ret;

    ndisks = get_ndisks();
    printf("No of disks %d\n", ndisks);
	
    for(int i = 0; i < ndisks; i++)
    {
        get_nthdevice(pathname, ndisks);
        printf("Scanning %s\n", pathname);
        ret = scan_partitions(pathname, i);
        if(ret < 0)
        {
            printf("Scanning of %s failed: ", pathname);
            continue;
        }
    }
	/*
    // Now Mount the LVM Partitions
    if(groups.empty())
        return;

    list<VolumeGroup *>::iterator i;
    VolumeGroup *grp;
    for(i = groups.begin(); i != groups.end(); i++)
    {
        grp = (*i);
        grp->logical_mount();
    }
	*/
}

/* Scans The partitions */
int Ext2Read::scan_partitions(char *path, int diskno)
{
    unsigned char sector[SECTOR_SIZE];
    struct MBRpartition *part;
    //Ext2Partition *partition;
    FileHandle handle;
    int sector_size;
    int ret, i;

    handle = open_disk(path, &sector_size);
    if(handle < 0)
        return -1;

    ret = read_disk(handle,sector, 0, 1, sector_size);
    if(ret < 0)
        return ret;

    if(ret < sector_size)
    {
        printf("Error Reading the MBR on %s \n", path);
        return -1;
    }
    if(!valid_part_table_flag(sector))
    {
        printf("Partition Table Error on %s\n", path);
        printf("Invalid End of sector marker");
        return -INVALID_TABLE;
    }

    /* First Scan primary Partitions */
    for(i = 0; i < 4; i++)
    {
        part = pt_offset(sector, i);
        if((part->sys_ind != 0x00) || (get_nr_sects(part) != 0x00))
        {
            printf("index %d ID %X size %Ld start_sect %Ld\n", i, part->sys_ind, get_nr_sects(part), get_start_sect(part));

            if(part->sys_ind == EXT2)
            {
				printf("Linux Partition found on disk %d partition %d\n", diskno, i);
                //partition = new Ext2Partition(get_nr_sects(part), get_start_sect(part), sector_size, handle, NULL);
                //if(partition->is_valid)
                //{
//                    partition->set_linux_name("/dev/sd", diskno, i);
//                    nparts.push_back(partition);
				//	;
                    //printf("Linux Partition found on disk %d partition %d\n", diskno, i);
                //}
                //else
                //{	
//                    delete partition;
                //}
            }

            if(part->sys_ind == LINUX_LVM)
            {
                printf("LVM Physical Volume found disk %d partition %d\n", diskno, i);
                //LVM lvm(handle, get_start_sect(part), this);
                //lvm.scan_pv();
            }
            else if((part->sys_ind == 0x05) || (part->sys_ind == 0x0f))
            {
                scan_ebr(handle, get_start_sect(part), sector_size, diskno);
            }
        }
    }

    return 0;
}

/* Reads The Extended Partitions */
int Ext2Read::scan_ebr(FileHandle handle, lloff_t base, int sectsize, int disk)
{
    unsigned char sector[SECTOR_SIZE];
    struct MBRpartition *part, *part1;
    Ext2Partition *partition;
    int logical = 4, ret;
    lloff_t  ebrBase, nextPart, ebr2=0;
	//表示此扩展分区开始的首扇区的相对扇区号(应该是相对于上一个分区吧)
    ebrBase = base;
    nextPart = base;
    while(1)
    {
		//读取此逻辑分区的第一个扇区
        ret = read_disk(handle, sector, nextPart, 1, sectsize);
        if(ret < 0)
            return ret;

        if(ret < sectsize)
        {
            printf("Error Reading the EBR \n");
            return -1;
        }
		//得到当前逻辑分区的分区信息，偏移446字节
        part = pt_offset(sector, 0);
        printf("index %d ID %X size %Ld \n", logical, part->sys_ind, get_nr_sects(part));
		//若文件系统为EXT2则进行处理，记录此分区的总扇区数，当前分区相对扇区号，扇区大小，设备控制句柄。
        if(part->sys_ind == EXT2)
        {
            partition = new Ext2Partition(get_nr_sects(part), get_start_sect(part)+ ebrBase + ebr2, sectsize, handle, NULL);
            if(partition->is_valid)
            {
				//为什么logical的初值为4，因为逻辑分区的分区号是从5开始的，在set_linux_name函数中将进行变化为+1
                partition->set_linux_name("/dev/sd", disk, logical);
				//将ext2分区的对象存入到ext2有效分区列表中
                nparts.push_back(partition);
            }
            else
            {
                delete partition;
            }
        }
		//对分区为lvm的分区的处理 
        if(part->sys_ind == LINUX_LVM)
        {
            printf("LVM Physical Volume found disk %d partition %d\n", disk, logical);
//            LVM lvm(handle, get_start_sect(part)+ ebrBase + ebr2, this);
//            lvm.scan_pv();
        }
		//得到下一个逻辑分区的分区信息，下一个逻辑分区信息是在当前逻辑分区表的第二个分区表中；
        part1 = pt_offset(sector, 1);
		//得到当前分区的开始扇区相对扇区号(应该是相对于上一个分区的)
        ebr2 = get_start_sect(part1);
		//累加得到当前分区的开始扇区的相对扇区号
        nextPart = (ebr2 + ebrBase);
		//累加得到分区的个数
        logical++;
		//如果逻辑分区表中的第二个表的文件系统为0，则表示已经没有逻辑分区了
        if(part1->sys_ind == 0)
            break;
    }
    return logical;
}

list<Ext2Partition *> Ext2Read::get_partitions()
{
    return nparts;
}