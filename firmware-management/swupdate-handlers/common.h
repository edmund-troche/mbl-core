
/* Create a new string buffer.
   This function allocates memory for a new string buffer. It is the caller's
   responsibility to free the allocated memory. */
char *str_new(const size_t size);

/* Copy a string.
   This function allocates memory for the new string src is copied into.
   It is the caller's responsibility to free the allocated memory. */
char *str_copy(const char *src);

/* Delete a string buffer and free the memory. */
void str_delete(char *str);

/* Return 0 if fullstr ends with substr, else return 1 */
int str_endswith(const char *const substr, const char *const fullstr);

/* Read a file to a new string.
   Allocate memory for a new string buffer the size of the file.
   Returns a pointer to the newly allocated string buffer.
   This function should not be used to read large files.
   It is the callers responsibility to free the memory allocated. */
char *read_file_to_new_str(const char *const filepath);

int get_mounted_device_path(char *dst, const char *const mount_point);

int find_target_bank(char *dst
                     , const char *const mounted_device
                     , const char *const bank1_part_num
                     , const char *const bank2_part_num);


