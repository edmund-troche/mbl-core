#include <mntent.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "util.h"

// Read a file to a string.
// This will not work on files greater than 4Gb as fseek will fail.
// The in/out param buf is allocated on the heap, it is the caller's
// responsibility to free the memory allocated to buf.
int read_file_to_string(const char* const filepath, char** buf)
{
    int return_val = 1;

    INFO("%s%s", "Opening file: ", filepath);
    FILE* fp = fopen(filepath, "r");
    if (fp == NULL)
        return 1;

    if (fseek(fp, 0, SEEK_END) == -1)
    {
        return_val = 1;
        goto close;
    }

    const long file_len = ftell(fp);
    INFO("%s%i", "file length: ", file_len);
    if (file_len == -1)
    {
        return_val = 1;
        goto close;
    }

    if (fseek(fp, 0, SEEK_SET) == -1)
    {
        return_val = 1;
        goto close;
    }

    (*buf) = malloc(file_len + 1);
    INFO("%s%i", "allocated buffer size", sizeof(buf));
    if (!*buf)
    {
        return_val = 1;
        goto close;
    }

    fread(buf, 1, file_len, fp);
    if (feof(fp) != 0 || ferror(fp) != 0)
    {
        return_val = 1;
        goto close;
    }

    INFO("%s%s", "Buffer contains: ", buf);
    buf[file_len-1] = '\0';
    return_val = 0;

close:
    if (fclose(fp) == -1)
        return 1;

    return return_val;
}

int string_endswith(const char* const substr, const char* const fullstr)
{
    const size_t endlen = strlen(fullstr) - strlen(substr);
    if (endlen <= 0)
        return 1;
    return strncmp(&fullstr[strlen(fullstr) - strlen(substr)], substr, strlen(substr));
}

int concat_strs(const char* const str_a, const char* const str_b, char** buf)
{
    const size_t buf_size = strlen(str_a) + strlen(str_b) + 1;
    (*buf) = malloc(buf_size);
    if (!*buf)
        return 1;
    snprintf(*buf, buf_size, "%s%s", str_a, str_b);
    return 0;
}

int get_mounted_device_filename(const char* const mount_point, char** buf)
{
    FILE *mtab = setmntent("/etc/mtab", "r");
    struct mntent *mntent_desc;

    while ((mntent_desc = getmntent(mtab)))
    {
        if (strcmp(mount_point, mntent_desc->mnt_dir) == 0)
        {
            const size_t buf_size = strlen(mntent_desc->mnt_fsname) + 1;
            (*buf) = malloc(buf_size);
            if (!strncpy(*buf, mntent_desc->mnt_fsname, buf_size))
            {
                return 1;
            }
            buf[buf_size-1] = '\0';
            return 0;
        }
    }

    return 1;
}

int find_target_partition(const char *const mnt_fsname
                          , const char *const bank1_part_num
                          , const char *const bank2_part_num
                          , char** output_buf)
{
    int return_value = 1;

    char* mnt_device_cpy = malloc(strlen(mnt_fsname) + 1);
    if (!mnt_device_cpy)
        return 1;

    if (!strncpy(mnt_device_cpy, mnt_fsname, strlen(mnt_fsname)))
    {
        return_value = 1;
        goto free;
    }

    mnt_device_cpy[strlen(mnt_fsname)] = '\0';
    static const char* const delim = "p";
    char* token;
    token = strtok(mnt_device_cpy, delim);
    if (token == NULL)
    {
        return_value = 1;
        goto free;
    }

    if (string_endswith(mnt_fsname, bank1_part_num) == 0)
    {
        return_value = concat_strs(token, bank2_part_num, output_buf);
    }
    else if (string_endswith(mnt_fsname, bank2_part_num) == 0)
    {
        return_value = concat_strs(token, bank1_part_num, output_buf);
    }

free:
    free(mnt_device_cpy);
    return return_value;
}

