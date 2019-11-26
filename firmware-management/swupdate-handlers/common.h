// Read a file to a string.
// This will not work on files greater than 4Gb as fseek will fail.
// The in/out param buf is allocated on the heap, it is the caller's
// responsibility to free the memory allocated to buf.
int read_file_to_string(const char* const filepath, char** buf);

int string_endswith(const char* const substr, const char* const fullstr);

int concat_strs(const char* const str_a, const char* const str_b, char** buf);

int get_mounted_device_filename(const char* const mount_point, char** buf);

int find_target_partition(const char *const mnt_fsname
                          , const char *const bank1_part_num
                          , const char *const bank2_part_num
                          , char** output_buf);


