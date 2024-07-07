#include "utils.h"
#include "fileSystemOper.h"
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>

int load_superblock_and_fat(const char* filesystem_file, struct superblock* sb, struct fat_entry* fat, int* fd) {
	*fd = open(filesystem_file, O_RDWR);
	if (*fd == -1) {
		perror("There is a problem in opening the file");
		return -1;
	}
	
	int read_bytes = -1;
	read_bytes = read(*fd, sb, sizeof(struct superblock));
	if (read_bytes == -1) {
		perror("There is a problem in reading superblock");
		return -1;
	}
		
	// Go to start of FAT in file using lseek
	lseek(*fd, sb->fat_index * sb->block_size, SEEK_SET);
	
	read_bytes = read(*fd, fat, sizeof(struct fat_entry) * NUM_BLOCKS);
	if (read_bytes == -1) {
		perror("There is a problem in reading superblock");
		return -1;
	}
		
	return 0;
}

struct superblock sb;
struct fat_entry fat[NUM_BLOCKS];
int fd;

// Returns -1 if error
int read_file_name(int fd, char* name, uint32_t name_size, uint16_t* fat_cur_index, uint16_t* remaining_block_size) {
	int read_bytes = -1;
	uint32_t read_name_size = 0;
	int is_file_name_read_done = 0;
	while (!is_file_name_read_done && name_size != 0) {
		if (name_size <= *remaining_block_size) {
			read_bytes = read(fd, &name[read_name_size], name_size);
	
			if (read_bytes == -1) {
				perror("There is a problem in reading entry file name");
				free(name);
				return -1;
			}
			
			*remaining_block_size -= read_bytes;
			is_file_name_read_done = 1;
		}
		else {
			if (*remaining_block_size != 0) {
				read_bytes = read(fd, &name[read_name_size], *remaining_block_size);
				if (read_bytes == -1) {
					perror("There is a problem in reading entry file name");
					free(name);
					return -1;
				}
				name_size -= read_bytes;
				read_name_size += read_bytes;
			}
			
			uint16_t fat_next_index = fat[*fat_cur_index].next_index;
			*fat_cur_index = fat_next_index;
			
			lseek(fd, *fat_cur_index * sb.block_size, SEEK_SET); // Go to the beginning of the block since there is remaining file name, there is no need to check -1 fat index state
			*remaining_block_size = sb.block_size; // Reset remaining block size
		}			
	}
}

// Searches the given file name in directory entries inside a directory
// Return values:
// -1 in error case, 0 not found, 1 found
int search_name_in_dir(struct directory_entry* dir_entry, struct directory_entry* found_entry, char* file_name, off_t* offset) {
	int is_found = 0;
	uint16_t fat_cur_index = dir_entry->first_block_index;
	
	lseek(fd, fat_cur_index * sb.block_size, SEEK_SET);
	uint16_t remaining_block_size = sb.block_size;
	
	struct directory_entry entry;
	int is_done = 0;
	while (!is_done) {
		
		// If there is no remaining size in the block to read a directory entry, then go to the next block for this directory if possible
		if (remaining_block_size < sizeof(struct directory_entry)) {	
			// Go to next fat index 
			uint16_t fat_next_index = fat[fat_cur_index].next_index;
			fat_cur_index = fat_next_index;
			
			// If there is no next block for this directory then exit the loop
			if (fat_cur_index == FAT_ENTRY_END) {
				is_found = 0;
				is_done = 1;
			}
			else {
				lseek(fd, fat_cur_index * sb.block_size, SEEK_SET); // Go to the beginning of the next block for this directory
				remaining_block_size = sb.block_size; // Reset remaining block size
			}
			continue;
		}
		
		*offset = get_current_offset(fd);
		int read_bytes = read(fd, &entry, sizeof(struct directory_entry));
		if (read_bytes == -1) {
			perror("There is a problem in reading directory entry");
			printf("fd: %d \n", fd);
			return -1;
		}
		remaining_block_size -= read_bytes;
	
		// The search inside directory is done if it is the last entry
		is_done = entry.is_last_entry;
	
		// If entry is not a passive and the last entry, then we need to read the name (passive + last entry = no need to read because it is passive and there is no more entries)
		
		if (!entry.is_last_entry || entry.is_active_entry) {
			uint32_t name_size = entry.entry_size - sizeof(struct directory_entry);
			char* name = malloc(sizeof(char) * name_size);
			
			if (read_file_name(fd, name, name_size, &fat_cur_index, &remaining_block_size) == -1) {
				return -1;
			}
			
			// If it is acive entry, then we need to compare the names
			if (entry.is_active_entry && strcmp(file_name, name) == 0) {
				memcpy(found_entry, &entry, sizeof(struct directory_entry));
				is_done = 1;
				is_found = 1;
			}	
			free(name);
		}
	}
	return is_found;
}

// -1 if not found, index if found
int find_free_block(struct superblock* sb) {
	// Traverse through FAT, look for FAT_ENTRY_EMPTY value which indicates
	// the block is free

	for (uint16_t i = 0; i < NUM_BLOCKS; ++i) {
		if (fat[i].next_index == FAT_ENTRY_EMPTY) return i;
	}
	return -1;
}

