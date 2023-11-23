#include "fs.h"
#include "cmath"
#include "utility"
#include "tuple"

int INE5412_FS::fs_format()
{
    if (is_disk_mounted)
    {
        cout << "Disk is already mounted. Cannot format a mounted disk.\n";
        return 0;
    }

    // Formatting the superblock
    int disk_size = disk->size();
    int n_inodeblocks = std::ceil(disk_size * 0.1);

    union fs_block superblock;
    superblock.super = {FS_MAGIC, disk_size, n_inodeblocks, INODES_PER_BLOCK * n_inodeblocks};
    disk->write(0, superblock.data);

    // Formatting the inode blocks
    for (int i = 1; i < n_inodeblocks + 1; i++)
    {
        union fs_block fs_inodeblock;
        for (int j = 0; j < INODES_PER_BLOCK; j++)
        {
            fs_inodeblock.inode[j] = {0, 0, {0, 0, 0, 0}, 0};
        }
        disk->write(i, fs_inodeblock.data);
    }

    // Formatting the bitmap
    construct_bitmap(disk);

    return 1;
}


void INE5412_FS::fs_debug()
{
	union fs_block block;

	disk->read(0, block.data);

	cout << "superblock:\n";
	cout << "    " << (block.super.magic == FS_MAGIC ? "magic number is valid\n" : "magic number is invalid!\n");
 	cout << "    " << block.super.nblocks << " blocks\n";
	cout << "    " << block.super.ninodeblocks << " inode blocks\n";
	cout << "    " << block.super.ninodes << " inodes\n";

    for (int x = 1; x < block.super.ninodeblocks; ++x) {
        fs_block newblock;
        disk->read(x, newblock.data);
        for (int i = 0; i < INODES_PER_BLOCK; ++i) {
            if (newblock.inode[i].isvalid > 0) {
                cout << "inode " << (x - 1)*INODES_PER_BLOCK + i + 1 << ":\n";
                cout << "    size: " << newblock.inode[i].size << " bytes\n";
                cout << "    direct blocks: ";
                for (int j = 0; j < POINTERS_PER_INODE; ++j) {
                    if (newblock.inode[i].direct[j]) {
                        cout << newblock.inode[i].direct[j] << " ";
                    }
                }
                cout << "\n";
                if (newblock.inode[i].indirect) {
                    cout << "    indirect block: " << newblock.inode[i].indirect << "\n";
                    fs_block indirect;
                    disk->read(newblock.inode[i].indirect, indirect.data);
                    cout << "    indirect data blocks: ";
                    for (int k = 0; k < POINTERS_PER_BLOCK; ++k) {
                        if (indirect.pointers[k] > 0) {
                            cout << indirect.pointers[k] << " ";
                        }
                    }
                    cout << "\n";
                }
            }
        }
    }
}

int INE5412_FS::fs_mount()
{
    // Check if the disk is already mounted
    if (is_disk_mounted)
    {
        cout << "Disk is already mounted. Unable to mount again.\n";
        return 0;
    }

    // Read the superblock to check for a valid filesystem
    union fs_block fs_superblock;
    disk->read(0, fs_superblock.data);
    if (fs_superblock.super.magic != FS_MAGIC)
    {
        cout << "Invalid filesystem. Disk does not contain a valid filesystem.\n";
        return 0;
    }

    // Construct the bitmap
    construct_bitmap(disk);

    union fs_block block;

    // Iterate through inode blocks
    for (int i = 1; i <= fs_superblock.super.ninodeblocks; i++)
    {
        disk->read(i, block.data);

        // Iterate through inodes in the block
        for (int j = 0; j < INODES_PER_BLOCK; j++)
        {
            if (block.inode[j].isvalid)
            {
                // Set the inode block in the bitmap
                disk->bitmap[i] = 1;

                // Set direct pointers in the bitmap
                for (int k = 0; k < POINTERS_PER_INODE; k++)
                {
                    if (block.inode[j].direct[k] != 0)
                        disk->bitmap[block.inode[j].direct[k]] = 1;
                }

                // Set indirect pointer in the bitmap
                if (block.inode[j].indirect != 0)
                {
                    disk->bitmap[block.inode[j].indirect] = 1;

                    // Read the indirect block
                    disk->read(block.inode[j].indirect, block.data);

                    // Set pointers in the indirect block in the bitmap
                    for (int k = 0; k < POINTERS_PER_BLOCK; k++)
                    {
                        if (block.pointers[k] != 0)
                            disk->bitmap[block.pointers[k]] = 1;
                    }
                }
            }
        }
    }

    // Mark the disk as mounted
    is_disk_mounted = true;
    numinodes = std::ceil(disk->size() * 0.1) * INODES_PER_BLOCK;
    return 1;
}


