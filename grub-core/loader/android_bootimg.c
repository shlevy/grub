/* android_bootimg.c - Helpers for interacting with the android bootimg format. */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2016 Free Software Foundation, Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <grub/file.h>
#include <grub/misc.h>
#include <grub/android_bootimg.h>

/* from https://android.googlesource.com/platform/system/core/+/506d233e7ac8ca4efa80768153d842c296477f99/mkbootimg/bootimg.h */
/* From here until the end of struct boot_img_hdr available under the following terms:
 *
 * Copyright 2007, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#define BOOT_MAGIC	"ANDROID!"
#define BOOT_MAGIC_SIZE	8
#define BOOT_NAME_SIZE	16
#define BOOT_EXTRA_ARGS_SIZE	1024

struct boot_img_hdr
{
  grub_uint8_t magic[BOOT_MAGIC_SIZE];

  grub_uint32_t kernel_size;
  grub_uint32_t kernel_addr;

  grub_uint32_t ramdisk_size;
  grub_uint32_t ramdisk_addr;

  grub_uint32_t second_size;
  grub_uint32_t second_addr;

  grub_uint32_t tags_addr;
  grub_uint32_t page_size;
  grub_uint32_t unused[2];

  grub_uint8_t name[BOOT_NAME_SIZE];

  grub_uint8_t cmdline[BOOT_ARGS_SIZE];

  grub_uint32_t id[8];

  grub_uint8_t extra_cmdline[BOOT_EXTRA_ARGS_SIZE];
} GRUB_PACKED;

static grub_err_t
read_hdr (grub_disk_t disk, struct boot_img_hdr *hd)
{
  if (grub_disk_read (disk, 0, 0, sizeof *hd, hd))
    goto fail;

  if (grub_memcmp (hd->magic, BOOT_MAGIC, BOOT_MAGIC_SIZE))
    goto fail;

  if (hd->unused[0] || hd->unused[1])
    goto fail;

  hd->kernel_size = grub_le_to_cpu32 (hd->kernel_size);
  hd->kernel_addr = grub_le_to_cpu32 (hd->kernel_addr);
  hd->ramdisk_size = grub_le_to_cpu32 (hd->ramdisk_size);
  hd->ramdisk_addr = grub_le_to_cpu32 (hd->ramdisk_addr);
  hd->second_size = grub_le_to_cpu32 (hd->second_size);
  hd->second_addr = grub_le_to_cpu32 (hd->second_addr);
  hd->tags_addr = grub_le_to_cpu32 (hd->tags_addr);
  hd->page_size = grub_le_to_cpu32 (hd->page_size);

  grub_size_t i;
  for (i = 0; i < sizeof hd->id / sizeof hd->id[0]; ++i)
    {
      hd->id[i] = grub_le_to_cpu32 (hd->id[i]);
    }

  return GRUB_ERR_NONE;

fail:
  return grub_error (GRUB_ERR_BAD_FS, N_("%s not an android bootimg"),
                     disk->name);
}

static grub_ssize_t
grub_android_bootimg_read (grub_file_t file, char *buf, grub_size_t len)
{
  grub_size_t len_left = file->size - file->offset;
  len = len > len_left ? len_left : len;
  grub_off_t *begin_offset = file->data;
  grub_off_t actual_offset = *begin_offset + file->offset;
  file->device->disk->read_hook = file->read_hook;
  file->device->disk->read_hook_data = file->read_hook_data;
  grub_errno = grub_disk_read (file->device->disk, 0, actual_offset, len, buf);
  file->device->disk->read_hook = 0;

  if (grub_errno)
    return -1;
  else
    return len;
}

static grub_err_t
grub_android_bootimg_close (grub_file_t file)
{
  grub_free (file->data);
  return GRUB_ERR_NONE;
}

static struct grub_fs grub_android_bootimg_fs =
  {
    .name = "android_bootimg",
    .read = grub_android_bootimg_read,
    .close = grub_android_bootimg_close
  };

grub_err_t
grub_android_bootimg_load_kernel (const char *disk_path, grub_file_t *file,
                                  char *cmdline)
{
  grub_err_t ret = GRUB_ERR_NONE;
  struct grub_file *f = 0;
  grub_disk_t disk = grub_disk_open (disk_path);
  if (!disk)
    goto err;

  struct boot_img_hdr hd;
  if (read_hdr (disk, &hd))
    goto err;

  f = grub_zalloc (sizeof *f);
  if (!f)
    goto err;

  f->fs = &grub_android_bootimg_fs;
  f->device = grub_zalloc (sizeof *(f->device));
  if (!f->device)
    goto err;
  f->device->disk = disk;

  f->name = grub_malloc (sizeof "kernel");
  if (!f->name)
    goto err;
  grub_memcpy (f->name, "kernel", sizeof "kernel");
  grub_off_t *begin_offset = grub_malloc (sizeof *begin_offset);
  if (!begin_offset)
    goto err;
  *begin_offset = hd.page_size;
  f->data = begin_offset;
  f->size = hd.kernel_size;

  *file = f;
  grub_memcpy (cmdline, hd.cmdline, BOOT_ARGS_SIZE);

  return ret;

err:
  ret = grub_errno;
  if (disk)
    grub_disk_close (disk);

  if (f)
    {
      grub_free (f->device);
      grub_free (f->name);
      grub_free (f);
    }

  return ret;
}

grub_err_t
grub_android_bootimg_load_initrd (const char *disk_path, grub_file_t *file)
{
  grub_err_t ret = GRUB_ERR_NONE;
  struct grub_file *f = 0;
  grub_disk_t disk = grub_disk_open (disk_path);
  if (!disk)
    goto err;

  struct boot_img_hdr hd;
  if (read_hdr (disk, &hd))
    goto err;

  if (!hd.ramdisk_size)
    {
      grub_error (GRUB_ERR_FILE_NOT_FOUND, N_("no ramdisk in `%s'"), disk_path);
      goto err;
    }

  f = grub_zalloc (sizeof *f);
  if (!f)
    goto err;

  f->fs = &grub_android_bootimg_fs;
  f->device = grub_zalloc (sizeof *(f->device));
  if (!f->device)
    goto err;
  f->device->disk = disk;

  f->name = grub_malloc (sizeof "ramdisk");
  if (!f->name)
    goto err;
  grub_memcpy (f->name, "ramdisk", sizeof "ramdisk");
  grub_off_t *begin_offset = grub_malloc (sizeof *begin_offset);
  if (!begin_offset)
    goto err;
  *begin_offset =
    hd.page_size * (1 + (hd.kernel_size + hd.page_size - 1) / hd.page_size);
  f->data = begin_offset;
  f->size = hd.ramdisk_size;

  *file = f;

  return ret;

err:
  ret = grub_errno;
  if (disk)
    grub_disk_close (disk);

  if (f)
    {
      grub_free (f->device);

      grub_free (f->name);

      grub_free (f);
    }

  return ret;
}
