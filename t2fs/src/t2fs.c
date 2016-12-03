//
//  t2fs.c
//  t2fs
//
//  Created by Henrique Valcanaia on 14/11/16.
//  Copyright © 2016 SISOP. All rights reserved.
//

#include <assert.h>
#include <stdio.h>
#include "../include/t2fs.h"

static int INITIALIZED = 0;
#define INIT()\
do {\
if (!INITIALIZED) {\
init_t2fs();\
INITIALIZED = 1;\
}\
} while(0)

// ------- Definido na especificação -------
#define ID "T2FS"
#define VERSION 0x7E02
#define DISK_NAME "t2fs_disk.dat"

#define OPENED_FILES_LIMIT 20
static FILE2 opened_files[OPENED_FILES_LIMIT];
static int pointer_opened_files[OPENED_FILES_LIMIT];

#define T2FS_RECORD_SIZE 64 // conforme email do Carissimi
#define ROOT_INODE_BLOCK_INDEX 0
// -----------------------------------------

struct t2fs_superbloco* superblock;

// MARK: - Private functions
static int is_asbsolute_path(char* path) {
    return path[0] == '/';
}

void show_superblock_info(struct t2fs_superbloco* superblock) {
    printf("ID: %s\n", superblock->id);
    printf("Version: 0x%x\n", superblock->version);
    printf("Superblock Size: %hu\n", superblock->superblockSize);
    printf("Free Blocks Bitmap Size: %hu\n", superblock->freeBlocksBitmapSize);
    printf("Free inode Bitmap Size: %hu\n", superblock->freeInodeBitmapSize);
    printf("inode Area Size: %hu\n", superblock->inodeAreaSize);
    printf("BlockSize: %hu\n", superblock->blockSize);
    printf("Disksize: %u\n", superblock->diskSize);
}

void show_bitmap_info(int handle) {
    int i=0;
    for(; i < SECTOR_SIZE * superblock->freeInodeBitmapSize; i++) {
        printf("bit %i -> %i\n",i, getBitmap2(handle, i));
    }
}

void show_inode_info(int index) {
    char buffer[SECTOR_SIZE];
    
    // superblock + bitmapINODE + bitmapDADOS
    int inodeAreaSectorOffset = superblock->superblockSize + superblock->freeInodeBitmapSize + superblock->freeBlocksBitmapSize;
    int inodeIndexOffset = index*(sizeof(struct t2fs_inode));
    if(read_sector(inodeAreaSectorOffset, buffer) != 0) {
        printf("ERRO LENDO SETOR: %i\n", inodeAreaSectorOffset);
        exit(ERROR);
    }
    // int i =0;
    // printf("BUFFER\n");
    // for (; i<256; i++) {
    //     printf("%c - ",buffer[i]);
    // }
    printf("\nEND_OF_BUFFER\n");
    struct t2fs_inode inode;
    memcpy(&inode.dataPtr[0],buffer, 4);
    memcpy(&inode.dataPtr[1],buffer+4, 4);
    memcpy(&inode.singleIndPtr, buffer+8, 4);
    memcpy(&inode.doubleIndPtr, buffer+12, 4);
    printf("show_inode\n");
    printf("%i\n", inode.dataPtr[0]);
    printf("%i\n", inode.dataPtr[1]);
    printf("%i\n", inode.singleIndPtr);
    printf("%i\n", inode.doubleIndPtr);
    return;
}

struct t2fs_inode* get_inode(int index) {
    struct t2fs_inode* inode = (struct t2fs_inode*) malloc(sizeof(struct t2fs_inode));
    char buffer[SECTOR_SIZE];
    int inodeAreaSectorOffset = 1 + 1 + 1;//superblock+bitmapINODE+bitmapDADOS
    int inodeIndexOffset = index*(sizeof(struct t2fs_inode));
    
    if(read_sector(inodeAreaSectorOffset, buffer) != 0) {
        printf("ERRO LENDO SETOR: %i\n", inodeAreaSectorOffset);
        exit(ERROR);
    }
    
    memcpy(inode->dataPtr,buffer+inodeIndexOffset, 8);
    memcpy(&inode->singleIndPtr, buffer+8+inodeIndexOffset, 4);
    memcpy(&inode->doubleIndPtr, buffer+12+inodeIndexOffset, 4);
    
    // printf("get_inode: %i\n", index);
    // printf("%i\n", inode->dataPtr[0]);
    // printf("%i\n", inode->dataPtr[1]);
    // printf("%i\n", inode->singleIndPtr);
    // printf("%i\n", inode->doubleIndPtr);
    return inode;
}

typedef int BLOCK_TYPE;
#define BLOCK_TYPE_DATA 0
#define BLOCK_TYPE_INODE 1
int data_block_sector_offset;

