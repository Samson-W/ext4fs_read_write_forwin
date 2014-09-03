#include "StdAfx.h"
#include "ext4_read.h"
#include "ext4fs.h"

//size:分区的总扇区数  offset:此分区的相对开始扇区数  ssize：扇区的大小  phandle:设备控制句柄
Ext2Partition::Ext2Partition(lloff_t size, lloff_t offset, int ssize, FileHandle phandle, void *vol)
{
    int ret;

    total_sectors = size;
    relative_sect = offset;
    handle = phandle;
    sect_size = ssize;
    onview = false;
    inode_buffer = NULL;
    //wbc lvol = vol;
    //wbc buffercache.setMaxCost(MAX_CACHE_SIZE);
    //has_extent = 1;
    ret = mount();
    if(ret < 0)
    {
        is_valid = false;
        return;
    }

    root = read_inode(EXT2_ROOT_INO);
    if(!root)
    {
        is_valid = false;
        printf("Cannot read the root of %s \n", linux_name.c_str());
        return;
    }

    root->file_name = linux_name;
    root->file_type = 0x02;   //FIXME: do not hardcode
    is_valid = true;
}

Ext2Partition::~Ext2Partition()
{
    free (desc);
}

void Ext2Partition::set_linux_name(const char *name, int disk, int partition)
{
    char dchar = 'a' + disk;
    char pchar = '1' + partition;


    linux_name = name;
    linux_name.append(1, dchar);
    linux_name.append(1, pchar);
}

string &Ext2Partition::get_linux_name()
{
    return linux_name;
}

//读取一个块(根据文件系统格式时的配置而不同，一般为4096)
int Ext2Partition::ext2_readblock(lloff_t blocknum, void *buffer)
{
    char *newbuffer = NULL;
	//得到一个块的扇区数
    int nsects = blocksize/sect_size;
    int ret;
    lloff_t sectno;

    if(!newbuffer)
    {
        newbuffer = new char[blocksize];
        if(!newbuffer)
            return -1;
		/* wbc
        if(lvol)
        {
            //sectno = lvol->lvm_mapper((lloff_t)nsects * blocknum);
			;
        }
        else*/
        {
			//得到块号对应的扇区偏移号
            sectno = (lloff_t)((lloff_t)nsects * blocknum) + relative_sect;
        }
		//得到sectno扇区开始的nsects个扇区的数据到存储空间中
        ret = read_disk(handle, newbuffer, sectno, nsects, sect_size);
        if(ret < 0)
        {
            delete [] newbuffer;
            return ret;
        }
    }

    memcpy(buffer, newbuffer, blocksize);

    return 0;
}

int Ext2Partition::ext2_writeblock(lloff_t blocknum, void *buffer)
{
    char *newbuffer = NULL;
    int nsects = blocksize/sect_size;
    int ret;
    lloff_t sectno;

    //wbc newbuffer = buffercache.take(blocknum);
    if(buffer)
    {
        {
            sectno = (lloff_t)((lloff_t)nsects * blocknum) + relative_sect;
        }
        ret = write_disk(handle, buffer, sectno, nsects, sect_size);
        if(ret < 0)
        {
            return ret;
        }
    }
    return 0;
}