int insert_name_to_dir(int fd, char* name, uint32_t name_size, uint16_t* fat_cur_index, uint16_t* remaining_block_size) {
	int write_bytes = -1;
	uint32_t written_name_size = 0;
	int is_name_write_done = 0;
	while (!is_name_write_done) {
		if (name_size <= *remaining_block_size) {
			write_bytes = write(fd, &name[written_name_size], name_size);
			if (write_bytes == -1) {
				perror("There is a problem in writing file name");
				return -1;
			}
			*remaining_block_size -= write_bytes;
			is_name_write_done = 1;
		}
		else {
			if (*remaining_block_size != 0) {
				write_bytes = read(fd, &name[written_name_size], *remaining_block_size);
				if (write_bytes == -1) {
					perror("There is a problem in reading entry file name");
					free(name);
					return -1;
				}
				name_size -= write_bytes;
				written_name_size += write_bytes;
			}
			
			// Go to next fat index 
			uint16_t fat_next_index = fat[*fat_cur_index].next_index;
			
			// If there is no next block for this directory, then allocate new block if possible
			if (fat_next_index == FAT_ENTRY_END) {
				int free_block_index = find_free_block(&sb);
				if (free_block_index == -1) {
					// No free block found
					is_name_write_done = 1;
					return -1;
				}
				else {
					// Free block found
					sb.num_free_blocks -= 1;
					
					fat[*fat_cur_index].next_index = free_block_index;
					fat[free_block_index].next_index = FAT_ENTRY_END;
					
					// Write superblock and fat
					if (write_superblock_and_fat(fd, &sb, fat) == -1) return -1;
					
					*fat_cur_index = fat_next_index;		
					
					lseek(fd, *fat_cur_index * sb.block_size, SEEK_SET); // Go to the beginning of the next block for this directory
					*remaining_block_size = sb.block_size; // Reset remaining block size
				}
			}
			else {
				*fat_cur_index = fat_next_index;
				
				lseek(fd, *fat_cur_index * sb.block_size, SEEK_SET); // Go to the beginning of the next block for this directory
				*remaining_block_size = sb.block_size; // Reset remaining block size
			}
		}			
	}
}

int calculate_total_num_blocks_for_file(struct directory_entry* new_entry) {
	return (int) ceil((double) new_entry->size / sb.block_size);
}

int calculate_total_num_blocks_for_entry(struct directory_entry* new_entry, int remaining_block_size) {
	// Calculate how many blocks are necessary for directory entry
	int num_blocks_for_dir_entry = 0;
	if (new_entry->entry_size > remaining_block_size) {
		int remaining_size = new_entry->entry_size - remaining_block_size;
		num_blocks_for_dir_entry = (int) ceil((double) remaining_block_size / sb.block_size);
	}
	
	int num_total_blocks = num_blocks_for_dir_entry;
	if (check_attribute(&new_entry->attributes, FILE)) {
		// File
		num_total_blocks += calculate_total_num_blocks_for_file(new_entry);
	}
	else {
		// Directory
		num_total_blocks += 1;
	}
	return num_total_blocks;
}

int allocate_block_for_new_entry(struct directory_entry* new_entry) {
	int free_block_index = find_free_block(&sb);
	if (free_block_index == -1) {
		// No free block found
		printf("No free block found \n");
		return -1;
	}
	else {
		// Free block found
		sb.num_free_blocks -= 1;
		
		fat[free_block_index].next_index = FAT_ENTRY_END;
		
		// Write superblock and fat
		if (write_superblock_and_fat(fd, &sb, fat) == -1) return -1;
		
		new_entry->first_block_index = free_block_index;
	
		if (check_attribute(&new_entry->attributes, DIRECTORY)) {
			// Save current position which is inside the current directory table
			off_t offset = get_current_offset(fd);
			lseek(fd, free_block_index * sb.block_size, SEEK_SET); // Go to the beginning of the next block for this directory
			
			struct directory_entry last_dir_indicator_entry; // This entry is inserted when a directory is firstly created
			create_last_dir_indicator_entry(&last_dir_indicator_entry);
		
			int written_bytes = write(fd, (void*) &last_dir_indicator_entry, sizeof(struct directory_entry));
			if (written_bytes == -1) {
				perror("There is a problem with writing to the file. Program is closing...");
				return -1; // Will be freed in main
			}
			
			// Restore current position which is inside the current directory table
			lseek(fd, offset, SEEK_SET);
		}
	}
	return 0;
}