struct t2fs_record* read_block(int index, BLOCK_TYPE block_type){
    struct t2fs_record *record = (struct t2fs_record*) malloc(T2FS_RECORD_SIZE);
    char buffer[SECTOR_SIZE];
    
    int data_block_sector_offset_to_read = data_block_sector_offset;
    if (block_type == BLOCK_TYPE_DATA) {
        data_block_sector_offset_to_read += superblock->inodeAreaSize;
    }
    int block_sector_offset = index * superblock->blockSize;
    
    int sector = data_block_sector_offset_to_read + block_sector_offset;
    
    if(read_sector(sector, buffer) != 0) {
        printf("Erro lendo!\n");
        printf("Tipo do bloco: %d(%s)\n", block_type, ((block_type == BLOCK_TYPE_DATA) ? "Dados" : "Inode"));
        printf("Offset \n");
        printf("Erro lendo!\n");
        exit(ERROR);
    }
    
    int i = 0;
    printf("\n-------------------------");
    for (; i < SECTOR_SIZE; i++) {
        printf("%d: %c\n", i, buffer[i]);
    }
    printf("\n-------------------------");
    
    memcpy(&record->TypeVal,        buffer,    1);
    memcpy(&record->name,           buffer+1,  31);
    memcpy(&record->blocksFileSize, buffer+33, 4);
    memcpy(&record->bytesFileSize,  buffer+37, 4);
    memcpy(&record->inodeNumber,    buffer+41, 4);
    
    printf("show_record\n");
    printf("%hhu\n", record->TypeVal);
    printf("%s\n", record->name);
    printf("%d\n", record->blocksFileSize);
    printf("%d\n", record->bytesFileSize);
    printf("%i\n", record->inodeNumber);
    
    return record;
}

static void init_t2fs() {
    char buffer[SECTOR_SIZE];
    
    if (read_sector(0, buffer)) {
        printf("Erro ao ler o superbloco. O arquivo %s esta no mesmo caminho do executavel?\n", DISK_NAME);
        exit(EXIT_FAILURE);
    }
    
    superblock = malloc(sizeof(*superblock));
    
    /* offset 0 bytes */
    strncpy(superblock->id, buffer, 4);
    if (strncmp(superblock->id, ID, 4)) {
        printf("Sistemas de arquivos desconhecido!\nEsperado: %s\nEncontrado: %s\n", ID, superblock->id);
        exit(EXIT_FAILURE);
    }
    
    superblock->version = *((WORD *)(buffer + 4));
    if (superblock->version != 0x7E02) {
        printf("Versão do sistema de arquivos não suportada!\nEsperado: %d\nEncontrado: 0x%hu\n", VERSION, superblock->version);
        exit(EXIT_FAILURE);
    }
    
    superblock->superblockSize = *((WORD *)(buffer + 6));
    superblock->freeBlocksBitmapSize = *((WORD *)(buffer + 8));
    superblock->freeInodeBitmapSize = *((WORD *)(buffer + 10));
    superblock->inodeAreaSize = *((WORD *)(buffer + 12));
    superblock->blockSize = *((DWORD *)(buffer + 14));
    superblock->diskSize = *((DWORD *)(buffer + 16));
    
    show_superblock_info(superblock);
    
    data_block_sector_offset = superblock->superblockSize + superblock->freeBlocksBitmapSize + superblock->freeInodeBitmapSize;
    
    // printf("BITMAP iNODE\n");
    // show_bitmap_info(BITMAP_INODE);
    // printf("BITMAP DADOS\n");
    // show_bitmap_info(BITMAP_DADOS);
    // printf("INODE 0\n");
    // struct t2fs_inode* inode = read_inode(1);
    // printf("%s\n", (inode->dataPtr[0]));
    // printf("%s\n", (inode->dataPtr[1]));
    // show_inode_info(0);
    struct t2fs_inode *inode = get_inode(3);
    // // printf("%i\n", inode->dataPtr[0]);
    // struct t2fs_record *record = read_block(inode->dataPtr[0]);
    
    // inode = get_inode(1);
    
}

int open_entry() {
    return -1;
}

// MARK: libt2fs functions

// Informa a identificação dos desenvolvedores do T2FS.
int identify2 (char *name, int size) {
    printf("240501 - Henrique Valcanaia\n243666 - Pietro Degrazia");
    return 0;
}

// Função usada para criar um novo arquivo no disco.
FILE2 create2 (char *filename) {
    FILE2 file = ERROR;
    INIT();
    
    return file;
}

// Função usada para remover (apagar) um arquivo do disco.
int delete2 (char *filename) {
    return ERROR;
}

