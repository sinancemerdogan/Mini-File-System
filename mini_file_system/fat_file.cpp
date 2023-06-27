#include "fat.h"
#include "fat_file.h"
#include <cassert>
#include <cstdarg>
#include <cstring>
#include <cstdio>
#include <math.h>

// Little helper to show debug messages. Set 1 to 0 to silence.
#define DEBUG 1
inline void debug(const char * fmt, ...) {
#if DEBUG>0
	va_list args;
   va_start(args, fmt);
   vprintf(fmt, args);
   va_end(args);
#endif
}

// Delete index-th item from vector.
template<typename T>
static void vector_delete_index(std::vector<T> &vector, const int index) {
	vector.erase(vector.begin() + index);
}

// Find var and delete from vector.
template<typename T>
static bool vector_delete_value(std::vector<T> &vector, const T var) {
	for (long unsigned int i=0; i<vector.size(); ++i) { //Changed the template code to get rid of warning (int to long int)
		if (vector[i] == var) {
			vector_delete_index(vector, i);
			return true;
		}
	}
	return false;
}

void mini_file_dump(const FAT_FILESYSTEM *fs, const FAT_FILE *file)
{
	printf("Filename: %s\tFilesize: %d\tBlock count: %d\n", file->name, file->size, (int)file->block_ids.size());
	printf("\tMetadata block: %d\n", file->metadata_block_id);
	printf("\tBlock list: ");
	for (long unsigned int i=0; i<file->block_ids.size(); ++i) { //Changed the template code to get rid of warning (int to long int)
		printf("%d ", file->block_ids[i]);
	}
	printf("\n");

	printf("\tOpen handles: \n");
	for (long unsigned int i=0; i<file->open_handles.size(); ++i) { //Changed the template code to get rid of warning (int to long int)
		printf("\t\t%ld) Position: %d (Block %d, Byte %d), Is Write: %d\n", i,
			file->open_handles[i]->position,
			position_to_block_index(fs, file->open_handles[i]->position),
			position_to_byte_index(fs, file->open_handles[i]->position),
			file->open_handles[i]->is_write);
	}
}


/**
 * Find a file in loaded filesystem, or return NULL.
 */
FAT_FILE * mini_file_find(const FAT_FILESYSTEM *fs, const char *filename)
{
	for (long unsigned int i=0; i<fs->files.size(); ++i) { //Changed the template code to get rid of warning
		if (strcmp(fs->files[i]->name, filename) == 0) // Match
			return fs->files[i];
	}
	return NULL;
}

/**
 * Create a FAT_FILE struct and set its name.
 */
FAT_FILE * mini_file_create(const char * filename)
{
	FAT_FILE * file = new FAT_FILE;
	file->size = 0;
	strcpy(file->name, filename);
	return file;
}


/**
 * Create a file and attach it to filesystem.
 * @return FAT_OPEN_FILE pointer on success, NULL on failure
 */
FAT_FILE * mini_file_create_file(FAT_FILESYSTEM *fs, const char *filename)
{
	assert(strlen(filename)< MAX_FILENAME_LENGTH);
	FAT_FILE *fd = mini_file_create(filename);

	int new_block_index = mini_fat_allocate_new_block(fs, FILE_ENTRY_BLOCK);
	if (new_block_index == -1)
	{
		fprintf(stderr, "Cannot create new file '%s': filesystem is full.\n", filename);
		return NULL;
	}
	fs->files.push_back(fd); // Add to filesystem.
	fd->metadata_block_id = new_block_index;
	return fd;
}

/**
 * Return filesize of a file.
 * @param  fs       filesystem
 * @param  filename name of file
 * @return          file size in bytes, or zero if file does not exist.
 */
int mini_file_size(FAT_FILESYSTEM *fs, const char *filename) {
	FAT_FILE * fd = mini_file_find(fs, filename);
	if (!fd) {
		fprintf(stderr, "File '%s' does not exist.\n", filename);
		return 0;
	}
	return fd->size;
}


/**
 * Opens a file in filesystem.
 * If the file does not exist, returns NULL, unless it is write mode, where
 * the file is created.
 * Adds the opened file to file's open handles.
 * @param  is_write whether it is opened in write (append) mode or read.
 * @return FAT_OPEN_FILE pointer on success, NULL on failure
 */
