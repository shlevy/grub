/* android_bcb.c - module for interacting with the android bootloader control block */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2016  Free Software Foundation, Inc.
 *
 *  GRUB is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GRUB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <grub/dl.h>
#include <grub/env.h>
#include <grub/disk.h>

GRUB_MOD_LICENSE ("GPLv3+");

/* Definition of struct bootloader message from https://android.googlesource.com/platform/bootable/recovery/+/9d72d4175b06a70c64c8867ff65b3c4c2181c9a9/bootloader.h#20
 * Available under the following copyright and terms:
 *
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
struct bootloader_message
{
  char command[32];
  char status[32];
  char recovery[768];
  // The 'recovery' field used to be 1024 bytes.  It has only ever
  // been used to store the recovery command line, so 768 bytes
  // should be plenty.  We carve off the last 256 bytes to store the
  // stage string (for multistage packages) and possible future
  // expansion.
  char stage[32];
  char slot_suffix[32];
  char reserved[192];
} GRUB_PACKED;

static grub_err_t read_message (const char *name) {
  grub_disk_t disk = grub_disk_open (name);
  if (disk)
    {
      struct bootloader_message msg;
      grub_err_t err = grub_disk_read (disk, 0, 0, sizeof msg, &msg);
      if (!err)
        {
          /* struct bootloader_message has no magic number or other identifier! */
          if (!grub_memchr (msg.command, '\0', sizeof msg.command))
            err = grub_error (GRUB_ERR_BAD_FS,
                              N_("%s doesn't contain a valid bcb"), name);
          else
            grub_env_set ("android_bcb_command", msg.command);
        }

      grub_disk_close (disk);

      return err;
    }
  else
    return grub_errno;
}

static char *handle_write (struct grub_env_var *var __attribute__ ((unused)),
                           const char *val)
{
  if (read_message (val))
    grub_print_error ();

  return grub_strdup (val);
}

GRUB_MOD_INIT(android_bcb)
{
  const char *disk_path = grub_env_get ("android_bcb_disk");
  if (disk_path && read_message (disk_path))
    grub_print_error ();

  if (!grub_register_variable_hook ("android_bcb_disk", 0, handle_write))
    grub_print_error ();
}

GRUB_MOD_FINI(android_bcb)
{
  grub_register_variable_hook ("android_bcb_disk", 0, 0);
}
