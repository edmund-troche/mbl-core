#include <mntent.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "swupdate.h"
#include "handler.h"
#include "util.h"

// Read a file to a string.
// This will not work on files greater than 4Gb as fseek will fail.
// The returned string buffer is allocated on the heap, it is the caller's
// responsibility to free the memory allocated to the buffer.
char* read_file_to_string(const char* const filepath)
{
    char* return_val;

    FILE* fp = fopen(filepath, "r");
    if (fp == NULL)
        return NULL;

    if (fseek(fp, 0, SEEK_END) == -1)
    {
        return_val = NULL;
        goto close;
    }

    const long file_len = ftell(fp);
    if (file_len == -1)
    {
        return_val = NULL;
        goto close;
    }

    if (fseek(fp, 0, SEEK_SET) == -1)
    {
        return_val = NULL;
        goto close;
    }

    char* buffer = malloc(file_len + 1);
    if (buffer == NULL)
        return NULL;

    fread(buffer, 1, file_len, fp);
    if (feof(fp) != 0 || ferror(fp) != 0)
    {
        return_val = NULL;
        goto close;
    }

    buffer[file_len] = '\0';
    return_val = buffer;

close:
    if (fclose(fp) == -1)
        return NULL;

    return return_val;
}

int string_endswith(const char* const substr, const char* const fullstr)
{
    const size_t endlen = strlen(fullstr) - strlen(substr);
    if (endlen <= 0)
        return 1;
    return strncmp(&fullstr[strlen(fullstr) - strlen(substr)], substr, strlen(substr));
}

char *concat_strs(const char* const str_a, const char* const str_b)
{
    const size_t buf_size = sizeof(str_a) + sizeof(str_b) + 1;
    char *dest = malloc(buf_size);
    snprintf(dest, buf_size, "%s%s", str_a, str_b);
    return dest;
}

char *get_mounted_device_filename(const char* const mount_point)
{
    FILE *mtab = setmntent("/etc/mtab", "r");
    char *devname = NULL;
    struct mntent *mntent_desc;

    while ((mntent_desc = getmntent(mtab)))
    {
        if (strcmp(mount_point, mntent_desc->mnt_dir) == 0)
        {
            return mntent_desc->mnt_fsname;
        }
    }

    return devname;
}

char *find_target_partition(const char *const mnt_fsname, const char *const bank1_part_num, const char *const bank2_part_num)
{
    if (string_endswith(mnt_fsname, bank1_part_num))
        return concat_strs(mnt_fsname, bank2_part_num);
    else if (string_endswith(mnt_fsname, bank2_part_num))
        return concat_strs(mnt_fsname, bank1_part_num);

    return NULL;
}

int rootfs_handler(struct img_type *img
                   , void __attribute__ ((__unused__)) *data)
{
    static const char *const BANK1_PART_NUM_FILE = "/config/factory/part-info/MBL_ROOT_FS_PART_NUMBER_BANK1";
    static const char *const BANK2_PART_NUM_FILE = "/config/factory/part-info/MBL_ROOT_FS_PART_NUMBER_BANK2";
    int return_value = 0;
    char *b1_pnum = NULL;
    char *b2_pnum = NULL;
    char *target_part = NULL;

    char *mnt_fsname = get_mounted_device_filename("/");
    if (!mnt_fsname)
        return 1;

    b1_pnum = read_file_to_string(BANK1_PART_NUM_FILE);
    if (!b1_pnum)
        return 1;

    b2_pnum = read_file_to_string(BANK2_PART_NUM_FILE);
    if (!b2_pnum)
    {
        return_value = 1;
        goto free;
    }

    target_part = find_target_partition(mnt_fsname, b1_pnum, b2_pnum);
    if (!target_part)
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