int Ext2Partition::mount()
{
    EXT2_SUPER_BLOCK sblock;
    int gSizes, gSizeb;		/* Size of total group desc in sectors */
    char *tmpbuf;
	//读取超级块数据，读取位置为分区的1024字节后的1024字节
    read_disk(handle, &sblock, relative_sect + 2, 2, sect_size);	/* read superBlock of root */
    if(sblock.s_magic != EXT2_SUPER_MAGIC)
    {
        printf("Bad Super Block. The drive %s is not ext2 formatted.\n", linux_name.c_str());
        return -1;
    }
	//判断此分区是否支持ext2的特性
    if(sblock.s_feature_incompat & EXT2_FEATURE_INCOMPAT_COMPRESSION)
    {
        printf("File system compression is used which is not supported.\n");
    }
	//得到块的大小
    blocksize = EXT2_BLOCK_SIZE(&sblock);
	//得到每个group中有多少个inode
    inodes_per_group = EXT2_INODES_PER_GROUP(&sblock);
	//得到每个inode结构体的长度
    inode_size = EXT2_INODE_SIZE(&sblock);

    printf("Block size %d, inp %d, inodesize %d\n", blocksize, inodes_per_group, inode_size);
	//得到总的group的个数，用整个分区的块数/每个group的块的个数
    totalGroups = (sblock.s_blocks_count)/EXT2_BLOCKS_PER_GROUP(&sblock);
	//得到分区所有的group描述信息的大小
    gSizeb = (sizeof(EXT2_GROUP_DESC) * totalGroups);
	//得到所有group描述信息所占的扇区数
    gSizes = (gSizeb / sect_size)+1;
	//为所有group分配描述信息的空间
    desc = (EXT2_GROUP_DESC *) calloc(totalGroups, sizeof(EXT2_GROUP_DESC));
    if(desc == NULL)
    {
        printf("Not enough Memory: mount: desc: Exiting\n");
        exit(1);
    }
	//分配所有group描述信息所占的字节长度的存储空间
    if((tmpbuf = (char *) malloc(gSizes * sect_size)) == NULL)
    {
        printf("Not enough Memory: mount: tmpbuf: Exiting\n");
        exit(1);
    }

    /* Read all Group descriptors and store in buffer */
    /* I really dont know the official start location of Group Descriptor array */
    if((blocksize/sect_size) <= 2)
	{
		//relative_sect是本分区相对于整个硬盘的开始扇区号
        read_disk(handle, tmpbuf, relative_sect + ((blocksize/sect_size) + 2), gSizes, sect_size);
	}
    else
	{
		//relative_sect是本分区相对于整个硬盘的开始扇区号，偏移一个块的大小是为什么？偏移一个块是因为第一个块存储的是Boot Block等相关的信息
        read_disk(handle, tmpbuf, relative_sect + (blocksize/sect_size), gSizes, sect_size);
	}
	//得到所有的group信息存储到缓存desc中
    memcpy(desc, tmpbuf, gSizeb);

    free(tmpbuf);
    return 0;
}

EXT2DIRENT *Ext2Partition::open_dir(Ext2File *parent)
{
    EXT2DIRENT *dirent;

    if(!parent)
        return NULL;

    dirent = new EXT2DIRENT;
    dirent->parent = parent;
    dirent->next = NULL;
    dirent->dirbuf = NULL;
    dirent->read_bytes = 0;
    dirent->next_block = 0;

    return dirent;
}

Ext2File *Ext2Partition::read_dir(EXT2DIRENT *dirent, string getname)
{
    string filename;
    Ext2File *newEntry;
    char *pos;
    int ret;

    if(!dirent)
        return NULL;
    if(!dirent->dirbuf)
    {
        dirent->dirbuf = (EXT2_DIR_ENTRY *) new char[blocksize];
        if(!dirent->dirbuf)
            return NULL;
        ret = read_data_block(&dirent->parent->inode, dirent->next_block, dirent->dirbuf);
        if(ret < 0)
            return NULL;

        dirent->next_block++;
    }

    again:
    if(!dirent->next)
        dirent->next = dirent->dirbuf;
    else
    {
        pos = (char *) dirent->next;
		//得到当前目录下的下一项
        dirent->next = (EXT2_DIR_ENTRY *)(pos + dirent->next->rec_len);
        if(IS_BUFFER_END(dirent->next, dirent->dirbuf, blocksize))
        {
            dirent->next = NULL;
            if(dirent->read_bytes < dirent->parent->file_size)
            {
                ret = read_data_block(&dirent->parent->inode, dirent->next_block, dirent->dirbuf);
                if(ret < 0)
                    return NULL;

                dirent->next_block++;
                goto again;
            }
            return NULL;
        }
    }

    dirent->read_bytes += dirent->next->rec_len;
    filename.assign(dirent->next->name, dirent->next->name_len);
	//对比是否已经查找到需要进行查找的文件名或文件夹名
    if((filename.compare(".") == 0) ||
       (filename.compare("..") == 0) || (filename != getname))
        goto again;
	
    newEntry = read_inode(dirent->next->inode);
	printf("reading Inode %d parent inode %d.\n", dirent->next->inode, dirent->parent->inode_num);
    if(!newEntry)
    {
        printf("Error reading Inode %d parent inode %d.\n", dirent->next->inode, dirent->parent->inode_num);
        return NULL;
    }

    newEntry->file_type = dirent->next->filetype;
    newEntry->file_name = filename;

    return newEntry;
}


