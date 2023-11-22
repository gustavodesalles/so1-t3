#include "fs.h"
#include "cmath"

int INE5412_FS::fs_format()
{
    if (is_disk_mounted)
    {
        cout << "Disk is already mounted. Cannot format a mounted disk.\n";
        return 0;
    }

    // Formatting the superblock
    int disk_size = disk->size();
    int n_inodes = std::ceil(disk_size * 0.1) + 1;

    union fs_block fs_superblock;
    fs_superblock.super = {FS_MAGIC, disk_size, n_inodes, INODES_PER_BLOCK * n_inodes};
    disk->write(0, fs_superblock.data);

    // Formatting the inode blocks
    for (int i = 1; i < n_inodes + 1; i++)
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
            if (newblock.inode[i].isvalid) {
                cout << "inode " << i << ":\n";
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
    return 1;
}


int INE5412_FS::fs_create()
{
	return 0;
}

int INE5412_FS::fs_delete(int inumber)
{
	return 0;
}

int INE5412_FS::fs_getsize(int inumber)
{
	return -1;
}

int INE5412_FS::fs_read(int inumber, char *data, int length, int offset)
{
	return 0;
}

int INE5412_FS::fs_write(int inumber, const char *data, int length, int offset)
{
	return 0;
}

void INE5412_FS::construct_bitmap(Disk *disk)
{
    std::size_t bitmapSize = disk->size() + 1;
    
    // Initialize bitmap with all zeros
    disk->bitmap.assign(bitmapSize, 0);
    
    // Set the first element to 1 (used by the superblock)
    disk->bitmap[0] = 1;
}

