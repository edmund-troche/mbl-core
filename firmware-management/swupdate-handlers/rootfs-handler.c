#include <stdlib.h>
#include "common.h"
#include "swupdate.h"
#include "handler.h"
#include "util.h"


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

    INFO("%s", "Getting mounted device filename");
    if (get_mounted_device_filename("/", &mnt_fsname) != 0)
        return 1;

    INFO("%s%s", "Reading file to string: ", BANK1_PART_NUM_FILE);
    if (read_file_to_string(BANK1_PART_NUM_FILE, &b1_pnum) != 0)
    {
        return_value = 1;
        goto free;
    }

    INFO("%s%s", "Reading file to string: ", BANK2_PART_NUM_FILE);
    if (read_file_to_string(BANK2_PART_NUM_FILE, &b2_pnum) != 0)
    {
        return_value = 1;
        goto free;
    }

    INFO("%s%s", "Finding target partition: ", mnt_fsname);
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