int INE5412_FS::fs_create()
{
    if (!is_disk_mounted) {
        cout << "Disk is not mounted." << endl;
        return 0;
    }

    fs_block superblock;
    disk->read(0, superblock.data);
    for (int i = 0; i < superblock.super.ninodeblocks; ++i) {
        fs_block block;
        disk->read(i + 1, block.data);
        for (int j = 0; j < INODES_PER_BLOCK; ++j) {
            if (!block.inode[j].isvalid) {
                fs_inode newinode;
                newinode.isvalid = 1;
                newinode.size = 0;
                for (int k = 0; k < POINTERS_PER_INODE; ++k) {
                    newinode.direct[k] = 0;
                }
                newinode.indirect = 0;
                block.inode[j] = newinode;
                disk->write(i + 1, block.data);
                return (i*INODES_PER_BLOCK + j + 1);
            }
        }
    }
	return 0;
}

int INE5412_FS::fs_delete(int inumber)
{
    if (!is_disk_mounted) {
        cout << "Disk is not mounted." << endl;
        return 0;
    }

    if (inumber < 0 || inumber > numinodes + 1) {
        cout << "Invalid inode number." << endl;
        return 0;
    }

    auto block_and_inode = fs_getblock_and_inode(inumber);
    auto block = get<0>(block_and_inode);
    auto inode = get<1>(block_and_inode);
    auto blocknumber = get<2>(block_and_inode);

    inode.isvalid = false;
    inode.size = 0;

    inode.indirect = 0;
    disk->bitmap[inode.indirect] = 0;

    for (int i = 0; i < POINTERS_PER_INODE; ++i) {
        inode.direct[i] = 0;
        disk->bitmap[inode.direct[i]] = 0;
    }

    block.inode[(inumber - 1) % INODES_PER_BLOCK] = inode;
    disk->write(blocknumber, block.data);
	return 1;
}

tuple<INE5412_FS::fs_block, INE5412_FS::fs_inode, int> INE5412_FS::fs_getblock_and_inode(int inumber)
{
    int blocknumber = ((inumber - 1)/INODES_PER_BLOCK) + 1;
    int inodeposition = ((inumber - 1) % INODES_PER_BLOCK);

    fs_block block;
    disk->read(blocknumber, block.data);
    return {block, block.inode[inodeposition], blocknumber};
}

int INE5412_FS::fs_getsize(int inumber)
{
    if (!is_disk_mounted) {
        cout << "Disk is not mounted." << endl;
        return -1;
    }

    if (inumber < 0 || inumber > numinodes + 1) {
        cout << "Invalid inode number." << endl;
        return -1;
    }

    auto block_and_inode = fs_getblock_and_inode(inumber);
    auto inode = get<1>(block_and_inode);
    return inode.size;
}

int INE5412_FS::fs_read(int inumber, char *data, int length, int offset)
{
    if (!is_disk_mounted) {
        cout << "Disk is not mounted." << endl;
        return 0;
    }

    if (inumber < 0 || inumber > numinodes + 1) {
        cout << "Invalid inode number." << endl;
        return 0;
    }

    auto block_and_inode = fs_getblock_and_inode(inumber);
    auto inode = get<1>(block_and_inode);

    if (!inode.isvalid) {
        cout << "Inode is not valid." << endl;
        return 0;
    }

    if (offset < 0 || offset >= inode.size) {
        return 0;
    }

    int unread = min(length, inode.size - offset);
    int total_read = unread;
    while (unread) {
        int blocknumber = offset / Disk::DISK_BLOCK_SIZE;
        fs_block block_to_read;
        if (blocknumber < POINTERS_PER_INODE) {
            disk->read(inode.direct[blocknumber], block_to_read.data);
        } else {
            fs_block indirect;
            disk->read(inode.indirect, indirect.data);
            disk->read(indirect.pointers[blocknumber - POINTERS_PER_INODE], block_to_read.data);
        }

        int bytes_to_copy = min((int) Disk::DISK_BLOCK_SIZE, unread);
        for (int i = 0; i < bytes_to_copy; ++i, --unread, ++offset) {
            data[total_read - unread] = block_to_read.data[i];
        }
    }

	return total_read;
}

