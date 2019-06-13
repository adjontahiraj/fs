#include "disk.h"
#include "fs.h"

//global variables for superblock and file descriptors
file_d f_des[MAX_FD];
super_block* super_head;

int make_fs(char *disk_name) {
    //error checking
    if (make_disk(disk_name) == -1) {
        return -1;
    }
    if (open_disk(disk_name) == -1) {
        return -1;
    }
    super_block *temp = new super_block();
    if (temp == nullptr) {
        return -1;
    }
    char buf[BLOCK_SIZE];
    memset(buf, 0, BLOCK_SIZE);
    set_first_superblock(buf,temp);
    if (block_write(0, buf) == -1) {
        return -1;
    }
    delete temp;
    close_disk();
    
    return 0;
}

int mount_fs(char *disk_name) {
    //error checking
    if (disk_name == nullptr) {
        return -1;
    }
    if (open_disk(disk_name) == -1) {
        return -1;
    }
    
    super_head = new super_block();

    char buf[BLOCK_SIZE];
    memset(buf, 0, BLOCK_SIZE);
    //error check
    if( block_read(0, buf) == -1){
        return -1;
    }
    int* buf_pointer = (int*)(&buf);
    super_head->num_files = *(buf_pointer++);
    super_head->num_descriptors = *(buf_pointer++);
    for (int i = 0; i < MAX_FILES; i++) {
        super_head->dir[i].used = *(buf_pointer++);
        super_head->dir[i].f_size = *(buf_pointer++);
        super_head->dir[i].next_block = *(buf_pointer++);
        super_head->dir[i].num_blocks = *(buf_pointer++);
        super_head->dir[i].num_descriptors = 0;
        snprintf(super_head->dir[i].file_name, MAX_FILENAME+1, "%s", buf+(24+(32*i)));
        buf_pointer += 4;
    }
    
    reset_descriptors();
    return 0;
}

int umount_fs(char *disk_name) {
    if (disk_name == nullptr) {
        return -1;
    }
    char buf[BLOCK_SIZE];
    int *buf_pointer;
    
    memset(buf, 0, BLOCK_SIZE);
    if (block_read(0, buf) == -1) {
        return -1;
    }
    memset(buf, 0, BLOCK_SIZE);
    
    buf_pointer = (int*)(&buf);
    *(buf_pointer++) = super_head->num_files;
    *(buf_pointer++) = 0;
    for (int i = 0; i < MAX_FILES; i++) {
        *(buf_pointer++) = super_head->dir[i].used; 
        *(buf_pointer++) = super_head->dir[i].f_size; 
        *(buf_pointer++) = super_head->dir[i].next_block; 
        *(buf_pointer++) = super_head->dir[i].num_blocks; 
        snprintf(buf+(24+(31*i)), MAX_FILENAME+1, "%s", super_head->dir[i].file_name);
        buf_pointer += 4;
    }
    block_write(0, buf);
    reset_descriptors();
    delete super_head;
    close_disk();
    return 0;
}

int fs_open(char *name) {
    int index = -1;
    for (int i = 0; i < MAX_FILES; i++) {
        if (strcmp(super_head->dir[i].file_name, name) == 0) {
            index = i;
        }
    }
    //error check
    if (index == -1) {
        return -1;
    }
    if (super_head->num_descriptors >= 32) {
        return -1;
    }

    //find open index in descriptor table
    int fd = -1;
    for (int i = 0; i < MAX_FD; i++) {
        if (f_des[i].free == 1) {
            f_des[i].free = 0;
            f_des[i].offset = 0;
            fd = i;
            break;
        }
    }
    //update descriptor and supreblock head
    if (fd == -1) {
        return -1;
    }
    super_head->dir[index].num_descriptors++;
    f_des[fd].file = index;
    super_head->num_descriptors = super_head->num_descriptors +1;
    return fd;
}


int fs_close(int fildes) {
    if (fildes < 0 || fildes >= MAX_FD || f_des[fildes].free == 1) {
        return -1;
    }
    //decrease the number of descriptors
    super_head->num_descriptors = super_head->num_descriptors -1;
    //set the index in descriptor arr free
    f_des[fildes].free = 1;
    //update superblock head
    super_head->dir[f_des[fildes].file].num_descriptors-= 1;
    f_des[fildes].file = -1;
    f_des[fildes].offset = 0;
    return 0;
}

