#include "utils.h"

struct filesystem* create_filesystem(int block_size, const char* filesystem_name) {
	
	int block_number_for_superblock = 0;
	int block_number_for_fat = 0;
	if (block_size == 1024) {
		block_number_for_superblock = 5;
		block_number_for_fat = 8;
	}
	else {
		// 512
		block_number_for_superblock = 9;
		block_number_for_fat = 16;
	}
	int free_blocks_start_index = block_number_for_superblock + block_number_for_fat + 1; // 1 block is allocated for root directory table
	
	/* Initialize filesystem */
	
	struct filesystem* fs = (struct filesystem*) malloc(sizeof(struct filesystem));
	
	struct superblock* sb = &fs->sb;
	struct fat_entry* fat = fs->fat;
	struct block* blocks = fs->blocks;
	
	// Initialize superblock
	sb->block_size = block_size;
	sb->num_files = 0;
	sb->num_dirs = 1; // 1 for root
	sb->fat_index = block_number_for_superblock; // fat comes after superblock
	sb->block_number_for_fat = block_number_for_fat;
	sb->block_number_for_superblock = block_number_for_superblock;
	sb->num_free_blocks = NUM_BLOCKS - free_blocks_start_index;
	
	// Initialize root directory entry
	sb->root_dir.entry_size = sizeof(struct directory_entry);
	sb->root_dir.is_active_entry = 1;
	sb->root_dir.attributes = NO_PERMISSION;
	set_attribute(&sb->root_dir.attributes, WRITE_PERMISSION);
	set_attribute(&sb->root_dir.attributes, READ_PERMISSION);
	set_attribute(&sb->root_dir.attributes, DIRECTORY);
	for (int i = 0; i < PASSWORD_SIZE; ++i) sb->root_dir.pw[i] = '0';
	
	sb->root_dir.create_time = get_current_fat_time();
    sb->root_dir.create_date = get_current_fat_date();
    sb->root_dir.update_time = get_current_fat_time();
    sb->root_dir.update_date = get_current_fat_date();	
	
	sb->root_dir.first_block_index = free_blocks_start_index - 1;
	sb->root_dir.size = 0; // Root directory has no entry at the beginning, so its size is 0
		
	// Initialize fat
	
	// superblock in fat
	int i = 0;
	for (int i = 0; i < block_number_for_superblock - 1; ++i) fat[i].next_index = i + 1; 
	fat[i].next_index = FAT_ENTRY_END; 
	
	// FAT blocks in fat
	for (int i = sb->fat_index; i < sb->fat_index + block_number_for_fat - 1; ++i) fat[i].next_index = i + 1; 
	fat[i].next_index = FAT_ENTRY_END; 
	
	// Root directory table start block in fat
	fat[sb->root_dir.first_block_index].next_index = FAT_ENTRY_END; // For root directory, it takes only 1 block at the beginning.
	for (int i = free_blocks_start_index; i < NUM_BLOCKS; ++i) fat[i].next_index = FAT_ENTRY_EMPTY;
	
	// Initialize blocks
	// Do not initialize first allocated blocks because they are superblock, fat, root directory in order and we have them in memory already.
	for (int i = 0; i < free_blocks_start_index; ++i) blocks[i].data = NULL;
	for (int i = free_blocks_start_index; i < NUM_BLOCKS; ++i) {
		blocks[i].data = (char*) malloc(sizeof(char) * sb->block_size);
		for (int j = 0; j < sb->block_size; ++j) blocks[i].data[j] = '0';
	}
	
	/* Write to data file whose name is given */
	
	// Open file
	int fd = open(filesystem_name, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (fd == -1) {
		perror("There is a problem with creating the given data file. Program is closing...");
		return fs; // Will be freed in main
	}
	
	// Write size of superblock at the beginning to read afterwards
	/*
	uint16_t superblock_size = sizeof(struct superblock);
	if (write(fd, (void*) &superblock_size, sizeof(uint16_t)) == -1) {
		perror("There is a problem with writing to the file. Program is closing...");
		return fs; // Will be freed in main
	}
	*/
	
	// Write superblock (root directory entry inside this)
	int written_bytes = -1;
	written_bytes = write(fd, (void*) sb, sizeof(struct superblock));
	if (written_bytes == -1) {
		perror("There is a problem with writing to the file. Program is closing...");
		return fs; // Will be freed in main
	}
	//written_bytes += 2; // for superblock_size we wrote at the beginning 
	
	if (fill_remaining_block_with_zero(fd, sb->block_size, written_bytes - (sb->block_size * (block_number_for_superblock - 1))) == -1) {
		perror("There is a problem with writing to the file. Program is closing...");
		return fs; // Will be freed in main
	}
	
	// Write fat
	written_bytes = write(fd, (void*) fat, sizeof(struct fat_entry) * NUM_BLOCKS);
	if (written_bytes == -1) {
		perror("There is a problem with writing to the file. Program is closing...");
		return fs; // Will be freed in main
	}
		
	if (fill_remaining_block_with_zero(fd, sb->block_size, written_bytes - (sb->block_size * (block_number_for_fat - 1))) == -1) {
		perror("There is a problem with writing to the file. Program is closing...");
		return fs; // Will be freed in main
	}
	
	// Write ROOT directory (fill with all 0 actually because it has no entry at the beginning)
	struct directory_entry last_dir_indicator_entry; // This entry is inserted when a directory is firstly created
	create_last_dir_indicator_entry(&last_dir_indicator_entry);
	
	/*
	char* name = "ali";
	last_dir_indicator_entry.entry_size = sizeof(struct directory_entry) + 4;
	last_dir_indicator_entry.is_active_entry = 1;
	last_dir_indicator_entry.is_last_entry = 1;
	*/
	
	written_bytes = write(fd, (void*) &last_dir_indicator_entry, sizeof(struct directory_entry));
	if (written_bytes == -1) {
		perror("There is a problem with writing to the file. Program is closing...");
		return fs; // Will be freed in main
	}
	
	//written_bytes += write(fd, (void*) name, 4);
	
	if (fill_remaining_block_with_zero(fd, sb->block_size, written_bytes) == -1) {
		perror("There is a problem with writing to the file. Program is closing...");
		return fs; // Will be freed in main
	}
	
	// Write remaining free blocks
	int total = 0;
	for (int i = free_blocks_start_index; i < NUM_BLOCKS; ++i) {
		if (fill_remaining_block_with_zero(fd, sb->block_size, 0) == -1) {
			perror("There is a problem with writing to the file. Program is closing...");
			return fs; // Will be freed in main
		}
		++total;
	}
	
	return fs;
}

int main(int argc, char* argv[]) {
	
	if (argc < 3) {
		printf("Argument is not valid \n");
		return -1;
	}
	
	int block_size = -1;
	if (strcmp(argv[1], "1") == 0) {
		block_size = 1024;
	}
	else if (strcmp(argv[1], "0.5") == 0) {
		block_size = 512;
	}
	else {
		printf("Block size argument is not valid \n");
		return -1;
	}
	
	struct filesystem* fs = create_filesystem(block_size, argv[2]);
	if (fs != NULL) {
		free_filesystem(fs);
	}	
}