Ext2File *Ext2Partition::rename_filename(EXT2DIRENT *dirent, string getname, char *newname)
{
    string filename(newname);;
    Ext2File *newEntry;
    char *pos;
    int ret;
	lloff_t modblocknum;

    if(!dirent)
        return NULL;
    if(!dirent->dirbuf)
    {
        dirent->dirbuf = (EXT2_DIR_ENTRY *) new char[blocksize];
        if(!dirent->dirbuf)
            return NULL;
        ret = read_data_block(&dirent->parent->inode, dirent->next_block, dirent->dirbuf);
        if(ret < 0)
            return NULL;

        dirent->next_block++;
    }

    again:
    if(!dirent->next)
        dirent->next = dirent->dirbuf;
    else
    {
        pos = (char *) dirent->next;
        dirent->next = (EXT2_DIR_ENTRY *)(pos + dirent->next->rec_len);
        if(IS_BUFFER_END(dirent->next, dirent->dirbuf, blocksize))
        {
            dirent->next = NULL;
            if(dirent->read_bytes < dirent->parent->file_size)
            {
                //LOG("DIR: Reading next block %d parent %s\n", dirent->next_block, dirent->parent->file_name.c_str());
                ret = read_data_block(&dirent->parent->inode, dirent->next_block, dirent->dirbuf);
                if(ret < 0)
                    return NULL;

                dirent->next_block++;
                goto again;
            }
            return NULL;
        }
    }

    dirent->read_bytes += dirent->next->rec_len;
    filename.assign(dirent->next->name, dirent->next->name_len);
    if((filename.compare(".") == 0) ||
       (filename.compare("..") == 0) || /*filename.compare("boot")*/(filename != getname))
        goto again;
	//wbc rename filename
	
	dirent->next->name_len = strlen(newname);
	memcpy(dirent->next->name, newname, strlen(newname));
	modblocknum = dirent->next_block - 1;
	ret = write_data_block(&dirent->parent->inode, modblocknum, dirent->dirbuf);
    if(ret < 0)
      return NULL;
	
    newEntry = read_inode(dirent->next->inode);
	printf("reading Inode %d parent inode %d.\n", dirent->next->inode, dirent->parent->inode_num);
    if(!newEntry)
    {
        printf("Error reading Inode %d parent inode %d.\n", dirent->next->inode, dirent->parent->inode_num);
        return NULL;
    }

    newEntry->file_type = dirent->next->filetype;
    newEntry->file_name = string(newname);//filename;

    return newEntry;
}



void Ext2Partition::close_dir(EXT2DIRENT *dirent)
{
    delete [] dirent->dirbuf;
    delete dirent;
}