// dir_entry: Current directory's directory entry
// new_entry: New directory entry to add into current directory's directory table
// Return -1 if error
int add_dir_entry(struct directory_entry* dir_entry, struct directory_entry* new_entry, char* file_name) {
	// FIRST-FIT ALGORITHM: Search first available space in the current directory's directory table
	
	int is_found = 0;
	uint16_t fat_cur_index = dir_entry->first_block_index;
	
	lseek(fd, fat_cur_index * sb.block_size, SEEK_SET);
	uint16_t remaining_block_size = sb.block_size;
	
	off_t last_active_offset;
	int after_last_active = 0;
	struct directory_entry entry;
	int is_done = 0;
	while (!is_done) {
		// If there is no remaining size in the block to read a directory entry, then go to the next block for this directory if possible
		
		if (remaining_block_size < sizeof(struct directory_entry)) {
			// Go to next fat index 
			uint16_t fat_next_index = fat[fat_cur_index].next_index;
			
			// If there is no next block for this directory, then allocate new block if possible
			if (fat_next_index == FAT_ENTRY_END) {
				
				int free_block_index = find_free_block(&sb);
				if (free_block_index == -1) {
					// No free block found
					printf("There is not enough capacity \n");
					is_found = 0;
					is_done = 1;
				}
				else {	
					// Free block found
					sb.num_free_blocks -= 1;
					
					fat[fat_cur_index].next_index = free_block_index;
					fat[free_block_index].next_index = FAT_ENTRY_END;
					
					// Write superblock and fat
					if (write_superblock_and_fat(fd, &sb, fat) == -1) return -1;
					
					fat_cur_index = fat_next_index;		
					
					lseek(fd, fat_cur_index * sb.block_size, SEEK_SET); // Go to the beginning of the next block for this directory
					remaining_block_size = sb.block_size; // Reset remaining block size
				}
			}
			else {
				fat_cur_index = fat_next_index;
				
				lseek(fd, fat_cur_index * sb.block_size, SEEK_SET); // Go to the beginning of the next block for this directory
				remaining_block_size = sb.block_size; // Reset remaining block size
			}
			continue;
		}
		
		int write_bytes = -1;
		if (after_last_active) {
			int total_num_blocks = calculate_total_num_blocks_for_entry(new_entry, remaining_block_size);
			
			// Check size
			if (total_num_blocks > sb.num_free_blocks) { 
				// No such capacity
				printf("There is not enough capacity \n");
				is_done = 1;
				is_found = 0;
			}
			else {
				// Enough capacity
				
				// Update free blocks while allocating 1 data block no matter if it is directory or file. If directory, null indicator entry is necessary.
				if (allocate_block_for_new_entry(new_entry) == -1) {
					return -1;
				}
				
				off_t cur_offset = get_current_offset(fd);
				
				// Go back to the last entry which was active
				lseek(fd, last_active_offset, SEEK_SET);
				
				entry.is_last_entry = 0;
				if ((write_bytes = write(fd, (void*) &entry, sizeof(struct directory_entry))) == -1) {
					perror("There is a problem with writing to .data file. Program is closing...");
					return -1;
				}
				
				lseek(fd, cur_offset, SEEK_SET);
			
				// I can insert my entry here. The previous one was an active entry which was the last entry in the whole directory table for this directory.		
				if ((write_bytes = write(fd, (void*) new_entry, sizeof(struct directory_entry))) == -1) {
					perror("There is a problem with writing to .data file. Program is closing...");
					return -1;
				}
				remaining_block_size -= sizeof(struct directory_entry);
				
				off_t offset;
				if (insert_name_to_dir(fd, file_name, new_entry->entry_size - sizeof(struct directory_entry), &fat_cur_index, &remaining_block_size) == -1) return -1;
				is_done = 1;
				is_found = 1;
			}
			
			continue;
		}
		
		off_t offset = get_current_offset(fd);
		int read_bytes = read(fd, &entry, sizeof(struct directory_entry));
		if (read_bytes == -1) {
			perror("There is a problem in reading directory entry");
			printf("fd: %d \n", fd);
			return -1;
		}
		remaining_block_size -= read_bytes;
	
		if (entry.is_last_entry && !entry.is_active_entry) {
			// If last and passive entry, then new entry must inserted instead of that
			// Go previous offset and write there
			lseek(fd, offset, SEEK_SET);
			remaining_block_size += read_bytes;
			
			// Set new entry as last entry in the directory
			new_entry->is_last_entry = 1;
			
			int total_num_blocks = calculate_total_num_blocks_for_entry(new_entry, remaining_block_size);
			
			// Check size
			if (total_num_blocks > sb.num_free_blocks) { 
				// No such capacity
				printf("There is not enough capacity \n");
				is_done = 1;
				is_found = 0;
			}
			else {
				// Enough capacity
			
				// Update free blocks while allocating 1 data block no matter if it is directory or file. If directory, null indicator entry is necessary.
				if (allocate_block_for_new_entry(new_entry) == -1) {
					return -1;
				}
						
				if ((write_bytes = write(fd, (void*) new_entry, sizeof(struct directory_entry))) == -1) {
					perror("There is a problem with writing to .data file. Program is closing...");
					return -1;
				}
				remaining_block_size -= sizeof(struct directory_entry);
				
				off_t offset;
				if (insert_name_to_dir(fd, file_name, new_entry->entry_size - sizeof(struct directory_entry), &fat_cur_index, &remaining_block_size) == -1) return -1;
				is_done = 1;
				is_found = 1;
			}
		}
		else if (entry.is_last_entry) {
			// If last and active entry, then new entry must inserted after that
			
			// Read the name and start writing after that
			uint32_t name_size = entry.entry_size - sizeof(struct directory_entry);
			char* name = malloc(sizeof(char) * name_size);
			if (read_file_name(fd, name, name_size, &fat_cur_index, &remaining_block_size) == -1) {
				free(name);
				return -1;
			}
			free(name);
			
			// Set new entry as last entry in the directory
			new_entry->is_last_entry = 1;
			
			after_last_active = 1;
			last_active_offset = offset;
		}
		else if (!entry.is_active_entry) {
			// If not last but passive entry, then check if we can insert here
			
			int total_num_blocks = calculate_total_num_blocks_for_entry(new_entry, remaining_block_size);
			
			// Check size
			if ((entry.entry_size == new_entry->entry_size || entry.entry_size >= new_entry->entry_size + sizeof(struct directory_entry)) && total_num_blocks <= sb.num_free_blocks) { 
				// Update free blocks while allocating 1 data block no matter if it is directory or file. If directory, null indicator entry is necessary.
				if (allocate_block_for_new_entry(new_entry) == -1) {
					return -1;
				}
			
				// Enough capacity to put in here instead passive fragment + Put a new directory entry indicating the empty fragment
				if ((write_bytes = write(fd, (void*) new_entry, sizeof(struct directory_entry))) == -1) {
					perror("There is a problem with writing to .data file. Program is closing...");
					return -1;
				}
				remaining_block_size -= sizeof(struct directory_entry);
				
				off_t offset;
				if (insert_name_to_dir(fd, file_name, new_entry->entry_size - sizeof(struct directory_entry), &fat_cur_index, &remaining_block_size) == -1) return -1;
				
				// Passive directory entry to keep fragment
				int remaining_passive_fragment_size = entry.entry_size - new_entry->entry_size;
				if (remaining_passive_fragment_size > 0) {
					struct directory_entry passive_entry;
					memset(&passive_entry, 0, sizeof(struct directory_entry));
			
					passive_entry.entry_size = remaining_passive_fragment_size;
					passive_entry.is_active_entry = 0;
					passive_entry.is_last_entry = 0;
					
					if ((write_bytes = write(fd, (void*) &passive_entry, sizeof(struct directory_entry))) == -1) {
						perror("There is a problem with writing to .data file. Program is closing...");
						return -1;
					}
					remaining_block_size -= sizeof(struct directory_entry);
				}
				
				is_done = 1;
				is_found = 1;
			}
		}
		else {
			// Continue by reading the name
			if (!entry.is_last_entry || entry.is_active_entry) {
				uint32_t name_size = entry.entry_size - sizeof(struct directory_entry);
				char* name = malloc(sizeof(char) * name_size);
				
				if (read_file_name(fd, name, name_size, &fat_cur_index, &remaining_block_size) == -1) {
					free(name);
					return -1;
				}
				
				free(name);
			}
		}
	}
	return is_found;
}


