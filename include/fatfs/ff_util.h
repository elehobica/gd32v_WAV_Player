#ifndef _FF_UTIL_H_
#define _FF_UTIL_H_

#include "ff.h"

#define TGT_DIRS    (1<<0)
#define TGT_FILES   (1<<1)

FRESULT scan_files (char* path_in, int recursive);

void sort_entry_list(DIR *pt_dir_ob, FILINFO *fno, char entry_list[][13], int32_t max_entry_cnt);
int32_t make_sfn_list(DIR *pt_dir_ob, char entry_list[][13], int target, FILINFO *fno);
FRESULT my_f_stat(DIR *pt_dir_ob, char *sfn, FILINFO *fno);

#endif
