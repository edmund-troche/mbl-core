#include "swupdate.h"
#include "handler.h"
#include "util.h"


__attribute__((constructor))
void rootfs_handler_init(void)
{
    register_handler("rootfs update"
                     , rootfs_handler
                     , HANDLER_MASK::IMAGE_HANDLER, nullptr);
}

int rootfs_handler(struct img_type *img
                   , void __attribute__ ((__unused__)) *data)
{
    int fd = openfile(img->device);
    if (fd < 0)
        return fd;

    const int ret = copyimage(&fd, img, nullptr);
    if (ret < 0)
        return ret;

//    swupdate_umount(bpart.get_current_device().c_str());
    //swupdate_mount(bpart.get_target_device().c_str());

    return ret;
}
