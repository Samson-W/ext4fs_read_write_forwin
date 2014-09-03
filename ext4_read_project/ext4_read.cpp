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
	//��ʾ����չ������ʼ�������������������(Ӧ�����������һ��������)
    ebrBase = base;
    nextPart = base;
    while(1)
    {
		//��ȡ���߼������ĵ�һ������
        ret = read_disk(handle, sector, nextPart, 1, sectsize);
        if(ret < 0)
            return ret;

        if(ret < sectsize)
        {
            printf("Error Reading the EBR \n");
            return -1;
        }
		//�õ���ǰ�߼������ķ�����Ϣ��ƫ��446�ֽ�
        part = pt_offset(sector, 0);
        printf("index %d ID %X size %Ld \n", logical, part->sys_ind, get_nr_sects(part));
		//���ļ�ϵͳΪEXT2����д�����¼�˷�����������������ǰ������������ţ�������С���豸���ƾ����
        if(part->sys_ind == EXT2)
        {
            partition = new Ext2Partition(get_nr_sects(part), get_start_sect(part)+ ebrBase + ebr2, sectsize, handle, NULL);
            if(partition->is_valid)
            {
				//Ϊʲôlogical�ĳ�ֵΪ4����Ϊ�߼������ķ������Ǵ�5��ʼ�ģ���set_linux_name�����н����б仯Ϊ+1
                partition->set_linux_name("/dev/sd", disk, logical);
				//��ext2�����Ķ�����뵽ext2��Ч�����б���
                nparts.push_back(partition);
            }
            else
            {
                delete partition;
            }
        }
		//�Է���Ϊlvm�ķ����Ĵ��� 
        if(part->sys_ind == LINUX_LVM)
        {
            printf("LVM Physical Volume found disk %d partition %d\n", disk, logical);
//            LVM lvm(handle, get_start_sect(part)+ ebrBase + ebr2, this);
//            lvm.scan_pv();
        }
		//�õ���һ���߼������ķ�����Ϣ����һ���߼�������Ϣ���ڵ�ǰ�߼�������ĵڶ����������У�
        part1 = pt_offset(sector, 1);
		//�õ���ǰ�����Ŀ�ʼ�������������(Ӧ�����������һ��������)
        ebr2 = get_start_sect(part1);
		//�ۼӵõ���ǰ�����Ŀ�ʼ���������������
        nextPart = (ebr2 + ebrBase);
		//�ۼӵõ������ĸ���
        logical++;
		//����߼��������еĵڶ�������ļ�ϵͳΪ0�����ʾ�Ѿ�û���߼�������
        if(part1->sys_ind == 0)
            break;
    }
    return logical;
}

list<Ext2Partition *> Ext2Read::get_partitions()
{
    return nparts;
}