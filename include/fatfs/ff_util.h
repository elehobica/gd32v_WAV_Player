#ifndef _FF_UTIL_H_
#define _FF_UTIL_H_

#include "ff.h"

#define TGT_DIRS    (1<<0)
#define TGT_FILES   (1<<1)
#define FFL_SZ 6

FRESULT scan_files (char* path_in, int recursive);

void idx_qsort_entry_list_by_lfn(DIR *pt_dir_ob, int target, uint16_t entry_list[], uint16_t max_entry_cnt);
void idx_sort_entry_list_by_lfn(DIR *pt_dir_ob, int target, FILINFO *fno, uint16_t entry_list[], uint16_t max_entry_cnt);
uint16_t idx_get_max(DIR *pt_dir_ob, int target, FILINFO *fno);
FRESULT idx_f_stat(DIR *pt_dir_ob, int target, uint16_t idx, FILINFO *fno);

void sfn_sort_entry_list_by_lfn(DIR *pt_dir_ob, FILINFO *fno, char entry_list[][13], int32_t max_entry_cnt);
int32_t sfn_make_list(DIR *pt_dir_ob, char entry_list[][13], int target, FILINFO *fno);
FRESULT sfn_f_stat(DIR *pt_dir_ob, char *sfn, FILINFO *fno);

#endif
