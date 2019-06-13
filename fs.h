#define MAX_FILES       64
#define MAX_FD          32
#define MAX_FILENAME    15
#define BLOCK


#include "stdio.h"
#include <cstring>
#include <iostream>
#include <cstdlib>

//struct for file descriptors
struct file_d{
    int free = 1;
    int file = -1;
    int offset = 0;
};


//struct to hold the info for a file
struct finfo {
    int used;
    char file_name[MAX_FILENAME+1];
    int f_size;
    int next_block;
    int num_blocks;
    int num_descriptors;
};

//struct for superblock
struct super_block {
    int num_files;
    int num_descriptors;
    finfo dir[MAX_FILES];
};

int make_fs(char *disk_name);
int mount_fs(char *disk_name); 
int umount_fs(char *disk_name); 
int fs_open(char *name);
int fs_close(int fildes);
int fs_create(char *name);
int fs_delete(char *name);
int fs_read(int fildes, void *buf, size_t nbyte);
int fs_write(int fildes, void *buf, size_t nbyte);
int fs_get_filesize(int fildes);
int fs_lseek(int fildes, off_t offset);
int fs_truncate(int fildes, off_t length);

// Helper functions.
int find_file_spot(char *name);

void reset_descriptors();

int get_next(int current_block) {
    char buf[BLOCK_SIZE];
    int *buf_pointer;
    memset(buf, 0, BLOCK_SIZE);
    block_read((current_block-5)/1024+1, buf);
    buf_pointer = (int*)(&buf);
    buf_pointer += (current_block-5)%1024;
    return *buf_pointer;
}

int find_free_block(int current_block) {
    for (int i = 0; i < 4; ++i) {
        char buf[BLOCK_SIZE];
        int *buf_pointer;
        memset(buf, 0, BLOCK_SIZE);
        block_read(i+1, buf);
        buf_pointer = (int*)(&buf[0]);
        for (int j = 1; j <= 1024; ++j) {
            if (*buf_pointer == 0) {
                *buf_pointer = -1;
                block_write(i+1, buf);
                memset(buf, 0, BLOCK_SIZE);
                block_read((current_block-5)/1024+1, buf);
                buf_pointer = (int*)(&buf);
                buf_pointer += (current_block-5)%1024;
                
                *buf_pointer = (1024*i + j + 4);
                
                block_write((current_block-5)/1024+1, buf);
                return (1024*i + j + 4);
            }
            buf_pointer++;
        }
    }
    return -1;
}

void set_first_superblock(char *buf, super_block *temp) {
    int *b;
    b = (int*)(&buf[0]);
    *(b++) = temp->num_files;
    *(b++) = temp->num_descriptors;
    for (int i = 0; i < MAX_FILES; ++i) {
        *(b++) = 0;
        *(b++) = 0; 
        *(b++) = -1; 
        *(b++) = 0; 
        snprintf(&buf[(24+(32*i))], MAX_FILENAME+1, "000000000000000");
        b += 4;
    }
}

int clear_data(int current_block) {
    char buf[BLOCK_SIZE];
    int *buf_pointer;
    memset(buf, 0, BLOCK_SIZE);
    block_read((current_block-5)/1024+1, buf);
    buf_pointer = (int*)(&buf[0]);
    buf_pointer += (current_block-5)%1024;
    int next_block = *buf_pointer;
    *buf_pointer = 0;
    block_write((current_block-5)/1024+1, buf);
    return next_block;
}