int print_entries_in_directory(struct directory_entry* dir_entry) {
	uint16_t fat_cur_index = dir_entry->first_block_index;
	
	lseek(fd, fat_cur_index * sb.block_size, SEEK_SET);
	uint16_t remaining_block_size = sb.block_size;
	
	struct directory_entry entry;
	int is_done = 0;
	while (!is_done) {
		// If there is no remaining size in the block to read a directory entry, then go to the next block for this directory if possible
		if (remaining_block_size < sizeof(struct directory_entry)) {	
			// Go to next fat index 
			uint16_t fat_next_index = fat[fat_cur_index].next_index;
			fat_cur_index = fat_next_index;
			
			// If there is no next block for this directory then exit the loop
			if (fat_cur_index == FAT_ENTRY_END) {
				is_done = 1;
			}
			else {
				lseek(fd, fat_cur_index * sb.block_size, SEEK_SET); // Go to the beginning of the next block for this directory
				remaining_block_size = sb.block_size; // Reset remaining block size
			}
			continue;
		}
		
		int read_bytes = read(fd, &entry, sizeof(struct directory_entry));
		if (read_bytes == -1) {
			perror("There is a problem in reading directory entry");
			printf("fd: %d \n", fd);
			return -1;
		}
		remaining_block_size -= read_bytes;
		
		uint32_t name_size = entry.entry_size - sizeof(struct directory_entry);

		// The search inside directory is done if it is the last entry
		is_done = entry.is_last_entry;
	
		// If entry is not a passive and the last entry, then we need to read the name (passive + last entry = no need to read because it is passive and there is no more entries)
		if (!entry.is_last_entry || entry.is_active_entry) {
			uint32_t name_size = entry.entry_size - sizeof(struct directory_entry);
			char* name = malloc(sizeof(char) * name_size);
			
			if (read_file_name(fd, name, name_size, &fat_cur_index, &remaining_block_size) == -1) {
				return -1;
			}
			
			if (entry.is_active_entry) {
				printf("**************************************** \n");
				printf("File name: %s \n", name);
				
				printf("Size: %d \n", entry.size);
				
				int year, month, day;
				decode_fat_date(entry.create_date, &year, &month, &day);
			 	printf("Create Date: %04d-%02d-%02d\n", year, month, day);
				
				int hours, minutes, seconds;
				decode_fat_time(entry.create_time, &hours, &minutes, &seconds);
				printf("Create Time: %02d:%02d:%02d\n", hours, minutes, seconds);
				
				decode_fat_date(entry.update_date, &year, &month, &day);
				printf("Update Date: %04d-%02d-%02d\n", year, month, day);
				
				decode_fat_time(entry.update_time, &hours, &minutes, &seconds);
				printf("Update Time: %02d:%02d:%02d\n", hours, minutes, seconds);
				
				printf("Attributes: ");
				if (check_attribute(&entry.attributes, FILE)) printf("f");
				else printf("d");
				
				if (check_attribute(&entry.attributes, WRITE_PERMISSION)) printf("w");
				else printf("-");
				
				if (check_attribute(&entry.attributes, READ_PERMISSION)) printf("r");
				else printf("-");
				printf("\n");
				
				printf("Password Protection: ");
				if (check_attribute(&entry.attributes, PASSWORD_ENABLED)) printf("YES");
				else printf("NO");
				printf("\n");
			}
			
			free(name);
		}
	}
	printf("\n");
	return 0;
}

