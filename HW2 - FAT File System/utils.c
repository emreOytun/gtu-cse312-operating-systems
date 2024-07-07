#include "utils.h"

int check_attribute(uint8_t* attributes, uint8_t permission) {
	return (*attributes & permission) != 0;
}

uint8_t set_attribute(uint8_t* attributes, uint8_t permission) {
	 *attributes =  *attributes | permission;
	 return *attributes;
}

uint8_t remove_attribute(uint8_t* attributes, uint8_t permission) {
	*attributes = *attributes & (~permission);
}

// Function to get the current time in FAT format
/*
Time is stored as:
    Bits 15-11: Hours (0 - 23)
    Bits 10-5: Minutes (0 - 59)
    Bits 4-0: Seconds / 2 (0 - 29, represents 0 to 58 seconds)
*/
uint16_t get_current_fat_time() {
    // Get the current time
    time_t raw_time;
    time(&raw_time);

    // Convert to local time structure
    struct tm *time_info = localtime(&raw_time);

    // Return the FAT time format
    return ((time_info->tm_hour << 11) & 0xF800) | 
           ((time_info->tm_min << 5) & 0x07E0) | 
           ((time_info->tm_sec / 2) & 0x001F);
}

// Function to get the current date in FAT format
/*
Date is stored as:
    Bits 15-9: Year (0 = 1980, 127 = 2107)
    Bits 8-5: Month (1 = January, 12 = December)
    Bits 4-0: Day (1 - 31)
*/

uint16_t get_current_fat_date() {
    // Get the current time
    time_t raw_time;
    time(&raw_time);

    // Convert to local time structure
    struct tm *time_info = localtime(&raw_time);

    // Return the FAT date format
    return (((time_info->tm_year - 80) << 9) & 0xFE00) | 
           (((time_info->tm_mon + 1) << 5) & 0x01E0) | 
           (time_info->tm_mday & 0x001F);
}

// Function to decode the FAT time format into hours, minutes, and seconds
void decode_fat_time(uint16_t fat_time, int *hours, int *minutes, int *seconds) {
    *hours = (fat_time >> 11) & 0x1F;   // Extract bits 15-11
    *minutes = (fat_time >> 5) & 0x3F;  // Extract bits 10-5
    *seconds = (fat_time & 0x1F) * 2;   // Extract bits 4-0 and multiply by 2
}

// Function to decode the FAT date format into year, month, and day
void decode_fat_date(uint16_t fat_date, int *year, int *month, int *day) {
    *year = ((fat_date >> 9) & 0x7F) + 1980;   // Extract bits 15-9 and add 1980
    *month = (fat_date >> 5) & 0x0F;           // Extract bits 8-5
    *day = fat_date & 0x1F;                    // Extract bits 4-0
}

off_t get_current_offset(int fd) {
	return lseek(fd, 0, SEEK_CUR);
}

int fill_remaining_block_with_zero(int fd, int block_size, int written_bytes) {
	
	// Create a buffer filled with zeros
	int remaining_bytes = block_size - written_bytes;
    if (remaining_bytes != 0) {
    	//printf("REmaining: %d \n", remaining_bytes);
		
		char *zero_buffer = (char*)calloc(1, remaining_bytes);
		if (!zero_buffer) {
		    perror("calloc failed");
	   		return -1;     
		}

		// Write the zero buffer to the file
		int bytes;
		if ((bytes = write(fd, zero_buffer, remaining_bytes)) == -1) {
			perror("write failed");
			free(zero_buffer);
			return -1;
		}
	
		// Free the zero buffer
		free(zero_buffer);
    }
}

// This entry is inserted when a directory is firstly created
void create_last_dir_indicator_entry(struct directory_entry* last_dir_indicator_entry) {
	memset(last_dir_indicator_entry, 0, sizeof(struct directory_entry));
	last_dir_indicator_entry->is_active_entry = 0;
	last_dir_indicator_entry->is_last_entry = 1;
	last_dir_indicator_entry->entry_size = sizeof(struct directory_entry);
}

int write_superblock_and_fat(int fd, struct superblock* sb, struct fat_entry* fat) {
	off_t cur_offset = get_current_offset(fd);
	lseek(fd, 0, SEEK_SET);

	// Write superblock (root directory entry inside this)
	int written_bytes = -1;
	written_bytes = write(fd, (void*) sb, sizeof(struct superblock));
	if (written_bytes == -1) {
		perror("There is a problem with writing to the file. Program is closing...");
		return -1; // Will be freed in main
	}
		
	if (fill_remaining_block_with_zero(fd, sb->block_size, written_bytes - (sb->block_size * (sb->block_number_for_superblock - 1))) == -1) {
		perror("There is a problem with writing to the file. Program is closing...");
		return -1; // Will be freed in main
	}
	
	// Write fat
	written_bytes = write(fd, (void*) fat, sizeof(struct fat_entry) * NUM_BLOCKS);
	if (written_bytes == -1) {
		perror("There is a problem with writing to the file. Program is closing...");
		return -1; // Will be freed in main
	}
	
	if (fill_remaining_block_with_zero(fd, sb->block_size, written_bytes - (sb->block_size * (sb->block_number_for_fat - 1))) == -1) {
		perror("There is a problem with writing to the file. Program is closing...");
		return -1; // Will be freed in main
	}
	
	lseek(fd, cur_offset, SEEK_SET);
	
	return 0;
}

void free_filesystem(struct filesystem* fs) {
	for (int i = 0; i < NUM_BLOCKS; ++i) {
		if (fs->blocks[i].data != NULL) free(fs->blocks[i].data);
	}
	
	free(fs);
}