FAT_OPEN_FILE * mini_file_open(FAT_FILESYSTEM *fs, const char *filename, const bool is_write)
{
	FAT_FILE * fd = mini_file_find(fs, filename);
	if (!fd) {
		//Check if it's write mode, and if so create it. Otherwise return NULL.
		if(is_write) {
			if((fd =mini_file_create_file(fs, filename)));
			else return NULL;
		}
		else {
			return NULL;
		}
	}
	if (is_write) {
		//Check if other write handles are open.
		for(long unsigned int i = 0; i < fd->open_handles.size(); i++) {
			if(fd->open_handles[i]->is_write) {
				return NULL;
			}
		}
	}
	//Create new open handle
	FAT_OPEN_FILE * open_file = new FAT_OPEN_FILE;

	//Assign open_file fields.	
	open_file->file = fd;
	open_file->position = 0;
	open_file->is_write = is_write;
	

	//Add to list of open handles for fd:
	fd->open_handles.push_back(open_file);
	return open_file;
}

/**
 * Close an existing open file handle.
 * @return false on failure (no open file handle), true on success.
 */
bool mini_file_close(FAT_FILESYSTEM *fs, const FAT_OPEN_FILE * open_file)
{
	if (open_file == NULL) return false;
	FAT_FILE * fd = open_file->file;
	if (vector_delete_value(fd->open_handles, open_file)) {
		return true;
	}

	fprintf(stderr, "Attempting to close file that is not open.\n");
	return false;
}

/**
 * Write size bytes from buffer to open_file, at current position.
 * @return           number of bytes written.
 */
int mini_file_write(FAT_FILESYSTEM *fs, FAT_OPEN_FILE * open_file, const int size, const void * buffer)
{
	int written_bytes = 0;

	//Find the file
	FAT_FILE * fd = mini_file_find(fs, open_file->file->name);
	if (!fd) {
		fprintf(stderr, "File '%s' does not exist.\n", fd->name);
		return 0;
	}
	//If file has no block allocate one
	if(open_file->file->block_ids.empty()) {

		int new_block_index = mini_fat_allocate_new_block(fs, FILE_DATA_BLOCK);
		if (new_block_index == -1)
		{
			fprintf(stderr, "Cannot create new block for the file '%s': filesystem is full.\n", fd->name);
			return written_bytes;
		}
		fd->block_ids.push_back(new_block_index); // Add to filesystem.
	}
	
	int over_write = 0;
	int size_before_over_write = fd->size;
	if(open_file->position < fd->size) {
		over_write = 1;	
	}

	int free_block_size = fs->block_size - position_to_byte_index(fs,open_file->position);	
	int size_needed = size;
	char* write_buffer = (char*)buffer;

	//If file has enough space directly write
	if(size <= free_block_size) {

		int block_index = position_to_block_index(fs, open_file->position);
		int block = open_file->file->block_ids[block_index];
		int block_offset = position_to_byte_index(fs, open_file->position);
			
		written_bytes += mini_fat_write_in_block(fs, block, block_offset, size, buffer);
		fd->size += written_bytes;	
		open_file->position += written_bytes;
		
	}

	//Else create new block as needed and write to them
	else {
		
		int block_index = position_to_block_index(fs, open_file->position);
		int block = open_file->file->block_ids[block_index];
		int block_offset = position_to_byte_index(fs, open_file->position);	
		int written_in_one_iteration = 0;
	

		written_in_one_iteration = mini_fat_write_in_block(fs, block, block_offset, free_block_size, write_buffer);
		written_bytes += written_in_one_iteration;
		fd->size += written_in_one_iteration;
		open_file->position += written_in_one_iteration;
		size_needed -= written_in_one_iteration;
	       	write_buffer += written_in_one_iteration;	


		while(size_needed > 0) {

			int new_block_index = mini_fat_allocate_new_block(fs, FILE_DATA_BLOCK);
			if (new_block_index == -1) {
				fprintf(stderr, "Cannot create new block for the file '%s': filesystem is full.\n", fd->name);
				return written_bytes;
			}	
				fd->block_ids.push_back(new_block_index); // Add new block to filesystem.
		

			block_index = position_to_block_index(fs, open_file->position);
			block = open_file->file->block_ids[block_index];
			block_offset = position_to_byte_index(fs, open_file->position);
			free_block_size = fs->block_size - block_offset;
				
			
			if(size_needed <= free_block_size) {
				free_block_size = size_needed;
			}

	
			written_in_one_iteration = mini_fat_write_in_block(fs, block, block_offset, free_block_size, write_buffer);
			written_bytes += written_in_one_iteration;	
			fd->size += written_in_one_iteration;
			open_file->position += written_in_one_iteration;
			size_needed -= written_in_one_iteration;
			write_buffer += written_in_one_iteration;	

		}
	}
	//Overwrite
	if(over_write) {
		//Ignore the written bytes
		fd->size -= written_bytes;
		//When over write increase the size	
		if(open_file->position > size_before_over_write) {
			fd->size += (size_before_over_write - open_file->position); 
		}
	}
	return written_bytes;
}
/**
 * Read up to size bytes from open_file into buffer.
 * @return           number of bytes read.
 */
