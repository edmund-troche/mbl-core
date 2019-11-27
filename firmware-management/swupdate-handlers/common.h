char *str_new(const size_t size);
void str_delete(char *str);

// Read a file to a new string.
// Allocate memory for a new string buffer the size of the file.
// Returns a pointer to the newly allocated string buffer.
// This function should not be used to read large files.
// It is the callers responsibility to free the memory allocated.
char *read_file_to_new_str(const char* const filepath);

int string_endswith(const char* const substr, const char* const fullstr);

int concat_strs(char *dst, const char* const str_a, const char* const str_b);

int get_mounted_device_filename(char* dst, const char* const mount_point);

int find_target_partition(char *dst
                          , const char *const mnt_fsname
                          , const char *const bank1_part_num
                          , const char *const bank2_part_num);


