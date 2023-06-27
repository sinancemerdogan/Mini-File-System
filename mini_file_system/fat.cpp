#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <cassert>
#include <list>
#include <cstdlib>
#include "fat.h"
#include "fat_file.h"
#include <unistd.h>
#include <sys/types.h>


/**
 * Write inside one block in the filesystem.
 * @param  fs           filesystem
 * @param  block_id     index of block in the filesystem
 * @param  block_offset offset inside the block
 * @param  size         size to write, must be less than BLOCK_SIZE
 * @param  buffer       data buffer
 * @return              written byte count
 */
int mini_fat_write_in_block(FAT_FILESYSTEM *fs, const int block_id, const int block_offset, const int size, const void * buffer) {
	assert(block_offset >= 0);
	assert(block_offset < fs->block_size);
	assert(size + block_offset <= fs->block_size);

	int written = 0;

	//Open the file system file
	FILE* fat_fd = fopen(fs->filename, "rb+");
	if (fat_fd == NULL) {
		perror("Cannot write block to file");
		return false;
	}
	
	//Determine seek position	
	int position = (block_id * fs->block_size) + block_offset;
	fseek(fat_fd, position, SEEK_SET);	
        fwrite(buffer, size, 1, fat_fd);
        written = size;
	fclose(fat_fd);
	return written;
}

/**
 * Read inside one block in the filesystem
 * @param  fs           filesystem
 * @param  block_id     index of block in the filesystem
 * @param  block_offset offset inside the block
 * @param  size         size to read, must fit inside the block
 * @param  buffer       buffer to write the read stuff to
 * @return              read byte count
 */
int mini_fat_read_in_block(FAT_FILESYSTEM *fs, const int block_id, const int block_offset, const int size, void * buffer) {
	assert(block_offset >= 0);
	assert(block_offset < fs->block_size);
	assert(size + block_offset <= fs->block_size);

	int read = 0;

	//Open the file system file
	FILE * fat_fd = fopen(fs->filename, "rb");
	if (fat_fd == NULL) {
		perror("Cannot read block to file");
		return false;
	}	
	//Determine seek position
	int position = (block_id * fs->block_size) + block_offset;
	fseek(fat_fd, position, SEEK_SET);
        fread(buffer,size,1, fat_fd);
        read = size;
	fclose(fat_fd);
	return read;

}


/**
 * Find the first empty block in filesystem.
 * @return -1 on failure, index of block on success
 */
int mini_fat_find_empty_block(const FAT_FILESYSTEM *fat) {	
	if(!fat->block_map.empty()) {
		for(long unsigned int i = 0; i < fat->block_map.size(); i++) {
			if(fat->block_map[i] == EMPTY_BLOCK) {
				return i;
			}
		}

	}
	return -1;
}

/**
 * Find the first empty block in filesystem, and allocate it to a type,
 * i.e., set block_map[new_block_index] to the specified type.
 * @return -1 on failure, new_block_index on success
 */
int mini_fat_allocate_new_block(FAT_FILESYSTEM *fs, const unsigned char block_type) {
	int new_block_index = mini_fat_find_empty_block(fs);
	if (new_block_index == -1)
	{
		fprintf(stderr, "Cannot allocate block: filesystem is full.\n");
		return -1;
	}
	fs->block_map[new_block_index] = block_type;
	return new_block_index;
}

void mini_fat_dump(const FAT_FILESYSTEM *fat) {
	printf("Dumping fat with %d blocks of size %d:\n", fat->block_count, fat->block_size);
	for (int i=0; i<fat->block_count;++i) {
		printf("%d ", (int)fat->block_map[i]);
	}
	printf("\n");

	for (long unsigned int i=0; i<fat->files.size(); ++i) {
		mini_file_dump(fat, fat->files[i]);
	}
}

static FAT_FILESYSTEM * mini_fat_create_internal(const char * filename, const int block_size, const int block_count) {
	FAT_FILESYSTEM * fat = new FAT_FILESYSTEM;
	fat->filename = filename;
	fat->block_size = block_size;
	fat->block_count = block_count;
	fat->block_map.resize(fat->block_count, EMPTY_BLOCK); // Set all blocks to empty.
	fat->block_map[0] = METADATA_BLOCK;
	return fat;
}

/**
 * Create a new virtual disk file.
 * The file should be of the exact size block_size * block_count bytes.
 * Overwrites existing files. Resizes block_map to block_count size.
 * @param  filename    name of the file on real disk
 * @param  block_size  size of each block
 * @param  block_count number of blocks
 * @return             FAT_FILESYSTEM pointer with parameters set.
 */
