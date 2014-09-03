#include "StdAfx.h"
#include "ext4_read.h"
#include "ext4fs.h"

//size:��������������  offset:�˷�������Կ�ʼ������  ssize�������Ĵ�С  phandle:�豸���ƾ��
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

//��ȡһ����(�����ļ�ϵͳ��ʽʱ�����ö���ͬ��һ��Ϊ4096)
int Ext2Partition::ext2_readblock(lloff_t blocknum, void *buffer)
{
    char *newbuffer = NULL;
	//�õ�һ�����������
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
			//�õ���Ŷ�Ӧ������ƫ�ƺ�
            sectno = (lloff_t)((lloff_t)nsects * blocknum) + relative_sect;
        }
		//�õ�sectno������ʼ��nsects�����������ݵ��洢�ռ���
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
	//��ȡ���������ݣ���ȡλ��Ϊ������1024�ֽں��1024�ֽ�
    read_disk(handle, &sblock, relative_sect + 2, 2, sect_size);	/* read superBlock of root */
    if(sblock.s_magic != EXT2_SUPER_MAGIC)
    {
        printf("Bad Super Block. The drive %s is not ext2 formatted.\n", linux_name.c_str());
        return -1;
    }
	//�жϴ˷����Ƿ�֧��ext2������
    if(sblock.s_feature_incompat & EXT2_FEATURE_INCOMPAT_COMPRESSION)
    {
        printf("File system compression is used which is not supported.\n");
    }
	//�õ���Ĵ�С
    blocksize = EXT2_BLOCK_SIZE(&sblock);
	//�õ�ÿ��group���ж��ٸ�inode
    inodes_per_group = EXT2_INODES_PER_GROUP(&sblock);
	//�õ�ÿ��inode�ṹ��ĳ���
    inode_size = EXT2_INODE_SIZE(&sblock);

    printf("Block size %d, inp %d, inodesize %d\n", blocksize, inodes_per_group, inode_size);
	//�õ��ܵ�group�ĸ����������������Ŀ���/ÿ��group�Ŀ�ĸ���
    totalGroups = (sblock.s_blocks_count)/EXT2_BLOCKS_PER_GROUP(&sblock);
	//�õ��������е�group������Ϣ�Ĵ�С
    gSizeb = (sizeof(EXT2_GROUP_DESC) * totalGroups);
	//�õ�����group������Ϣ��ռ��������
    gSizes = (gSizeb / sect_size)+1;
	//Ϊ����group����������Ϣ�Ŀռ�
    desc = (EXT2_GROUP_DESC *) calloc(totalGroups, sizeof(EXT2_GROUP_DESC));
    if(desc == NULL)
    {
        printf("Not enough Memory: mount: desc: Exiting\n");
        exit(1);
    }
	//��������group������Ϣ��ռ���ֽڳ��ȵĴ洢�ռ�
    if((tmpbuf = (char *) malloc(gSizes * sect_size)) == NULL)
    {
        printf("Not enough Memory: mount: tmpbuf: Exiting\n");
        exit(1);
    }

    /* Read all Group descriptors and store in buffer */
    /* I really dont know the official start location of Group Descriptor array */
    if((blocksize/sect_size) <= 2)
	{
		//relative_sect�Ǳ��������������Ӳ�̵Ŀ�ʼ������
        read_disk(handle, tmpbuf, relative_sect + ((blocksize/sect_size) + 2), gSizes, sect_size);
	}
    else
	{
		//relative_sect�Ǳ��������������Ӳ�̵Ŀ�ʼ�����ţ�ƫ��һ����Ĵ�С��Ϊʲô��ƫ��һ��������Ϊ��һ����洢����Boot Block����ص���Ϣ
        read_disk(handle, tmpbuf, relative_sect + (blocksize/sect_size), gSizes, sect_size);
	}
	//�õ����е�group��Ϣ�洢������desc��
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
		//�õ���ǰĿ¼�µ���һ��
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
	//�Ա��Ƿ��Ѿ����ҵ���Ҫ���в��ҵ��ļ������ļ�����
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

//�õ�һ��inode�������Ϣ
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
	//�õ�inum���ڵ�group��
    group = (inum - 1) / inodes_per_group;

    if(group > totalGroups)
    {
        printf("Error Reading Inode %X. Invalid Inode Number\n", inum);
        return NULL;
    }
	//�õ�inum��group�е�ƫ���ֽ���
    index = ((inum - 1) % inodes_per_group) * inode_size;
	//�õ�������group�е�ƫ������
    inode_index = (index % blocksize);
	//�õ�����group�е�inode���λ��
    blknum = desc[group].bg_inode_table + (index / blocksize);

    if(blknum != last_block)
        ret = ext2_readblock(blknum, inode_buffer);

    file = new Ext2File;
    if(!file)
    {
        printf("Allocation of File Failed. \n");
        return NULL;
    }
	//�õ�inode������Ϣ
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
*ino:inode��
*lbn:Ҫ��ȡ�Ĵ�inode��Ӧ�����ݵĵڼ���ƫ�ƿ�
*buf:��ȡ�Ŀ�洢��
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
*����ino��lbn�õ���Ӧ�����ݿ�Ŀ��
*/
lloff_t Ext2Partition::extent_to_logical(EXT2_INODE *ino, lloff_t lbn)
{
    lloff_t block;
    struct ext4_extent_header *header;

    header = get_ext4_header(ino);
	//�õ�inode��Ӧ�����ݵĿ��
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