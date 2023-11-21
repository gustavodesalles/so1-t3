#include "fs.h"
#include "cmath"

int INE5412_FS::fs_format()
{
    fs_block newblock;

    // Reservar dez por cento para inodes, arr. para cima
    int tamanho_blocos_inodes = (int) ceil(disk->size() * 0.1);

    // Liberar tabela de inodes (blocos depois de 0)
    for (int i = 1; i < tamanho_blocos_inodes; ++i) {
        for (int j = 0; j < INODES_PER_BLOCK; ++j) {
            newblock.inode[j].isvalid = 0; // nenhum dos inodes foi "criado" no disco formatado
        }
        disk->write(i, newblock.data); // não tenho certeza
    }

    //Preencher o superbloco
    newblock.super.magic = FS_MAGIC;
    newblock.super.ninodeblocks = tamanho_blocos_inodes;
    newblock.super.ninodes = tamanho_blocos_inodes * INODES_PER_BLOCK;
    newblock.super.nblocks = disk->size();

    disk->write(0, newblock.data);
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
	return 0;
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
