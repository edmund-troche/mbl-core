#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "common.h"
#include "swupdate.h"
#include "handler.h"
#include "util.h"


int rootfs_handler(struct img_type *img
                   , void __attribute__ ((__unused__)) *data)
{
    int return_value = 0;

    static const char *const bank1_pnum_filepath = "/config/factory/part-info/MBL_ROOT_FS_PART_NUMBER_BANK1";
    static const char *const bank2_pnum_filepath = "/config/factory/part-info/MBL_ROOT_FS_PART_NUMBER_BANK2";

    char mnt_fsname[PATH_MAX];
    if (get_mounted_device_filename(mnt_fsname, "/") != 0)
    {
        ERROR("%s", "Failed to get mounted device filename.");
        return 1;
    }

    char *b1_pnum;
    char *b2_pnum;
    char *target_part;
    if (!(b1_pnum = read_file_to_new_str(bank1_pnum_filepath)))
    {
        ERROR("%s%s", "Failed to read file: ", bank1_pnum_filepath);
        return_value = 1;
        goto free;
    }

    if (!(b2_pnum = read_file_to_new_str(bank2_pnum_filepath)))
    {
        ERROR("%s%s", "Failed to read file: ", bank2_pnum_filepath);
        return_value = 1;
        goto free;
    }

    target_part = str_new(strlen(b1_pnum) + strlen(b2_pnum) + 1);
    if (find_target_partition(target_part, mnt_fsname, b1_pnum, b2_pnum) != 0)
    {
        ERROR("%s", "Failed to find target partition.");
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

    swupdate_umount(mnt_fsname);
    swupdate_mount(target_part, "/", "ext4");

free:
    str_delete(b1_pnum);
    str_delete(b2_pnum);
    str_delete(target_part);

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

