#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include "common.h"
#include "swupdate.h"
#include "handler.h"
#include "util.h"

int get_target_device_file_path(char *target_device_filepath, const char *const mounted_device_filepath)
{
    int return_value = 0;

    static const char *const bank1_pnum_filepath = "/config/factory/part-info/MBL_ROOT_FS_PART_NUMBER_BANK1";
    static const char *const bank2_pnum_filepath = "/config/factory/part-info/MBL_ROOT_FS_PART_NUMBER_BANK2";

    char *b1_pnum;
    char *b2_pnum;
    if (!(b1_pnum = read_file_to_new_str(bank1_pnum_filepath)))
    {
        ERROR("%s%s", "Failed to read file: ", bank1_pnum_filepath);
        return_value = 1;
        goto free;
    }

    TRACE("%s%s", "bank 1 part number is ", b1_pnum);

    if (!(b2_pnum = read_file_to_new_str(bank2_pnum_filepath)))
    {
        ERROR("%s%s", "Failed to read file: ", bank2_pnum_filepath);
        return_value = 1;
        goto free;
    }

    TRACE("%s%s", "bank 2 part number is ", b2_pnum);

    if (find_target_bank(target_device_filepath, mounted_device_filepath, b1_pnum, b2_pnum) != 0)
    {
        ERROR("%s", "Failed to find target partition.");
        return_value = 1;
    }

free:
    str_delete(b1_pnum);
    str_delete(b2_pnum);
    return return_value;
}

int copy_image_to_block_device(struct img_type *img, const char *const device_filepath)
{
    int fd = openfileoutput(device_filepath);
    if (fd < 0)
    {
        ERROR("%s%s", "Failed to open output file ", device_filepath);
        return -1;
    }

    const int ret = copyimage(&fd, img, NULL);
    if (ret < 0)
    {
        ERROR("%s%s", "Failed to copy image to target device");
        return -1;
    }

    if (fsync(fd) == -1)
    {
        WARN("%s: %s", "Failed to sync filesystem", strerror(errno));
    }

    return 0;
}


int rootfs_handler(struct img_type *img
                   , void __attribute__ ((__unused__)) *data)
{
    int return_value = 0;

    static const char *const root_mnt_point = "/";
    char mounted_device_filepath[PATH_MAX];
    if (get_mounted_device_path(mounted_device_filepath, root_mnt_point) != 0)
    {
        ERROR("%s", "Failed to get mounted device file path.");
        return 1;
    }

    TRACE("%s%s", "Mounted rootfs device file path is ", mounted_device_filepath);

    char target_device_filepath[PATH_MAX];
    if (get_target_device_file_path(target_device_filepath, mounted_device_filepath) != 0)
    {
        ERROR("%s", "Failed to get target device file path");
        return 1;
    }

    TRACE("%s%s", "Target partition device file path is ", target_device_filepath);

    if (copy_image_to_block_device(img, target_device_filepath) == -1)
    {
        ERROR("%s %s %s", "Failed to copy image", img->fname "to device", target_device_filepath);
        return 1;
    }

    TRACE("%s %s", "Unmounting device", mounted_device_filepath);
    if (swupdate_umount(root_mnt_point) == -1)
    {
        ERROR("%s %s: %s", "Failed to unmount device", mounted_device_filepath, strerror(errno));
        return 1;
    }

    TRACE("%s%s", "Mounting device ", target_device_filepath);
    if (swupdate_mount(target_device_filepath, root_mnt_point, "ext4") == -1)
    {
        ERROR("%s %s: %s", "Failed to mount device", target_device_filepath, strerror(errno));
        return 1;
    }

    return 0;
}


__attribute__((constructor))
void rootfs_handler_init(void)
{
    register_handler("ROOTFSv4"
                     , rootfs_handler
                     , IMAGE_HANDLER
                     , NULL);
}