// It is actually for "read" command. It reads the directory in the filesystem and copies into the given linux file.
int print_file(struct directory_entry* dir_entry, char* linux_file_name) {
	// Open file
	int fd_copy = open(linux_file_name, O_WRONLY | O_CREAT | O_TRUNC);
	if (fd_copy == -1) {
		perror("Error in opening linux file to copy in read");
		return -1;
	}
		
	uint16_t fat_cur_index = dir_entry->first_block_index;
	
	lseek(fd, fat_cur_index * sb.block_size, SEEK_SET);
	uint16_t remaining_block_size = sb.block_size;
		
	uint32_t size = dir_entry->size;
	int is_done = 0;
	while (!is_done && size != 0) {
		// If there is no remaining size in the block to read a directory entry, then go to the next block for this directory if possible
		if (remaining_block_size == 0) {	
			// Go to next fat index 
			uint16_t fat_next_index = fat[fat_cur_index].next_index;
			fat_cur_index = fat_next_index;
			
			// If there is no next block for this directory then exit the loop
			if (fat_cur_index == FAT_ENTRY_END) {
				is_done = 1;
			}
			else {
				lseek(fd, fat_cur_index * sb.block_size, SEEK_SET); // Go to the beginning of the next block for this directory
				remaining_block_size = sb.block_size; // Reset remaining block size
			}
			continue;
		}
		
		char ch;
		if (read(fd, (void*) &ch, sizeof(char)) == -1) {
			perror("Error in reading from filesystem file");
			close(fd_copy);
			return -1;
		}
		
		if (write(fd_copy, (void*) &ch, sizeof(char)) == -1) {
			perror("Error in writing to the linux file");
			close(fd_copy);
			return -1;
		}
		
		--size;
	}
	close(fd_copy);
	
	// Copy owner permissions to linux file that I opened
	mode_t owner_permissions = 0;
	if (check_attribute(&dir_entry->attributes, READ_PERMISSION)) owner_permissions |= S_IRUSR;
	if (check_attribute(&dir_entry->attributes, WRITE_PERMISSION)) owner_permissions |= S_IWUSR;
	 
	if (chmod(linux_file_name, owner_permissions) == -1) {
        perror("chmod");
    	return -1;
    }
	
	return 0;
}

void update_entry_time(struct directory_entry* dir) {
	dir->update_time = get_current_fat_time();
	dir->update_date = get_current_fat_date();
}

int set_permission_given_from_command(struct directory_entry* dir, char* chmodpw_arg) {
	if (strlen(chmodpw_arg) < 2) {
		printf("Invalid mode argument \n");
		return -1;
	}
	
	int is_plus = 0;
	if (chmodpw_arg[0] == '+') is_plus = 1;
	else if (chmodpw_arg[0] == '-') is_plus = 0;
	else {
		printf("Invalid mode argument \n");
		return -1;
	}
	
	if (chmodpw_arg[1] == 'r') {
		if (is_plus) set_attribute(&dir->attributes, READ_PERMISSION);
		else remove_attribute(&dir->attributes, READ_PERMISSION);
	}
	else if (chmodpw_arg[1] == 'w') {
		if (is_plus) set_attribute(&dir->attributes, WRITE_PERMISSION);
		else remove_attribute(&dir->attributes, WRITE_PERMISSION);
	}
	else {
		printf("Invalid mode argument \n");
		return -1;
	}
	
	if (strlen(chmodpw_arg) == 3) {
		if (chmodpw_arg[1] == 'r') {
			if (is_plus) set_attribute(&dir->attributes, READ_PERMISSION);
			else remove_attribute(&dir->attributes, READ_PERMISSION);
		}
		else if (chmodpw_arg[1] == 'w') {
			if (is_plus) set_attribute(&dir->attributes, WRITE_PERMISSION);
			else remove_attribute(&dir->attributes, WRITE_PERMISSION);
		}
		else {
			printf("Invalid mode argument \n");
			return -1;
		}	
	}
	else if (strlen(chmodpw_arg) > 3) {
		printf("Invalid mode argument \n");
		return -1;		
	}
	
	return 0;
}

int traverse_dir_helper(struct directory_entry* dir_entry, char* path_args[], int path_argc, int index, int is_dir_command, char* linux_file_name, int is_chmod, int is_addpw, char* chmodpw_arg, char* pw) {
	// Check if there is no remaining name in the given path
	if (index == path_argc) {
		printf("The directory cannot found \n");
		return 0;
	}
	
	// Search the given name in the directory table 
	struct directory_entry dir;
	off_t offset;
	int search_res = search_name_in_dir(dir_entry, &dir, path_args[index], &offset);
	if (search_res == -1) {
		return -1;
	}
	
	// If found
	if (search_res == 1) {
		// If it is the last name in the path
		if (path_argc == index + 1) {
			if (is_dir_command && check_attribute(&dir.attributes, FILE)) {
				printf("It is not a directory \n");
				return -1;
			}
			if (!is_dir_command && check_attribute(&dir.attributes, DIRECTORY)) {
				printf("It is not a file \n");
				return -1;
			}
			
			if (is_dir_command) print_entries_in_directory(&dir);
			else {
				// Check pw
				if (check_attribute(&dir.attributes, PASSWORD_ENABLED) && (pw == NULL || strcmp(dir.pw, pw) != 0)) {
					printf("Wrong password \n");
					return -1;
				}
				
				if (is_chmod || is_addpw) {
					if (is_chmod) {
						if (set_permission_given_from_command(&dir, chmodpw_arg) == -1) return -1;
					}
					else {
						// is_addpw
						if (strlen(chmodpw_arg) == 0 || strlen(chmodpw_arg) > PASSWORD_SIZE - 1) {
							printf("Password size cannot be 0 or greater than 10. \n");
							return -1;
						}
						// copy password
						for (int i = 0; i < strlen(chmodpw_arg); ++i) {
							dir.pw[i] = chmodpw_arg[i];
						}
						dir.pw[strlen(chmodpw_arg)] = '\0'; 
						set_attribute(&dir.attributes, PASSWORD_ENABLED);
					}
					
					update_entry_time(&dir);
					
					// Overwrite the entry
					// file overwrite. change the current dir_entry to update
					off_t cur_offset = get_current_offset(fd);
					
					lseek(fd, offset, SEEK_SET);
					if (write(fd, (void*) &dir, sizeof(struct directory_entry)) == -1) {
						perror("Error in writing to the system file \n");
						return -1;
					}
					lseek(fd, cur_offset, SEEK_SET);
				}
				else {
					if (check_attribute(&dir.attributes, READ_PERMISSION)) print_file(&dir, linux_file_name);
					else {
						printf("No permission to read \n");
						return -1;
					}
				}
			}
		}
		else {
			if (!check_attribute(&dir.attributes, DIRECTORY)) {
				printf("A name in the path is not a directory \n");
				return -1;
			}
		
			// If it is not last name in the path, then continue from the next directory
			traverse_dir_helper(&dir, path_args, path_argc, index + 1, is_dir_command, linux_file_name, is_chmod, is_addpw, chmodpw_arg, pw);
		}
	}
	
	// If not found
	else {
		// If it is not the last name in the path
		printf("The file '%s' in the path does not exist \n", path_args[index]);
	}
	
	return 0;
}

