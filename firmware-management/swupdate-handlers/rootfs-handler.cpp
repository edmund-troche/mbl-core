#include <string>
#include <experimental/filesystem>
#include <fstream>
#include <streambuf>

extern "C" {
    #include "swupdate.h"
    #include "handler.h"
    #include "util.h"

    int rootfs_handler(struct img_type *img,
                       void __attribute__ ((__unused__)) *data);
    void rootfs_handler_init(void);
    int swupdate_umount(const char *dir);
    int swupdate_mount(const char *device, const char *dir, const char *fstype);
    int register_handler(const char *desc, handler installer, HANDLER_MASK mask, void *data);
    int openfile(const char *filename);
    int copyimage(void *out, struct img_type *img, writeimage callback);
    int rootfs_handler(struct img_type *img
                       , void __attribute__ ((__unused__)) *data);
    void rootfs_handler_init(void);
}

__attribute__((constructor))
void rootfs_handler_init(void)
{
    register_handler("rootfs"
                     , rootfs_handler
                     , HANDLER_MASK::IMAGE_HANDLER, nullptr);
}

namespace block_device
{
    bool is_active(const std::experimental::filesystem::path &device_file_path)
    {
        std::ifstream proc_mounts_path("/proc/mounts");
        std::stringstream buffer;
        buffer << proc_mounts_path.rdbuf();

        return (buffer.str().find(device_file_path) != std::string::npos);
    }

    std::experimental::filesystem::path get_file_path_from_label(const std::string &label)
    {
        std::experimental::filesystem::path dev_by_label = "/dev/disk/by-label";
        dev_by_label /= label;

        return std::experimental::filesystem::read_symlink(dev_by_label);
    }
};

class BankedPartition
{
public:
    BankedPartition(const std::pair<std::string, std::string> &labels)
    {
        const auto dev_a = block_device::get_file_path_from_label(labels.first);
        const auto dev_b = block_device::get_file_path_from_label(labels.second);

        if (block_device::is_active(dev_a))
        {
            active_bank_label = labels.first;
            target_bank_label = labels.second;
        }
        else if (block_device::is_active(dev_b))
        {
            active_bank_label = labels.second;
            active_bank_label = labels.first;
        }
    }

    std::experimental::filesystem::path get_target_device()
    {
        return block_device::get_file_path_from_label(target_bank_label);
    }

    std::experimental::filesystem::path get_current_device()
    {
        return block_device::get_file_path_from_label(active_bank_label);
    }

private:
    std::string active_bank_label;
    std::string target_bank_label;
};


int rootfs_handler(struct img_type *img
                   , void __attribute__ ((__unused__)) *data)
{
    BankedPartition bpart{std::pair<std::string, std::string>{"rootfs1", "rootfs2"}};

    int fd = openfile(bpart.get_target_device().c_str());
    if (fd < 0)
        return fd;

    const int ret = copyimage(&fd, img, nullptr);
    if (ret < 0)
        return ret;

    swupdate_umount(bpart.get_current_device().c_str());
    //swupdate_mount(bpart.get_target_device().c_str());

    return ret;
}