int fs_create(char *name) {
    int index = -1;
    for (int i = 0; i < MAX_FILES; i++) {
        if (strcmp(super_head->dir[i].file_name, name) == 0) {
            index = i;
        }
    }
    if (index != -1 || strlen(name) > (MAX_FILENAME) || strlen(name) == 0 || super_head->num_files >= MAX_FILES) {
        return -1;
    }
    return find_file_spot(name);
}

int fs_delete(char *name) {
    int index = -1;
    for (int i = 0; i < MAX_FILES; i++) {
        if (strcmp(super_head->dir[i].file_name, name) == 0) {
            index = i;
        }
    }
    if (index == -1 || super_head->dir[index].num_descriptors != 0) {
        return -1;
    }
    
    int b_index = super_head->dir[index].next_block;
    while (b_index != -1) {
        char buf[BLOCK_SIZE];
        memset(buf, 0, BLOCK_SIZE);
        block_write(b_index, buf);
        b_index = clear_data(b_index);
    }
    
    super_head->num_files= super_head->num_files -1;
    
    super_head->dir[index].used = 0;
    memset(super_head->dir[index].file_name, 0, MAX_FILENAME+1);
    super_head->dir[index].f_size = 0;
    super_head->dir[index].next_block = -1;
    super_head->dir[index].num_blocks = 0;
    super_head->dir[index].num_descriptors = 0;
    
    return 0;
}

int fs_read(int fildes, void *buf, size_t nbyte) {
    if (fildes < 0 || fildes >= MAX_FD || f_des[fildes].free == 1) {
        return -1;
    }
    
    int read = nbyte;
    int offset = f_des[fildes].offset;
    int index = f_des[fildes].file;
    
    finfo *file = &(super_head->dir[index]);
    
    int next_block = file->next_block;
    int f_size = file->f_size;
    
    if (next_block == -1) {
        return 0;
    }
    if (offset >= (BLOCK_SIZE*BLOCK_SIZE)) {
        return 0;
    }
    
    while (offset >= BLOCK_SIZE) {
        int tmp_block = get_next(next_block);
        offset -= BLOCK_SIZE;
        if (tmp_block == -1) {
            return 0;
        }
        next_block = tmp_block;
    }
    char *buf_index = (char*)buf;
    
    while (read > 0 && f_des[fildes].offset < f_size) {
        char block[BLOCK_SIZE];
        memset(block, 0, BLOCK_SIZE);
        block_read(next_block, block);
        for (int i = offset; i < BLOCK_SIZE; i++) {
            if (f_des[fildes].offset >= file->f_size) {
                return nbyte-read;
            }
            if (read == 0) {
                break;
            }
            *(buf_index++) = block[i];
            read = read -1;
            f_des[fildes].offset++;
        }
        
        offset = 0;
        if (f_des[fildes].offset >= file->f_size) {
            break;
        }
        if (read > 0) {
            next_block = get_next(next_block);
            if (next_block == -1) {
                break;
            }
        }
    }
    return (nbyte - read);
}

int fs_write(int fildes, void *buf, size_t nbyte) {
    if (fildes < 0 || fildes >= MAX_FD || nbyte < 0 || f_des[fildes].free == 1) {
        return -1;
    }
    if (nbyte == 0) {
        return 0;
    }
    int written = nbyte;
    int offset = f_des[fildes].offset;
    int index = f_des[fildes].file;
    
    finfo *file = &(super_head->dir[index]);
    
    
    int block_to_write = file->next_block;
    int last_set = file->f_size;
    
    if (block_to_write == -1) {
        file->next_block = -1;

        for (int i = 0; i < 4; i++) {
            char buf[BLOCK_SIZE];
            int *buf_pointer;
            memset(buf, 0, BLOCK_SIZE);
            block_read(i+1, buf);
            buf_pointer = (int*)(&buf[0]);
            for (int x = 1; x <= (BLOCK_SIZE/4); x++) {
                if (*buf_pointer == 0) {
                    *buf_pointer = -1;
                    block_write(i+1, buf);
                    file->next_block = ((BLOCK_SIZE/4)*i + x + 4);
                    break;
                }
                buf_pointer++;
            }
        }



        block_to_write = file->next_block;
        if (file->next_block == -1) {
            return 0;
        }
        file->num_blocks++;
    }
    
    if (offset >= (BLOCK_SIZE*BLOCK_SIZE+1)) {
        return 0;
    }
    while (offset >= BLOCK_SIZE) {
        int tmp_block = get_next(block_to_write);
        offset -= BLOCK_SIZE;
        if (tmp_block == -1) {
            tmp_block = find_free_block(block_to_write);
            if (tmp_block == -1) {
                return 0;
            }
            file->num_blocks++;
        }
        block_to_write = tmp_block;
    }
    char *buf_index = (char*)buf;
    while (written > 0) {
        
        char block[BLOCK_SIZE];
        memset(block, 0, BLOCK_SIZE);
        block_read(block_to_write, block);
        for (int i = offset; i < BLOCK_SIZE; i++) {
            if (written == 0) {
                break;
            }
            block[i] = *(buf_index++);
            f_des[fildes].offset++;
            written = written -1;
            if (last_set > f_des[fildes].offset) {
                last_set-=1;
            } else {
                file->f_size++;
            }
        }
        
        block_write(block_to_write, block);
        offset = 0;
        
        if (written > 0) {
            int block_tmp = get_next(block_to_write);
            if (block_tmp == -1) {
                block_tmp = find_free_block(block_to_write);
                if (block_tmp == -1) {
                    break;
                }
                file->num_blocks++;
            }
            block_to_write = block_tmp;
        }
    }
    
    return (nbyte - written);
    
}