FAT_FILESYSTEM * mini_fat_create(const char * filename, const int block_size, const int block_count) {

	FAT_FILESYSTEM * fat = mini_fat_create_internal(filename, block_size, block_count);

	//Create/open the virtual disk
	FILE* virtual_disk = fopen(filename, "wb+");

	if(virtual_disk == 0) {
		printf("Cannot create the virtual disk!\n");
		exit(-1);
	}
	//Fix the size of virtual disk
	ftruncate(fileno(virtual_disk), block_size * block_count);
	fclose(virtual_disk);
	return fat;
}

/**
 * Save a virtual disk (filesystem) to file on real disk.
 * Stores filesystem metadata (e.g., block_size, block_count, block_map, etc.)
 * in block 0.
 * Stores file metadata (name, size, block map) in their corresponding blocks.
 * Does not store file data (they are written directly via write API).
 * @param  fat virtual disk filesystem
 * @return     true on success
 */
bool mini_fat_save(const FAT_FILESYSTEM *fat) {
	//Open the file system
	FILE * fat_fd = fopen(fat->filename, "rb+");
	if (fat_fd == NULL) {
		perror("Cannot save fat to file");
		return false;
	}
	//Check if the file system is empty	
	if(fat->block_map.empty()) {

		return false;
	}
	//For every metadata and file entry block convert the data to string and write it to corresponding block	
	for(long unsigned int i = 0; i < fat->block_map.size(); i++) {
		char buffer[1024] = "";
		if(fat->block_map[i] == METADATA_BLOCK) {

			sprintf(buffer,"%d %d ", fat->block_count, fat->block_size);

			for(long unsigned int j = 0; j < fat->block_map.size(); j++) {			
				char block = fat->block_map[j] + '0';	
				strncat(buffer,&block, 1);
				strcat(buffer," ");	
			}
			mini_fat_write_in_block((FAT_FILESYSTEM*)fat, 0, 0, strlen(buffer),buffer); 
		}

		else if(fat->block_map[i] == FILE_ENTRY_BLOCK) {
			
			for(long unsigned int k = 0; k < fat->files.size(); k++) {

				FAT_FILE * fat_file = mini_file_find(fat, fat->files[k]->name);
				if (!fat_file) {
					fprintf(stderr, "File '%s' does not exist.\n", fat_file->name);
					return false;
				}		
				
				if(fat_file->metadata_block_id  == (int)i) {

					sprintf(buffer, "%d %ld %s ",fat_file->size, strlen(fat_file->name), fat_file->name);
					for (long unsigned int j=0; j<fat_file->block_ids.size(); ++j) {

						char blok_id = fat_file->block_ids[j] + '0';	
                				strncat(buffer, &blok_id,1);
						strcat(buffer, " ");
					}
					mini_fat_write_in_block((FAT_FILESYSTEM*)fat, i, 0, sizeof(buffer),buffer);

				}
			}
		}
	}
	fclose(fat_fd);	
	return true;
}

FAT_FILESYSTEM * mini_fat_load(const char *filename) {
	//Open the file system
	FILE * fat_fd = fopen(filename, "rb+");
	if (fat_fd == NULL) {
		perror("Cannot load fat from file");
		exit(-1);
	}
	int block_size = 1024, block_count = 10;
	FAT_FILESYSTEM * fat = mini_fat_create_internal(filename, block_size, block_count);


	int metadata_size_to_load = block_size;
	char buffer[1024];
	mini_fat_read_in_block(fat, 0, 0, metadata_size_to_load, buffer);


	//block_size
	char* token = strtok(buffer, " ");
	//block_count
	token = strtok(NULL, " ");

	token = strtok(NULL, " ");
	int index = 0;
	while(token != NULL) {
		fat->block_map[index] = token[0] - '0';
		index++;
		token = strtok(NULL, " ");
	}
	//For every file entry block write the data to corresponing blocks
	for(unsigned long int i = 0; i < fat->block_map.size(); i++) {

		if(fat->block_map[i] == FILE_ENTRY_BLOCK) {
	
			FAT_FILE* fat_file = new FAT_FILE;
			fat_file->metadata_block_id = i;
			mini_fat_read_in_block(fat, i, 0, fat->block_size, buffer);

			//file_size
			char* token = strtok(buffer, " ");
			fat_file->size = atoi(token);

			//name_size
			token = strtok(NULL, " ");

			//name
			token = strtok(NULL, " ");
			strcpy(fat_file->name, token);
			
			token = strtok(NULL, " ");
			while(token != NULL) {
				fat_file->block_ids.push_back(token[0] - '0');
				token = strtok(NULL, " ");
			}
			fat->files.push_back(fat_file);	
		}
	}	
	fclose(fat_fd);
	return fat;
}
