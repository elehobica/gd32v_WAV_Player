/*-----------------------------------------------------------/
/ file_menu_FatFs: File Menu sorting utility for FatFs v0.90
/------------------------------------------------------------/
/ Copyright (c) 2020, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/-----------------------------------------------------------*/

#ifndef _FILE_MENU_FATFS_H_
#define _FILE_MENU_FATFS_H_

#include "fatfs/ff.h"

#define TGT_DIRS    (1<<0)
#define TGT_FILES   (1<<1)
#define FFL_SZ 8

FRESULT file_menu_open_dir(const TCHAR *path);
FRESULT file_menu_ch_dir(uint16_t order);
void file_menu_close_dir(void);
uint16_t file_menu_get_size(void);
void file_menu_full_sort(void);
void file_menu_sort_entry(uint16_t scope_start, uint16_t scope_end_1);
FRESULT file_menu_get_fname(uint16_t order, char *str, uint16_t size);
TCHAR *file_menu_get_fname_ptr(uint16_t order);
int file_menu_is_dir(uint16_t order);
void file_menu_idle(void);

#endif