int fs_get_filesize(int fildes) {
    if (fildes < 0 || fildes >= MAX_FD || f_des[fildes].free == 1) {
        return -1;
    }
    
    return (super_head->dir[f_des[fildes].file]).f_size;
}

int fs_lseek(int fildes, off_t offset) {
    if (fildes < 0 || fildes >= MAX_FD || f_des[fildes].free == 1) {
        return -1;
    }
    
    int index = f_des[fildes].file;
    finfo *file = &(super_head->dir[index]);
    
    if (offset < 0 || offset > file->f_size) {
        return -1;
    }
    
    f_des[fildes].offset = offset;
    return 0;
}

int fs_truncate(int fildes, off_t length) {
    if (fildes < 0 || fildes >= MAX_FD || f_des[fildes].free == 1) {
        return -1;
    }
    int index = f_des[fildes].file;
    finfo *file = &(super_head->dir[index]);
    if (length > file->f_size || length < 0) {
        return -1;
    }
    
    int offset = length;
    //need to truncate the next block
    int next_block = file->next_block;
    
    while (offset >= BLOCK_SIZE) {
        int tmp_block = get_next(next_block);
        offset -= BLOCK_SIZE;
        if (tmp_block != -1) {
            next_block = tmp_block;
        } else {
            return -1;
        }
    }
    
    while (next_block != -1) {
        char block[BLOCK_SIZE];
        memset(block, 0, BLOCK_SIZE);
        block_read(next_block, block);
        if (offset > 0) {
            for (int i = offset; i < BLOCK_SIZE; i++) {
                block[i] = '\0';
            }
        } else {
            for (int i = 0; i < BLOCK_SIZE; i++) {
                block[i] = '\0';
            }
        }
        
        block_write(next_block, block);
        int tmp_block = next_block;
        next_block = get_next(next_block);
        
        if (offset > 0) {
            char buf[BLOCK_SIZE];
            int *buf_pointer;
            memset(buf, 0, BLOCK_SIZE);
            block_read((tmp_block-5)/((BLOCK_SIZE/4) + 1), buf);
            buf_pointer = (int*)(&buf[0]);
            buf_pointer += (tmp_block-5)%(BLOCK_SIZE/4);
            *buf_pointer = -1;
            block_write((tmp_block-5)/((BLOCK_SIZE/4)+1), buf);
        } else {
            if (offset == 0) {
                file->next_block = -1;
            }
            file->num_blocks--;
            clear_data(tmp_block);
        }
        
        offset = -1;
    }
    
    file->f_size = length;
    if (f_des[fildes].offset > file->f_size) {
        f_des[fildes].offset = file->f_size;
    }
    
    return 0;
}

int find_file_spot(char *name) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (super_head->dir[i].used == 0) {
            super_head->num_files++;
            super_head->dir[i].used = 1;
            super_head->dir[i].f_size = 0;
            super_head->dir[i].next_block = -1;
            super_head->dir[i].num_blocks = 0;
            super_head->dir[i].num_descriptors = 0;
            snprintf(super_head->dir[i].file_name, MAX_FILENAME+1, "%s", name);
            return 0;
        }
    }
    return -1;
}


void reset_descriptors() {
    for (int i = 0; i < MAX_FD; i++) {
        f_des[i].free= 1;
        f_des[i].file = -1;
        f_des[i].offset = 0;
    }
}

