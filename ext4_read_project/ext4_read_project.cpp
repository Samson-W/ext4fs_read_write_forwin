// ext4_read_project.cpp: ����Ŀ�ļ���

#include "stdafx.h"
#include <string>
#include "ext4_read.h"

using namespace System;

/*
*open_rootfile:��ɸ�Ŀ¼\�µ�����Ϊfindname��Ŀ¼���ļ��Ĳ���
*ext2part:ext2��������
*findname:Ҫ���ҵ�Ŀ¼���ļ�������
*/
Ext2File *open_rootfile(Ext2Partition *ext2part, string findname)
{
	if(ext2part == NULL || findname.length() == 0)
	{
		printf("parentpart Invalid Parameter \n");
		return NULL;
	}
	Ext2File *ptr = ext2part->get_root();
	Ext2File *dir_readfile = NULL;
	if(!ptr)
	{
		printf("get root %s is invalid.\n", ext2part->get_linux_name().c_str());
		return NULL;
	}
	EXT2DIRENT *dir_rent = ext2part->open_dir(ptr);
	if(!dir_rent)
	{
		printf("open conf dir failure\n");
		return NULL;
	}
	dir_readfile = ext2part->read_dir(dir_rent, findname);
	if(!dir_rent->next)
	{
		printf("conf dir find file failure\n");
		return NULL;
	}
	return dir_readfile;
}

/*
*openfile:��ɸ�Ŀ¼\���µ�Ŀ¼�µ�����Ϊfindname��Ŀ¼���ļ��Ĳ���
*parentfile:�ϼ�Ŀ¼�������Ϣ
*findname:Ҫ���ҵ�Ŀ¼���ļ�������
*/
Ext2File *openfile(Ext2File *parentfile, string findname)
{
	if(parentfile == NULL || findname.length() == 0)
	{
		printf("parentpart Invalid Parameter \n");
		return NULL;
	}
	Ext2Partition *temp = parentfile->partition;
	Ext2File *dir_readfile = NULL;
		
	EXT2DIRENT *dir_rent = temp->open_dir(parentfile);
	if(!dir_rent)
	{
		printf("open conf dir failure\n");
		return NULL;
	}
	dir_readfile = temp->read_dir(dir_rent, findname);
	if(!dir_rent->next)
	{
		printf("conf dir find file failure\n");
		return NULL;
	}
	return dir_readfile;
}

/*
*rename_file: ��ɸ�Ŀ¼\���µ�Ŀ¼�µ�����Ϊfindname���ļ��Ĳ��Ҳ��޸��ļ���Ϊnewname
*parentfile:�ϼ�Ŀ¼�������Ϣ
*findname:Ҫ�����޸ĵĵ�Ŀ¼���ļ�������
*newname:�޸ĺ���ļ�����
*/
Ext2File *rename_file(Ext2File *parentfile, string findname, char *newname)
{
	if(parentfile == NULL || findname.length() == 0 || newname == NULL)
	{
		printf("parentpart Invalid Parameter \n");
		return NULL;
	}
	Ext2Partition *temp = parentfile->partition;
	Ext2File *dir_readfile = NULL;
		
	EXT2DIRENT *dir_rent = temp->open_dir(parentfile);
	if(!dir_rent)
	{
		printf("open conf dir failure\n");
		return NULL;
	}
	dir_readfile = temp->rename_filename(dir_rent, findname, newname);
	if(!dir_rent->next)
	{
		printf("conf dir find file failure\n");
		return NULL;
	}
	return dir_readfile;
}

