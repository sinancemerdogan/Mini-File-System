Mini File System

•	mini fat create(filename, block size, block count)
The function just creates just sized (block size * block count) fat file (disk) using filename.

•	mini fat save(fs)
It writes the metadata of disk and files to the corresponding blocks. It writes the block using mini_fat_write_in_block() to write the contents.

•	mini fat load(filename)
It writes the metadata of disk and files from the corresponding blocks and loads the saved system. It reads the block using mini_fat_read_in_block() to write the contents.

•	mini file open(fs, filename, is write)
It attempts to open the file. If file can be opened then opens the file and return true otherwise false.

•	mini file delete(fs, filename)
It attempts to delete the file. If file can be deleted then deletes the file, frees the allocated blocks return true otherwise false.

•	mini file seek(fs, open file, offset, from start)
It seeks the file cursor between start and end of the file. If seek is in that range return true otherwise false.


•	mini file write(fs, open file, size, buffer)
It writes the data of the file to its corresponding blocks. It creates data blocks if needed. It handles the overwrite. It writes the block using mini_fat_write_in_block() to write the contents. 




•	mini file read(fs, open file, size, buffer)
It reads the data of the file from its corresponding blocks. It read the block using mini_fat_read_in_block() to write the contents. 


•	mini_fat_write_in_block(FAT_FILESYSTEM *fs, const int block_id, const int block_offset, const int size, void * buffer)

It writes the bytes to the given block starting from given offset.

•	mini_fat_read_in_block(FAT_FILESYSTEM *fs, const int block_id, const int block_offset, const int size, void * buffer)

It reads the bytes from the given block starting from given offset.

Compiled using ‘make’ command
Ran as ‘./minifs’


All the functions work correctly. For more information about the functions, you can see the comments in the code. Also, for other helper function you can refer to the code. 