int mini_file_read(FAT_FILESYSTEM *fs, FAT_OPEN_FILE * open_file, const int size, void * buffer)
{
	int read_bytes = 0;

	//Find the file
	FAT_FILE * fd = mini_file_find(fs, open_file->file->name);
	if (!fd) {
		fprintf(stderr, "File '%s' does not exist.\n", fd->name);
		return 0;
	}
	
	if(open_file->file->block_ids.empty()) {
		return 0;		
	}
		

	int block_size_to_read = fs->block_size - position_to_byte_index(fs,open_file->position);	
	int size_to_read = open_file->file->size - position_to_byte_index(fs,open_file->position);
	char* read_buffer = (char*)buffer;

	//If size fits in one block than read
	if(size <= block_size_to_read) {

		int block_index = position_to_block_index(fs, open_file->position);
		int block = open_file->file->block_ids[block_index];
		int block_offset = position_to_byte_index(fs, open_file->position);
				
		read_bytes += mini_fat_read_in_block(fs, block, block_offset, size, buffer);
		open_file->position += read_bytes;	
	}
	//Read all file blocks
	else {
		
		int block_index = position_to_block_index(fs, open_file->position);
		int block = open_file->file->block_ids[block_index];
		int block_offset = position_to_byte_index(fs, open_file->position);	
		int read_in_one_iteration = 0;

		read_in_one_iteration = mini_fat_read_in_block(fs, block, block_offset, block_size_to_read, read_buffer);
		read_bytes += read_in_one_iteration;
		open_file->position += read_in_one_iteration;
		size_to_read -= read_in_one_iteration; 
		read_buffer += read_in_one_iteration;

		while(size_to_read > 0) {

		
			block_index = position_to_block_index(fs, open_file->position);
			block = open_file->file->block_ids[block_index];
			block_offset = position_to_byte_index(fs, open_file->position);
			block_size_to_read = fs->block_size - block_offset;
			
			
			if(size_to_read <= block_size_to_read) {
				block_size_to_read = size_to_read;
			}

			read_in_one_iteration = mini_fat_read_in_block(fs, block, block_offset, block_size_to_read, read_buffer);
			read_bytes += read_in_one_iteration;
			open_file->position += read_in_one_iteration;
			size_to_read -= read_in_one_iteration;
			read_buffer += read_in_one_iteration;	
			
		}
	}
	return read_bytes;
}


/**
 * Change the cursor position of an open file.
 * @param  offset     how much to change
 * @param  from_start whether to start from beginning of file (or current position)
 * @return            false if the new position is not available, true otherwise.
 */
bool mini_file_seek(FAT_FILESYSTEM *fs, FAT_OPEN_FILE * open_file, const int offset, const bool from_start)
{
	//Check if seek position is valid then seek and return true otherwise return false
	if(offset < 0) {
		if(from_start){
			return false;
		}
		else {	
			if(open_file->position + offset < 0) {
				return false;
			}
		}	
		open_file->position += offset;
		return true;
	}
	if(from_start){

		if(offset <= mini_file_size(fs, open_file->file->name)) {
			open_file->position = offset;
			return true;
		}
		return false;
	}
	else {
		if(offset + open_file->position <= mini_file_size(fs, open_file->file->name)) {
			open_file->position += offset;
			return true;
		}
		return false;
	}
}

/**
 * Attemps to delete a file from filesystem.
 * If the file is open, it cannot be deleted.
 * Marks the blocks of a deleted file as empty on the filesystem.
 * @return true on success, false on non-existing or open file.
 */
bool mini_file_delete(FAT_FILESYSTEM *fs, const char *filename)
{
	//Delete file after checks
	FAT_FILE * fd = mini_file_find(fs, filename);
	
	if(!fd){
		printf("There is no file named %s\n", filename);
		return false;
	}
	if(!(fd->open_handles.empty())) {
		printf("File is open. Cannot be deleted!\n");
		return false;
	}

	fs->block_map[fd->metadata_block_id] = EMPTY_BLOCK;	
	for(long unsigned int i = 0; i < fd->block_ids.size(); i++ ) {
		fs->block_map[fd->block_ids[i]] = EMPTY_BLOCK;	
	
	}
	if(!(vector_delete_value(fs->files, fd))) {

		return false;
	}
	return true;
}
