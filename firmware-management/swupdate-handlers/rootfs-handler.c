#include <mntent.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "swupdate.h"
#include "handler.h"
#include "util.h"

// Read a file to a string.
// This will not work on files greater than 4Gb as fseek will fail.
// The in/out param buf is allocated on the heap, it is the caller's
// responsibility to free the memory allocated to buf.
int read_file_to_string(const char* const filepath, char** buf)
{
    int return_val;

    FILE* fp = fopen(filepath, "r");
    if (fp == NULL)
        return 1;

    if (fseek(fp, 0, SEEK_END) == -1)
    {
        return_val = 1;
        goto close;
    }

    const long file_len = ftell(fp);
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
    if (!*buf)
        return 1;

    fread(buf, 1, file_len, fp);
    if (feof(fp) != 0 || ferror(fp) != 0)
    {
        return_val = 1;
        goto close;
    }

    *buf[file_len] = '\0';
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
    const size_t buf_size = sizeof(str_a) + sizeof(str_b) + 1;
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
            if (!strncpy(*buf, mntent_desc->mnt_fsname, buf_size-1))
                return 1;

            *buf[buf_size-1] = '\0';
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
    char str_to_tokenise[strlen(mnt_fsname) + 1];
    if (!strncpy(str_to_tokenise, mnt_fsname, strlen(mnt_fsname)))
        return 1;

    str_to_tokenise[strlen(mnt_fsname)] = '\0';
    static const char* const delim = "p";
    char* token;
    token = strtok(str_to_tokenise, delim);

    if (token == NULL)
        return 1;

    if (string_endswith(mnt_fsname, bank1_part_num))
    {
        return concat_strs(token, bank2_part_num, output_buf);
    }
    else if (string_endswith(mnt_fsname, bank2_part_num))
    {
        return concat_strs(token, bank1_part_num, output_buf);
    }

    return 1;
}

int rootfs_handler(struct img_type *img
                   , void __attribute__ ((__unused__)) *data)
{
    static const char *const BANK1_PART_NUM_FILE = "/config/factory/part-info/MBL_ROOT_FS_PART_NUMBER_BANK1";
    static const char *const BANK2_PART_NUM_FILE = "/config/factory/part-info/MBL_ROOT_FS_PART_NUMBER_BANK2";
    int return_value = 0;
    char *b1_pnum;
    char *b2_pnum;
    char *target_part;
    char *mnt_fsname;

    if (get_mounted_device_filename("/", &mnt_fsname) != 0)
        return 1;

    if (read_file_to_string(BANK1_PART_NUM_FILE, &b1_pnum) != 0)
    {
        return_value = 1;
        goto free;
    }

    if (read_file_to_string(BANK2_PART_NUM_FILE, &b2_pnum) != 0)
    {
        return_value = 1;
        goto free;
    }

    if (find_target_partition(mnt_fsname, b1_pnum, b2_pnum, &target_part) != 0)
    {
        return_value = 1;
        goto free;
    }

    int fd = openfileoutput(target_part);
    if (fd < 0)
    {
        return_value = 1;
        goto free;
    }

    const int ret = copyimage(&fd, img, NULL);
    if (ret < 0)
    {
        return_value = 1;
        goto free;
    }

free:
    free(mnt_fsname);
    free(b1_pnum);
    free(b2_pnum);
    free(target_part);

    return return_value;
}


__attribute__((constructor))
void rootfs_handler_init(void)
{
    register_handler("ROOTFSv4"
                     , rootfs_handler
                     , IMAGE_HANDLER
                     , NULL);
}