//得到一个inode的相关信息
Ext2File *Ext2Partition::read_inode(uint32_t inum)
{
    uint32_t group, index, blknum;
    int inode_index, ret = 0;
    Ext2File *file = NULL;
    EXT2_INODE *src;

    if(inum == 0)
        return NULL;

    if(!inode_buffer)
    {
        inode_buffer = (char *)malloc(blocksize);
        if(!inode_buffer)
            return NULL;
    }
	//得到inum所在的group号
    group = (inum - 1) / inodes_per_group;

    if(group > totalGroups)
    {
        printf("Error Reading Inode %X. Invalid Inode Number\n", inum);
        return NULL;
    }
	//得到inum在group中的偏移字节数
    index = ((inum - 1) % inodes_per_group) * inode_size;
	//得到在所属group中的偏移索引
    inode_index = (index % blocksize);
	//得到所在group中的inode表的位置
    blknum = desc[group].bg_inode_table + (index / blocksize);

    if(blknum != last_block)
        ret = ext2_readblock(blknum, inode_buffer);

    file = new Ext2File;
    if(!file)
    {
        printf("Allocation of File Failed. \n");
        return NULL;
    }
	//得到inode描述信息
    src = (EXT2_INODE *)(inode_buffer + inode_index);
    file->inode = *src;

    //LOG("BLKNUM is %d, inode_index %d\n", file->inode.i_size, inode_index);
    file->inode_num = inum;
    file->file_size = (lloff_t) src->i_size | ((lloff_t) src->i_size_high << 32);
    if(file->file_size == 0)
    {
        printf("Inode %d with file size 0\n", inum);
    }
    file->partition = (Ext2Partition *)this;
    file->onview = false;

    last_block = blknum;

    return file;
}

/*
*ino:inode号
*lbn:要读取的此inode对应的内容的第几个偏移块
*buf:读取的块存储区
*/
int Ext2Partition::read_data_block(EXT2_INODE *ino, lloff_t lbn, void *buf)
{
    lloff_t block;

    if(INODE_HAS_EXTENT(ino))
        block = extent_to_logical(ino, lbn);
    else
        block = fileblock_to_logical(ino, lbn);

    if(block == 0)
        return -1;

    return ext2_readblock(block, buf);
}

int Ext2Partition::write_data_block(EXT2_INODE *ino, lloff_t lbn, void *buf)
{
    lloff_t block;

    if(INODE_HAS_EXTENT(ino))
        block = extent_to_logical(ino, lbn);
    else
        block = fileblock_to_logical(ino, lbn);

    if(block == 0)
        return -1;

    return ext2_writeblock(block, buf);
}

lloff_t Ext2Partition::extent_binarysearch(EXT4_EXTENT_HEADER *header, lloff_t lbn, bool isallocated)
{
    EXT4_EXTENT *extent;
    EXT4_EXTENT_IDX *index;
    EXT4_EXTENT_HEADER *child;
    lloff_t physical_block = 0;
    lloff_t block;

    if(header->eh_magic != EXT4_EXT_MAGIC)
    {
        printf("Invalid magic in Extent Header: %X\n", header->eh_magic);
        return 0;
    }
    extent = EXT_FIRST_EXTENT(header);
    //    LOG("HEADER: magic %x Entries: %d depth %d\n", header->eh_magic, header->eh_entries, header->eh_depth);
    if(header->eh_depth == 0)
    {        
        for(int i = 0; i < header->eh_entries; i++)
        {         
            //          LOG("EXTENT: Block: %d Length: %d LBN: %d\n", extent->ee_block, extent->ee_len, lbn);
            if((lbn >= extent->ee_block) &&
               (lbn < (extent->ee_block + extent->ee_len)))
            {
                physical_block = ext_to_block(extent) + lbn;
                physical_block = physical_block - (lloff_t)extent->ee_block;
                if(isallocated)
                    delete [] header;
                //                LOG("Physical Block: %d\n", physical_block);
                return physical_block;
            }
            extent++; // Pointer increment by size of Extent.
        }
        return 0;
    }

    index = EXT_FIRST_INDEX(header);
    for(int i = 0; i < header->eh_entries; i++)
    {
        //        LOG("INDEX: Block: %d Leaf: %d \n", index->ei_block, index->ei_leaf_lo);
        if(lbn >= index->ei_block)
        {
            child = (EXT4_EXTENT_HEADER *) new char [blocksize];
            block = idx_to_block(index);
            ext2_readblock(block, (void *) child);

            return extent_binarysearch(child, lbn, true);
        }
    }

    // We reach here if we do not find the key
    if(isallocated)
        delete [] header;

    return physical_block;
}
/*
*根据ino及lbn得到对应的数据块的块号
*/
lloff_t Ext2Partition::extent_to_logical(EXT2_INODE *ino, lloff_t lbn)
{
    lloff_t block;
    struct ext4_extent_header *header;

    header = get_ext4_header(ino);
	//得到inode对应的数据的块号
    block = extent_binarysearch(header, lbn, false);

    return block;
}