int traverse_file_or_dir(const char* filesystem_file, char* path_args[], int path_argc, int is_dir_command, char* linux_file_name, int is_chmod, int is_password, char* chmodpw_arg, char* pw) {
	if (load_superblock_and_fat(filesystem_file, &sb, fat, &fd) == -1) {	
		return -1;	
	}
	
	if (path_argc == 0) return 0;

	// Search inside root, no need for recursive
	if (strcmp(path_args[0], "/") == 0) {
		if (is_dir_command) print_entries_in_directory(&sb.root_dir);
		else return -1;
	}
	else traverse_dir_helper(&sb.root_dir, path_args, path_argc, 0, is_dir_command, linux_file_name, is_chmod, is_password, chmodpw_arg, pw);
}

int dir_command(const char* filesystem_file, char* path_args[], int path_argc) {
	traverse_file_or_dir(filesystem_file, path_args, path_argc, DIR_COMMAND, NULL, 0, 0, NULL, NULL);
}

int read_command(const char* filesystem_file, char* path_args[], int path_argc, char* linux_file_name, char* pw) {
	traverse_file_or_dir(filesystem_file, path_args, path_argc, FILE_COMMAND, linux_file_name, 0, 0, NULL, pw);
}

int chmod_command(const char* filesystem_file, char* path_args[], int path_argc, char* mods, char* pw) {
	traverse_file_or_dir(filesystem_file, path_args, path_argc, FILE_COMMAND, NULL, 1, 0, mods, pw);
}

int addpw_command(const char* filesystem_file, char* path_args[], int path_argc, char* new_pwd, char* pw) {
	traverse_file_or_dir(filesystem_file, path_args, path_argc, FILE_COMMAND, NULL, 0, 1, new_pwd, pw);
}

int copy_file_from_linux(struct directory_entry* new_entry, char* linux_file_name) {
	int fd_file = open(linux_file_name, O_RDONLY);
	if (fd_file == -1) {
		perror("There is a problem in opening the given linux file");
		return -1;
	}
	
	uint16_t fat_cur_index = new_entry->first_block_index;
	
	lseek(fd, fat_cur_index * sb.block_size, SEEK_SET);
	uint16_t remaining_block_size = sb.block_size;
	
	int is_done = 0;
	uint32_t size = new_entry->size;
	while (!is_done && size != 0) {	
		// If there is no remaining size in the block to read a directory entry, then go to the next block for this directory if possible
		if (remaining_block_size  == 0) {
			// Go to next fat index 
			uint16_t fat_next_index = fat[fat_cur_index].next_index;
			
			// If there is no next block for this directory, then allocate new block if possible
			if (fat_next_index == FAT_ENTRY_END) {
				
				// There is no such case there no free block available because available space is checked before
				int free_block_index = find_free_block(&sb);	
				
				// Free block found
				sb.num_free_blocks -= 1;
				
				fat[fat_cur_index].next_index = free_block_index;
				fat[free_block_index].next_index = FAT_ENTRY_END;
				
				// Write superblock and fat
				if (write_superblock_and_fat(fd, &sb, fat) == FAT_ENTRY_END) return -1;
				
				fat_cur_index = fat_next_index;		
				
				lseek(fd, fat_cur_index * sb.block_size, SEEK_SET); // Go to the beginning of the next block for this directory
				remaining_block_size = sb.block_size; // Reset remaining block size
			}
			else {
				fat_cur_index = fat_next_index;
				
				lseek(fd, fat_cur_index * sb.block_size, SEEK_SET); // Go to the beginning of the next block for this directory
				remaining_block_size = sb.block_size; // Reset remaining block size
			}
			continue;
		}
		
		// Read and write byte-byte
		char ch;
		if (read(fd_file, (void*) &ch, sizeof(char)) == -1) {
			perror("There is a problem in reading from linux file");
			close(fd_file);
			return -1;
		}
		
		if (write(fd, (void*) &ch, sizeof(char)) == -1) {
			perror("There is a problem in writing to the file in filesystem");
			close(fd_file);
			return -1;
		}
		
		--size;
	}
	
	close(fd_file);
	return 0;
}