/*
*modify_file: ����modi_ext2file��Ӧ���ļ��е����ݣ��ҵ�searchkey�ؼ��֣�
*����ƫ��λ��Ϊoffset��ֵ�����޸�Ϊsetnewvalue���˷��������޸Ĺؼ���+ƫ���� λ�õĵ����ַ�ֵ��
*modi_ext2file: Ҫ�����޸ĵ��ļ���Ӧ����ؽṹ
*searchkey:���ļ��в��ҵĹؼ���
*offset:Ҫ�����޸ĵ�����ڹؼ��ֵ�ƫ����
*setnewvalue:�Թؼ���λ��+ƫ����Ҫ�����޸ĵ��µ�ֵ
*/
int modify_file(Ext2File *modi_ext2file,char *searchkey, int offset,char setnewvalue)
{
	if(modi_ext2file == NULL || searchkey == NULL)
	{
		printf("parentpart Invalid Parameter \n");
		return -1;
	}
	lloff_t blocks, blkindex;
	int blksize, ret, modnum;
	char filebuffer[4096] = {0};
	blksize = modi_ext2file->partition->get_blocksize();
	blocks = modi_ext2file->file_size / blksize;
	modnum = modi_ext2file->file_size % blksize;
	if(modnum > 0 && modnum < blksize)
		blocks += 1;
	for(blkindex = 0; blkindex < blocks; blkindex++)
	{
		//��ȡ�ļ�������
		ret = modi_ext2file->partition->read_data_block(&modi_ext2file->inode, blkindex, filebuffer);
		if(ret < 0)
		{
			printf("read data block error 1\n");
			return -1;
		}
		//���ҵ�grub�ļ��е�searchkey��Ŀ����λ��
		char *defaultstr = strstr(filebuffer,searchkey);
		//����ؼ��ֲ������Ѷ�ȡ�Ŀ��У���ô�������ȡ��һ����
		if(defaultstr == NULL)
			continue;
		printf("offset  is %d\n", offset);
		//�޸���Ŀ�ı����
		*(defaultstr + offset) = setnewvalue;
		//���޸ĺ�Ŀ�д��Ӳ����
		modi_ext2file->partition->write_data_block(&modi_ext2file->inode, blkindex, filebuffer);
		return 0;
	}
}

int main(array<System::String ^> ^args)
{
    Console::WriteLine(L"Hello World");
	Ext2Partition *temp;
	list <Ext2Partition *> parts;
	list <Ext2Partition *>::iterator i   ;
    Ext2File *boot_dir_file, *grub_dir_file,*conf_dir_file;
	string getbootdir("boot"), grubdir("grub"), grubconf("testtest");
	int ret;
	char newname[32] = "testqq";

	Ext2Read *app = new Ext2Read;
	parts = app->get_partitions();
	//get boot dir
	for(i = parts.begin(); i != parts.end(); i++)
	{
		temp = (*i);
		boot_dir_file = open_rootfile(temp, getbootdir);
		if(boot_dir_file == NULL)
			continue;
		if(boot_dir_file->file_name.compare(getbootdir) == 0)
			break;
	}
	//get grub dir
	if(boot_dir_file == NULL)
	{
		printf("get boot dir is failure.\n");
		getchar();
		return -1;
	}
	grub_dir_file = openfile(boot_dir_file, grubdir);
	if(grub_dir_file == NULL || grub_dir_file->file_name.compare(grubdir) != 0)
	{
		printf("get grub dir is failure.\n");
		getchar();
		return -1;
	}
	//�޸Ĳ��ҵ�grubconf�ļ������޸�Ϊ�µ�����
	conf_dir_file = rename_file(grub_dir_file,grubconf,newname);
	if(conf_dir_file == NULL || conf_dir_file->file_name.compare(newname) != 0)
	{
		printf("rename conf file is failure.\n");
		getchar();
		return -1;
	}
	//�޸��ļ��еĹؼ���Ϊ"set default"�����е��Թؼ��ֵĵ�һ����ĸΪ����ַƫ��13���ֽڵ�ֵ�޸�Ϊ��ֵ
	ret = modify_file(conf_dir_file,"set default", 13, '0');
	if(ret != 0)
	{
		printf("modify_file is failure.\n");
		getchar();
		return -1;
	}
	conf_dir_file->partition->ext2_close_disk();
	getchar();
    return 0;
}
