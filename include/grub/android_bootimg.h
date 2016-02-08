#include <grub/file.h>

#define BOOT_ARGS_SIZE 512

/* Load a kernel from a bootimg. cmdline must be at least BOOT_ARGS_SIZE */
grub_err_t
grub_android_bootimg_load_kernel (const char *disk_path, grub_file_t *file,
                                  char *cmdline);

/* Load an initrd from a bootimg. */
grub_err_t
grub_android_bootimg_load_initrd (const char *disk_path, grub_file_t *file);