// Creates directory by recursively travelling through directories
// Return values:
// -1 in error case, number of bytes added/removed (+, -)
// The caller must update root_dir_entry in superblock
int mkdir_helper(struct directory_entry* dir_entry, char* path_args[], int path_argc, int index, int is_dir_command, char* linux_file_name, char* pw) {
	// Check if there is no remaining name in the given path
	if (index == path_argc) {
		printf("The directory cannot found \n");
		return 0;
	}
	
	// Search the given name in the directory table 
	struct directory_entry dir;
	off_t offset;
	int search_res = search_name_in_dir(dir_entry, &dir, path_args[index], &offset);
	if (search_res == -1) {
		return -1;
	}
	
	int dir_write = 0;
	int file_write = 0;
	int file_overwrite = 0;
	struct directory_entry new_entry;
	
	// If found
	if (search_res == 1) {
		// If it is the last name in the path
		if (path_argc == index + 1) {
			if (!is_dir_command) {
				// for existing file in write command
				
				// Check pw
				if (check_attribute(&dir.attributes, PASSWORD_ENABLED) && (pw == NULL || strcmp(dir.pw, pw) != 0)) {
					printf("Wrong password \n");
					return -1;
				}
				
				if (check_attribute(&dir.attributes, WRITE_PERMISSION)) {
					file_write = 1;
					file_overwrite = 1;
				}
				else {
					printf("The file does not have write permission \n");
					return 0;
				}
			}
			else {
				printf("The directory already exists \n");
				return 0;
			}
		}
		else {
			if (!check_attribute(&dir.attributes, DIRECTORY)) {
				printf("A name in the path is not directory \n");
				return 0;
			} 
			
			// If it is not last name in the path, then continue from the next directory
			int res = mkdir_helper(&dir, path_args, path_argc, index + 1, is_dir_command, linux_file_name, pw);
			if (res == -1) return -1;
			
			// Add # of bytes if added any to sub paths
			dir.size += res;
			
			// Update the update time and date
			dir.update_time = get_current_fat_time();
			dir.update_date = get_current_fat_date();
			
			lseek(fd, offset, SEEK_SET);
			if (write(fd, (void*) &dir, sizeof(struct directory_entry)) == -1) {
				perror("There is a problem with writing to .data file. Program is closing...");
				return -1;
			}
			return res;
		}
	}
	
	// If not found
	else {
		// If it is the last name in the path 
		if (path_argc == index + 1) {
			// Add file to this directory
			
			if (is_dir_command) dir_write = 1;	
			else file_write = 1;
		}
		else {
			// If it is not the last name in the path
			printf("The file '%s' in the path does not exist \n", path_args[index]);
			return 0;
		}
	}
	
	if (file_write || dir_write) {
		memset(&new_entry, 0, sizeof(struct directory_entry));
	
		new_entry.entry_size = sizeof(struct directory_entry) + strlen(path_args[index]) + 1; // 1 for EOF
		new_entry.is_active_entry = 1;
		new_entry.is_last_entry = 0;
	
		new_entry.attributes = NO_PERMISSION;
	
		new_entry.create_time = get_current_fat_time();
		new_entry.create_date = get_current_fat_date();
		new_entry.update_time = get_current_fat_time();
		new_entry.update_date = get_current_fat_date();
		
		if (dir_write) {
			set_attribute(&new_entry.attributes, READ_PERMISSION);
			set_attribute(&new_entry.attributes, WRITE_PERMISSION);
			set_attribute(&new_entry.attributes, DIRECTORY);
						
			new_entry.size = 0;
				
			if (add_dir_entry(dir_entry, &new_entry, path_args[index]) == -1) return -1;
			
			sb.num_dirs += 1;
			// Write superblock and fat
			if (write_superblock_and_fat(fd, &sb, fat) == -1) return -1;
			
			return 0;
		}
		else {
			// file_write
			set_attribute(&new_entry.attributes, FILE);
				
			if (file_overwrite) {
				// the file is in the system, but it will be overwritten so its metadata is being updated. preverse the necessary ones from the current one
				new_entry.is_last_entry = dir.is_last_entry;
				for (int i = 0; i < PASSWORD_SIZE; ++i) new_entry.pw[i] = dir.pw[i];
				new_entry.create_time = dir.create_time;
				new_entry.create_date = dir.create_date;
				new_entry.first_block_index = dir.first_block_index;
			}
				
			int fd_copy = open(linux_file_name, O_RDONLY);	
			if (fd_copy == -1) {
				perror("open");
				return 1;
			}

			// Calculate the size of the file
			off_t file_size = lseek(fd_copy, 0, SEEK_END);
			if (file_size == -1) {
				perror("lseek");
				close(fd_copy);
				return -1;
			}
			close(fd_copy);
			
			long long size = (long long) file_size;
			new_entry.size = size;
			
			//if (access(linux_file_name, R_OK) == 0) set_attribute(&new_entry.attributes, READ_PERMISSION);
			//if (access(linux_file_name, W_OK) == 0) set_attribute(&new_entry.attributes, WRITE_PERMISSION);
			
			struct stat file_stat;

			// Get file metadata
			if (stat(linux_file_name, &file_stat) == -1) {
				perror("stat");
				exit(EXIT_FAILURE);
			}

			// Check owner read permission
			if (file_stat.st_mode & S_IRUSR) set_attribute(&new_entry.attributes, READ_PERMISSION);
			// Check owner write permission
			if (file_stat.st_mode & S_IWUSR) set_attribute(&new_entry.attributes, WRITE_PERMISSION);
			
			if (!file_overwrite) {
				int res = add_dir_entry(dir_entry, &new_entry, path_args[index]); 
				if (res == -1 || res == 0) return -1;
				
				sb.num_files += 1;
				// Write superblock and fat
				if (write_superblock_and_fat(fd, &sb, fat) == -1) return -1;
			}
			else {
				// file overwrite. change the current dir_entry to update
				off_t cur_offset = get_current_offset(fd);
				
				lseek(fd, offset, SEEK_SET);
				if (write(fd, (void*) &new_entry, sizeof(struct directory_entry)) == -1) {
					perror("Error in writing to the system file \n");
					return -1;
				}
				lseek(fd, cur_offset, SEEK_SET);
			}
			
			if (copy_file_from_linux(&new_entry, linux_file_name) == -1) return -1;
			return new_entry.size;
		}	
	}
	
	return 0;
}

