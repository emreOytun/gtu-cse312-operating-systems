int load_superblock_and_fat(const char* filesystem_file, struct superblock* sb, struct fat_entry* fat, int* fd);
int read_file_name(int fd, char* name, uint32_t name_size, uint16_t* fat_cur_index, uint16_t* remaining_block_size);
int search_name_in_dir(struct directory_entry* dir_entry, struct directory_entry* found_entry, char* file_name, off_t* offset);
int find_free_block(struct superblock* sb);
int insert_name_to_dir(int fd, char* name, uint32_t name_size, uint16_t* fat_cur_index, uint16_t* remaining_block_size);
int calculate_total_num_blocks_for_file(struct directory_entry* new_entry);
int calculate_total_num_blocks_for_entry(struct directory_entry* new_entry, int remaining_block_size);
int allocate_block_for_new_entry(struct directory_entry* new_entry);
int add_dir_entry(struct directory_entry* dir_entry, struct directory_entry* new_entry, char* file_name);


int print_entries_in_directory(struct directory_entry* dir_entry);
int print_file(struct directory_entry* dir_entry, char* linux_file_name);

int set_permission_given_from_command(struct directory_entry* dir, char* chmodpw_arg);

int traverse_dir_helper(struct directory_entry* dir_entry, char* path_args[], int path_argc, int index, int is_dir_command, char* linux_file_name, int is_chmod, int is_addpw, char* chmodpw_arg, char* pw);
int traverse_file_or_dir(const char* filesystem_file, char* path_args[], int path_argc, int is_dir_command, char* linux_file_name, int is_chmod, int is_password, char* chmodpw_arg, char* pw);

int copy_file_from_linux(struct directory_entry* new_entry, char* linux_file_name);

int mkdir_helper(struct directory_entry* dir_entry, char* path_args[], int path_argc, int index, int is_dir_command, char* linux_file_name, char* pw);
int mkdir_entry(const char* filesystem_file, char* path_args[], int path_argc, int is_dir_command, char* linux_file_name, char* pw);

int mkdir_command(const char* filesystem_file, char* path_args[], int path_argc);
int write_command(const char* filesystem_file, char* path_args[], int path_argc, char* linux_file_name, char* pw);
int dir_command(const char* filesystem_file, char* path_args[], int path_argc);
int read_command(const char* filesystem_file, char* path_args[], int path_argc, char* linux_file_name, char* pw);
int chmod_command(const char* filesystem_file, char* path_args[], int path_argc, char* mods, char* pw);
int addpw_command(const char* filesystem_file, char* path_args[], int path_argc, char* new_pwd, char* pw);

// TODO:
int del_command(const char* filesystem_file, char* path_args[], int path_argc, char* pw);

// TODO:
int rmdir_command(const char* filesystem_file, char* path_args[], int path_argc);

// TODO:
int dumpe2fs_command();
