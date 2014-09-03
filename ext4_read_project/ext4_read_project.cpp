// ext4_read_project.cpp: 主项目文件。

#include "stdafx.h"
#include <string>
#include "ext4_read.h"

using namespace System;

/*
*open_rootfile:完成根目录\下的名称为findname的目录或文件的查找
*ext2part:ext2分区对象
*findname:要查找的目录或文件的名称
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
*openfile:完成根目录\以下的目录下的名称为findname的目录或文件的查找
*parentfile:上级目录的相关信息
*findname:要查找的目录或文件的名称
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
*rename_file: 完成根目录\以下的目录下的名称为findname的文件的查找并修改文件名为newname
*parentfile:上级目录的相关信息
*findname:要查找修改的的目录或文件的名称
*newname:修改后的文件名称
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
*modify_file: 查找modi_ext2file对应的文件中的内容，找到searchkey关键字，
*对其偏移位置为offset的值进行修改为setnewvalue，此方仅用于修改关键字+偏移量 位置的单个字符值；
*modi_ext2file: 要进行修改的文件对应的相关结构
*searchkey:在文件中查找的关键字
*offset:要进行修改的相对于关键字的偏移量
*setnewvalue:对关键字位置+偏移量要进行修改的新的值
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
		//读取文件的内容
		ret = modi_ext2file->partition->read_data_block(&modi_ext2file->inode, blkindex, filebuffer);
		if(ret < 0)
		{
			printf("read data block error 1\n");
			return -1;
		}
		//查找到grub文件中的searchkey条目所在位置
		char *defaultstr = strstr(filebuffer,searchkey);
		//如果关键字不存在已读取的块中，那么则继续读取下一个块
		if(defaultstr == NULL)
			continue;
		printf("offset  is %d\n", offset);
		//修改条目的编译号
		*(defaultstr + offset) = setnewvalue;
		//将修改后的块写回硬盘中
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
	//修改查找的grubconf文件名并修改为新的名称
	conf_dir_file = rename_file(grub_dir_file,grubconf,newname);
	if(conf_dir_file == NULL || conf_dir_file->file_name.compare(newname) != 0)
	{
		printf("rename conf file is failure.\n");
		getchar();
		return -1;
	}
	//修改文件中的关键字为"set default"的行中的以关键字的第一个字母为基地址偏移13个字节的值修改为０值
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
