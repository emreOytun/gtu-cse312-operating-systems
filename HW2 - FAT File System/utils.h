#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#define NUM_BLOCKS 4096
#define PASSWORD_SIZE 11

#define READ_PERMISSION 0x01  	// 00000001
#define WRITE_PERMISSION 0x02  	// 00000010
#define PASSWORD_ENABLED 0x04  	// 00000100
#define DIRECTORY 0x08  		// 00001000
#define FILE 0x10  				// 00010000
#define NO_PERMISSION 0x00		// 00000000

#define FAT_ENTRY_END 0x1111
#define FAT_ENTRY_EMPTY 0x1110

#define DIR_COMMAND 1
#define FILE_COMMAND 0 

struct directory_entry {
	uint32_t entry_size; // 4 bytes
	
	// This is useful to determine if some portion of the block is removed. For example, an entry inside a directory is removed when a file or directory inside a directory is removed.
	uint8_t is_active_entry; // 1 byte
	int8_t is_last_entry; // 1 byte
	
	uint8_t attributes; // 1 byte for attributes
	char pw[PASSWORD_SIZE]; // 11 bytes 
	
	uint16_t create_time; // 2 bytes 
	uint16_t create_date; // 2 bytes
	uint16_t update_time; // 2 bytes
	uint16_t update_date; // 2 bytes
	
	uint16_t first_block_index; // 2 bytes
	uint32_t size; // 4 bytes
};


struct superblock {
	uint16_t block_size; // One block's size in byte
	uint16_t fat_index; // Index of FATs
	uint16_t block_number_for_fat; // Total number of blocks for fat
	uint16_t block_number_for_superblock;
	
	// Keeps root directory's information (It does not keep the files/directories inside root directory but attributes of root directory itself)
	struct directory_entry root_dir; 
	
	uint16_t num_files;
	uint16_t num_dirs;
	
	uint16_t num_free_blocks;
};

struct fat_entry {
	uint16_t next_index;
};

struct block {
	char* data;
};

struct filesystem {
	struct superblock sb;
	struct fat_entry fat[NUM_BLOCKS]; // Keeps indexes for files and directories (since directories are also files in a way)
	struct block blocks[NUM_BLOCKS]; // File and directory data are kept in the blocks
};

int check_attribute(uint8_t* attributes, uint8_t permission);
uint8_t set_attribute(uint8_t* attributes, uint8_t permission);

uint8_t remove_attribute(uint8_t* attributes, uint8_t permission);

// Function to get the current time in FAT format
/*
Time is stored as:
    Bits 15-11: Hours (0 - 23)
    Bits 10-5: Minutes (0 - 59)
    Bits 4-0: Seconds / 2 (0 - 29, represents 0 to 58 seconds)
*/
uint16_t get_current_fat_time();

// Function to get the current date in FAT format
/*
Date is stored as:
    Bits 15-9: Year (0 = 1980, 127 = 2107)
    Bits 8-5: Month (1 = January, 12 = December)
    Bits 4-0: Day (1 - 31)
*/

uint16_t get_current_fat_date();

// Function to decode the FAT time format into hours, minutes, and seconds
void decode_fat_time(uint16_t fat_time, int *hours, int *minutes, int *seconds);

// Function to decode the FAT date format into year, month, and day
void decode_fat_date(uint16_t fat_date, int *year, int *month, int *day);

off_t get_current_offset(int fd);

int fill_remaining_block_with_zero(int fd, int block_size, int written_bytes);

// This entry is inserted when a directory is firstly created
void create_last_dir_indicator_entry(struct directory_entry* last_dir_indicator_entry);

int write_superblock_and_fat(int fd, struct superblock* sb, struct fat_entry* fat);
void free_filesystem(struct filesystem* fs);