int INE5412_FS::fs_write(int inumber, const char *data, int length, int offset)
{
    if (!is_disk_mounted) {
        cout << "Disk is not mounted." << endl;
        return 0;
    }

    if (inumber < 0 || inumber > numinodes + 1) {
        cout << "Invalid inode number." << endl;
        return 0;
    }

    auto block_and_inode = fs_getblock_and_inode(inumber);
    auto block = get<0>(block_and_inode);
    auto inode = get<1>(block_and_inode);
    auto inodeblocknumber = get<2>(block_and_inode);

    if (!inode.isvalid) {
        cout << "Inode is not valid." << endl;
        return 0;
    }

    if (offset < 0 || offset >= inode.size) {
        return 0;
    }

    int actual_length = min(length, Disk::DISK_BLOCK_SIZE * (POINTERS_PER_INODE + POINTERS_PER_BLOCK) - offset);
    int total_written = 0;
    cout << actual_length << endl;
    while (total_written < actual_length) {
        int blocknumber = offset / Disk::DISK_BLOCK_SIZE;
        int blockposition = offset % Disk::DISK_BLOCK_SIZE;

        if (blocknumber < POINTERS_PER_INODE && inode.direct[blocknumber] == 0) { // if direct is empty
            int newblocknumber = allocate_new_block();
            if (!newblocknumber) {
                cout << total_written << endl;
                return total_written;
            } else {
                inode.direct[blocknumber] = newblocknumber;
            }
        } else if (blocknumber >= POINTERS_PER_INODE && inode.indirect == 0) {
            int newblocknumber = allocate_new_block();
            if (!newblocknumber) {
                cout << total_written << endl;
                return total_written;
            } else {
                inode.indirect = newblocknumber;
                // initialize indirect block's pointers
                fs_block indirect;
                for (int i = 0; i < POINTERS_PER_BLOCK; ++i) {
                    indirect.pointers[i] = 0;
                }
                disk->write(inode.indirect, indirect.data);
            }
        }

        fs_block block_to_write;
        if (blocknumber < POINTERS_PER_INODE) { // read direct block
            disk->read(inode.direct[blocknumber], block_to_write.data);
        } else { // read indirect block
            disk->read(inode.indirect, block_to_write.data);
            disk->read(block_to_write.pointers[blocknumber - POINTERS_PER_INODE], block_to_write.data);
        }

        int bytes_to_copy = min(Disk::DISK_BLOCK_SIZE - blockposition, actual_length - total_written);
        for (int i = 0; i < bytes_to_copy; ++i) {
            block_to_write.data[blockposition + i] = data[total_written + 1];
        }

        if (blocknumber < POINTERS_PER_INODE) { // write on direct block
            disk->write(inode.direct[blocknumber], block_to_write.data);
        } else { // write on indirect block
            disk->write(block_to_write.pointers[blocknumber - POINTERS_PER_INODE], block_to_write.data);
        }
        total_written += bytes_to_copy;
//        cout << total_written << endl;
    }

    inode.size = max(inode.size, offset + total_written);
    disk->write(inodeblocknumber, block.data);
    return total_written;
}

void INE5412_FS::construct_bitmap(Disk *disk)
{
    std::size_t bitmapSize = disk->size() + 1;
    
    // Initialize bitmap with all zeros
    disk->bitmap.assign(bitmapSize, 0);
    
    // Set the first element to 1 (used by the superblock)
    disk->bitmap[0] = 1;
}

int INE5412_FS::allocate_new_block()
{
    for (size_t i = 0; i < disk->bitmap.size(); ++i) {
        if (disk->bitmap[i] == 0) { // return first unoccupied block
            disk->bitmap[i] = 1;
            return i;
        }
    }
    return 0; // no blocks available
}