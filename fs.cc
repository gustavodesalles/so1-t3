#include "fs.h"
#include "cmath"
#include "utility"
#include "tuple"
#include <string.h>

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
    // Check if the file system is mounted
    if (!is_disk_mounted)
    {
        cout << "ERROR: Disk is not mounted.\n";
        return 0;
    }

    // Calculate the inode block number and the index within the block
    int blockNumber = 1 + inumber / INODES_PER_BLOCK;
    int indexInBlock = inumber % INODES_PER_BLOCK;

    // Read the inode block
    union fs_block block;
    disk->read(blockNumber, block.data);

    // Get the specific inode from the block
    fs_inode inode = block.inode[indexInBlock];

    // Check if the inode is valid
    if (!inode.isvalid)
    {
        cout << "ERROR: Invalid inode.\n";
        return 0;
    }

    // Check if the offset is within the file size
    if (offset >= inode.size)
    {
        return 0; // Offset exceeds file size, nothing to read
    }

    // Adjust the length to be read to avoid exceeding the file size
    if (offset + length > inode.size)
    {
        length = inode.size - offset;
    }

    int total_read = 0; // Total bytes read

    while (total_read < length)
    {
        int block_rel = offset / Disk::DISK_BLOCK_SIZE;
        int pos_in_block = offset % Disk::DISK_BLOCK_SIZE;
        int size_to_read = min(Disk::DISK_BLOCK_SIZE - pos_in_block, length - total_read);

        int physical_block;

        if (block_rel < POINTERS_PER_INODE)
        {
            // Direct block
            physical_block = inode.direct[block_rel];
        }
        else
        {
            // Indirect block
            physical_block = get_indirect_block(inode, block_rel);
        }

        if (physical_block == 0)
        {
            break; // No more data to read
        }

        union fs_block data_block;
        disk->read(physical_block, data_block.data);
        memcpy(data + total_read, data_block.data + pos_in_block, size_to_read);

        total_read += size_to_read;
        offset += size_to_read;
    }

    return total_read;
}

int INE5412_FS::get_indirect_block(const fs_inode &inode, int block_rel)
{
    if (inode.indirect == 0)
    {
        return 0; // Indirect block not allocated, no more data to read
    }

    union fs_block indirect_block;
    disk->read(inode.indirect, indirect_block.data);
    return indirect_block.pointers[block_rel - POINTERS_PER_INODE];
}


int INE5412_FS::fs_write(int inumber, const char *data, int length, int offset)
{
    // Check if the file system is mounted
    if (!is_disk_mounted)
    {
        cout << "ERROR: Disk is not mounted.\n";
        return 0;
    }

    // Calculate the inode block number and the index within the block
    int blockNumber = 1 + inumber / INODES_PER_BLOCK;
    int indexInBlock = inumber % INODES_PER_BLOCK;

    // Read the inode block
    union fs_block block;
    disk->read(blockNumber, block.data);

    // Get the specific inode from the block
    fs_inode &inode = block.inode[indexInBlock];

    // Check if the inode is valid
    if (!inode.isvalid)
    {
        cout << "ERROR: Invalid inode.\n";
        return 0;
    }

    int total_written = 0;

    while (total_written < length)
    {
        // Calculate the relative block, position in the block, and size to write
        int block_rel = (offset + total_written) / Disk::DISK_BLOCK_SIZE;
        int pos_in_block = (offset + total_written) % Disk::DISK_BLOCK_SIZE;
        int size_to_write = min(Disk::DISK_BLOCK_SIZE - pos_in_block, length - total_written);

        // Get the physical block based on the relative block
        int physical_block;
        if (block_rel < POINTERS_PER_INODE)
        {
            // Direct block
            physical_block = handle_direct_block(inode, block_rel);
        }
        else
        {
            // Indirect block
            physical_block = handle_indirect_block(inode, block_rel);
        }

        // Read the data block, write the data, and update the disk
        union fs_block data_block;
        disk->read(physical_block, data_block.data);
        memcpy(data_block.data + pos_in_block, data + total_written, size_to_write);
        disk->write(physical_block, data_block.data);

        total_written += size_to_write;
    }

    // Update the inode size if necessary
    if (inode.size < offset + total_written)
    {
        inode.size = offset + total_written;
        disk->write(blockNumber, block.data);
    }

    return total_written;
}

int INE5412_FS::handle_direct_block(fs_inode &inode, int block_rel)
{
    int physical_block = inode.direct[block_rel];

    if (physical_block == 0)
    {
        // Allocate a new block if necessary
        physical_block = allocate_new_block();
        if (physical_block == 0)
        {
            // Disk is full
            return 0;
        }

        inode.direct[block_rel] = physical_block;
    }

    return physical_block;
}

int INE5412_FS::handle_indirect_block(fs_inode &inode, int block_rel)
{
    if (inode.indirect == 0)
    {
        inode.indirect = allocate_new_block();
        if (inode.indirect == 0)
        {
            // Disk is full
            return 0;
        }

        // Initialize all pointers to 0
        union fs_block indirect_block;
        memset(indirect_block.pointers, 0, sizeof(indirect_block.pointers));
        disk->write(inode.indirect, indirect_block.data);
    }

    // Read the indirect block and get the physical block
    union fs_block indirect_block;
    disk->read(inode.indirect, indirect_block.data);
    int physical_block = indirect_block.pointers[block_rel - POINTERS_PER_INODE];

    if (physical_block == 0)
    {
        // Allocate a new block if necessary
        physical_block = allocate_new_block();
        if (physical_block == 0)
        {
            // Disk is full
            return 0;
        }

        indirect_block.pointers[block_rel - POINTERS_PER_INODE] = physical_block;
        disk->write(inode.indirect, indirect_block.data);
    }

    return physical_block;
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