int mkdir_entry(const char* filesystem_file, char* path_args[], int path_argc, int is_dir_command, char* linux_file_name, char* pw) {
	if (path_argc == 0) return 0;
	if (strcmp(path_args[0], "/") == 0) {
		// Root is given, cannot create root
		printf("Cannot create root directory \n");
		return 0;
	}

	// Load filesystem's fundamental files from .data file
	if (load_superblock_and_fat(filesystem_file, &sb, fat, &fd) == -1) {	
		return -1;	
	}
	
	off_t offset = 0;	
	int res = mkdir_helper(&sb.root_dir, path_args, path_argc, 0, is_dir_command, linux_file_name, pw);
	if (res == -1) return -1;
	
	// Add # of bytes if added any to sub paths
	sb.root_dir.size += res;
	lseek(fd, 0, SEEK_SET); // Go to the beginning where superblock resided
	if (write(fd, (void*) &sb, sizeof(struct superblock)) == -1) {
		perror("There is a problem with writing to .data file. Program is closing...");
		return -1;
	}
	
	return 0;
}

int mkdir_command(const char* filesystem_file, char* path_args[], int path_argc) {
	return mkdir_entry(filesystem_file, path_args, path_argc, DIR_COMMAND, NULL, NULL);
}

int write_command(const char* filesystem_file, char* path_args[], int path_argc, char* linux_file_name, char* pw) {
	return mkdir_entry(filesystem_file, path_args, path_argc, FILE_COMMAND, linux_file_name, pw);
}

// TODO:
int del_command(const char* filesystem_file, char* path_args[], int path_argc, char* pw) {

}

// TODO:
int rmdir_command(const char* filesystem_file, char* path_args[], int path_argc) {

}

// TODO:
int dumpe2fs_command() {

}

void remove_char(char *str, char char_to_remove) {
    char *src, *dst;
    for (src = dst = str; *src != '\0'; src++) {
        *dst = *src;
        if (*dst != char_to_remove) dst++;
    }
    *dst = '\0';
}

int main(int argc, char* argv[]) {
	
	if (argc < 3) {
		printf("Argument is not valid \n");
		return -1;
	}
	
	char* data_file = argv[1];
	
	// Split the given path
	char** path_args = (char**) malloc(sizeof(char*) * 10);
	int len = 0;
	int size = 10;

	//printf("Incoming args: %s \n", argv[3]);

	if (strcmp(argv[3], "/") == 0) {
		path_args[0] = (char*) malloc(sizeof(char) * 2);
		strcpy(path_args[0], "/");
		path_args[0][1] = '\0';
		
		++len;
	}
	else {		
		const char* delim = "/";
		char* token = strtok(argv[3], delim);
		
		// Path split process continuing
		while (token != NULL) {
			
			// Dynamic allocation
			if (len == size) {
				// Grow array
				char** copy = (char**) malloc(sizeof(char*) * size * 2);
				for (int i = 0; i < len; ++i) {
					path_args[i] = copy[i];
				}
				size *= 2;
				free(copy);
			}
			
			path_args[len] = (char*) malloc(sizeof(char) * (strlen(token) + 1)); 
			
			strcpy(path_args[len], token);
			path_args[len][strlen(token)] = '\0';
			++len;
		
			token = strtok(NULL, delim);
		}
	}
	
	/*
	printf("args: \n");
	for (int i = 0; i < len; ++i) {
		printf("%s \n", path_args[i]);
	}
	*/
	
	
	if (strcmp(argv[2], "dir") == 0) {
		if (argc < 4) {
			printf("Argument is not valid \n");
			return -1;
		}

		dir_command(data_file, path_args, len);	
	}
	else if (strcmp(argv[2], "mkdir") == 0) {
		if (argc < 4) {
			printf("Argument is not valid \n");
			return -1;
		}

		mkdir_command(data_file, path_args, len);
	}
	else if (strcmp(argv[2], "read") == 0) {
		if (argc < 5) {
			printf("Argument is not valid \n");
			return -1;
		}
		
		char* pw = NULL;
		if (argc > 5) pw = argv[5];
		read_command(data_file, path_args, len, argv[4], pw);
	}
	else if (strcmp(argv[2], "write") == 0) {
		if (argc < 5) {
			printf("Argument is not valid \n");
			return -1;
		}
	
		char* pw = NULL;
		if (argc > 5) pw = argv[5];
		write_command(data_file, path_args, len, argv[4], pw);
	}
	else if (strcmp(argv[2], "chmod") == 0) {
		if (argc < 5) {
			printf("Argument is not valid \n");
			return -1;
		}
		
		char* pw = NULL;
		if (argc > 5) pw = argv[5];	
		chmod_command(data_file, path_args, len, argv[4], pw);
	}
	else if (strcmp(argv[2], "addpw") == 0) {
		if (argc < 5) {
			printf("Argument is not valid \n");
			return -1;
		}
		
		char* pw = NULL;
		if (argc > 5) pw = argv[5];
		addpw_command(data_file, path_args, len, argv[4], pw);
	}
	else if (strcmp(argv[2], "del") == 0) {
		if (argc < 4) {
			printf("Argument is not valid \n");
			return -1;
		}
		
		//TODO:
		char* pw = NULL;
		if (argc > 4) pw = argv[4];
		del_command(data_file, path_args, len, pw);
	}
	else if (strcmp(argv[2], "rmdir") == 0) {
		if (argc < 4) {
			printf("Argument is not valid \n");
			return -1;
		}
		
		//TODO:	
		rmdir_command(data_file, path_args, len);
	}
	else if (strcmp(argv[2], "dumpe2fs") == 0) {

		//TODO:
		dumpe2fs_command();
	}
	
	// Free dynamically allocated path_args array
	for (int i = 0; i < len; ++i) {
		free(path_args[i]);
	}
	free(path_args);
	
	
	return 0;
}
