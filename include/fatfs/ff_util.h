#ifndef _FF_UTIL_H_
#define _FF_UTIL_H_

#include "ff.h"

#define TGT_DIRS    (1<<0)
#define TGT_FILES   (1<<1)
#define FFL_SZ 4

FRESULT file_menu_open_dir(const TCHAR *path);
void file_menu_close_dir(void);
uint16_t file_menu_get_max(void);
//void file_menu_sort_entry(uint16_t scope_start, uint16_t scope_end_1);
FRESULT file_menu_get_fname(uint16_t idx, char *str, uint16_t size);
void file_menu_idle(void);

#if 0
uint32_t get_sorted(uint32_t *sorted_flg, uint16_t ofs);
int get_range_full_sorted(uint32_t *sorted_flg, uint16_t start, uint16_t end_1);
int get_range_full_unsorted(uint32_t *sorted_flg, uint16_t start, uint16_t end_1);

void idx_qsort_entry_list_by_lfn_with_key(DIR *pt_dir_ob, int target, FILINFO *fno, FILINFO *fno_temp, uint16_t entry_list[], uint32_t *sorted_flg, char fast_fname_list[][FFL_SZ], uint16_t r_start, uint16_t r_end_1, uint16_t start, uint16_t end_1);
void idx_qsort_range_entry_list_by_lfn(DIR *pt_dir_ob, int target, uint16_t entry_list[], uint32_t *sorted_flg, uint16_t r_start, uint16_t r_end_1, uint16_t max_entry_cnt);
void idx_sort_entry_list_by_lfn(DIR *pt_dir_ob, int target, FILINFO *fno, uint16_t entry_list[], uint16_t max_entry_cnt);
uint16_t idx_get_max(DIR *pt_dir_ob, int target, FILINFO *fno);
FRESULT idx_f_stat(DIR *pt_dir_ob, int target, uint16_t idx, FILINFO *fno);

FRESULT scan_files (char* path_in, int recursive);
void sfn_sort_entry_list_by_lfn(DIR *pt_dir_ob, FILINFO *fno, char entry_list[][13], int32_t max_entry_cnt);
int32_t sfn_make_list(DIR *pt_dir_ob, char entry_list[][13], int target, FILINFO *fno);
FRESULT sfn_f_stat(DIR *pt_dir_ob, char *sfn, FILINFO *fno);
#endif

#endif