uint32_t Ext2Partition::fileblock_to_logical(EXT2_INODE *ino, uint32_t lbn)
{
    uint32_t block, indlast, dindlast;
    uint32_t tmpblk, sz;
    uint32_t *indbuffer;
    uint32_t *dindbuffer;
    uint32_t *tindbuffer;

    if(lbn < EXT2_NDIR_BLOCKS)
    {
        return ino->i_block[lbn];
    }

    sz = blocksize / sizeof(uint32_t);
    indlast = sz + EXT2_NDIR_BLOCKS;
    indbuffer = new uint32_t [sz];
    if((lbn >= EXT2_NDIR_BLOCKS) && (lbn < indlast))
    {
        block = ino->i_block[EXT2_IND_BLOCK];
        ext2_readblock(block, indbuffer);
        lbn -= EXT2_NDIR_BLOCKS;
        block = indbuffer[lbn];
        delete [] indbuffer;
        return block;
    }

    dindlast = (sz * sz) + indlast;
    dindbuffer = new uint32_t [sz];
    if((lbn >= indlast) && (lbn < dindlast))
    {
        block = ino->i_block[EXT2_DIND_BLOCK];
        ext2_readblock(block, dindbuffer);

        tmpblk = lbn - indlast;
        block = dindbuffer[tmpblk/sz];
        ext2_readblock(block, indbuffer);

        lbn = tmpblk % sz;
        block = indbuffer[lbn];

        delete [] dindbuffer;
        delete [] indbuffer;
        return block;
    }

    tindbuffer = new uint32_t [sz];
    if(lbn >= dindlast)
    {
        block = ino->i_block[EXT2_TIND_BLOCK];
        ext2_readblock(block, tindbuffer);

        tmpblk = lbn - dindlast;
        block = tindbuffer[tmpblk/(sz * sz)];
        ext2_readblock(block, dindbuffer);

        block = tmpblk / sz;
        lbn = tmpblk % sz;
        block = dindbuffer[block];
        ext2_readblock(block, indbuffer);
        block = indbuffer[lbn];                                                                                                                                                                                                                                                                                                                                                                                                                                                                            

        delete [] tindbuffer;
        delete [] dindbuffer;
        delete [] indbuffer;

        return block;
    }

    // We should not reach here
    return 0;
}

Ext2File *Ext2Partition::read_file_from_inode(uint32_t inum, char *content_buffer,uint32_t size )
{
    uint32_t group, index, blknum;
    int inode_index, ret = 0;
    Ext2File *file = NULL;

    if(inum == 0 || size <= 0)
        return NULL;

    if(!content_buffer)
    {
		return NULL;
    }

    group = (inum - 1) / inodes_per_group;

    if(group > totalGroups)
    {
        printf("Error Reading Inode %X. Invalid Inode Number\n", inum);
        return NULL;
    }

    index = ((inum - 1) % inodes_per_group) * inode_size;
    inode_index = (index % blocksize);
    blknum = desc[group].bg_inode_table + (index / blocksize);


    //if(blknum != last_block)
        ret = ext2_readblock(blknum, content_buffer);
	/*
    file = new Ext2File;
    if(!file)
    {
        printf("Allocation of File Failed. \n");
        return NULL;
    }
    src = (EXT2_INODE *)(inode_buffer + inode_index);
    file->inode = *src;

    //LOG("BLKNUM is %d, inode_index %d\n", file->inode.i_size, inode_index);
    file->inode_num = inum;
    file->file_size = (lloff_t) src->i_size | ((lloff_t) src->i_size_high << 32);
    if(file->file_size == 0)
    {
        printf("Inode %d with file size 0\n", inum);
    }
    file->partition = (Ext2Partition *)this;
    file->onview = false;

    last_block = blknum;
	*/
    return file;
}

void Ext2Partition::ext2_close_disk()
{
	close_disk(handle);
}