// Função que abre um arquivo existente no disco.
FILE2 open2 (char *filename) {
//    return ERROR;
    
    char* string;
    string = strdup(filename);
    if (string != NULL) {
        char* tofree;
        char* token;
        
        tofree = string;
        while ((token = strsep(&string, "/")) != NULL) {
            if (strcmp(token, "") == 0) {
                // raiz
                printf("raiz\n");
                struct t2fs_record* entry = (struct t2fs_record*) malloc(T2FS_RECORD_SIZE);
                entry = read_block(ROOT_INODE_BLOCK_INDEX, BLOCK_TYPE_INODE);
            } else {
                printf("%s\n", token);
            }
        }
        
        free(tofree);
    }
    
    
    // olhar todos pq pode querer fechar um arquivo do meio do array
    int index = 0;
    struct t2fs_record record;
    opened_files[index] = record.inodeNumber;
    
    return record.inodeNumber;
}

// Função usada para fechar um arquivo.
int close2 (FILE2 handle) {
    return ERROR;
}

// Função usada para realizar a leitura de uma certa quantidade de bytes (size) de um arquivo.
int read2 (FILE2 handle, char *buffer, int size) {
    return ERROR;
}

// Função usada para realizar a escrita de uma certa quantidade de bytes (size) de um arquivo.
int write2 (FILE2 handle, char *buffer, int size) {
    return ERROR;
}

// Função usada para truncar um arquivo. Remove do arquivo todos os bytes a partir da posição atual do contador de posição (current pointer), inclusive, até o seu final.
int truncate2 (FILE2 handle) {
    return ERROR;
}

// Altera o contador de posição (current pointer) do arquivo.
int seek2 (FILE2 handle, DWORD offset) {
    return ERROR;
}

// Função usada para criar um novo diretório.
int mkdir2 (char *pathname) {
    return ERROR;
}

// Função usada para remover (apagar) um diretório do disco.
int rmdir2 (char *pathname) {
    return ERROR;
}

// Função que abre um diretório existente no disco.
DIR2 opendir2 (char *pathname) {
    DIR2 dir = ERROR;
    return dir;
}

// Função usada para ler as entradas de um diretório.
int readdir2 (DIR2 handle, DIRENT2 *dentry) {
    return ERROR;
}

// Função usada para fechar um diretório.
int closedir2 (DIR2 handle) {
    return ERROR;
}

/*
 * Given an absolute pathname, return the record of the file/subdir, if any
 * Else return NULL
 *
 * For now works only in the root directory, so if you pass "/home/teste" it
 * will be interpreted as "/home"
 */
//#define PWD_BUFFER_SIZE 256
//#define RECORD_SIZE sizeof(struct t2fs_record)
//
//static struct t2fs_record* get_record(char* pathname) {
//    assert(pathname && is_absolute_path(pathname) && strlen(pathname) < PWD_BUFFER_SIZE);
//    assert(SECTOR_SIZE % RECORD_SIZE == 0);
//
//    /* There is no record for the root directory */
//    if (strlen(pathname) == 1)
//        return NULL;
//    struct t2fs_record *records;
//    size_t size = alloc_max_records(&records);
//    size = read_records(records, size, superblock->RootSectorStart, superblock->DataSectorStart);
//    assert(size > 0);
//
//    // TODO
//    char *want = leftmost(pathname);
//    size_t i = 0;
//    while (i < size && strncmp(records[i].name, want, MAX_FILE_NAME_SIZE))
//        i++;
//    if (i >= size) {
//        free(records);
//        free(want);
//        return NULL;
//    } else {
//        struct t2fs_record *ans = malloc(sizeof(*ans));
//        *ans = records[i];
//        free(records);
//        free(want);
//        return ans;
//    }
//}
//
//
//FILE2 open_file(char *filename, BYTE type) {
//    if (!filename) {
//        printf("Unknown filename!\n");
//        return -1;
//    }
//
//    if (is_asbsolute_path(filename)) {
//        struct t2fs_record *record = get_record(filename);
//        if (!record) {
//            printf("%s not found in disk\n", filename);
//            return -1;
//        }
//        if (record->TypeVal != type) {
//            printf("%s is not a %s\n", is_asbsolute_path, type == TYPE_DIR ? "dir" : "file");
//            return -1;
//        }
//        printf("%s found in disk, adding to FT w/ fd %d\n", filename, CT);
//
//        struct file_table *e = malloc(sizeof(*e));
//        e->fd = CT++;
//        e->record = *record;
//        e->offset = 0;
//        HASH_ADD_INT(FT, fd, e);
//
//        return e->fd;
//    } else {
//        printf("Relative paths not implemented\n");
//        return -1;
//    }
//}
