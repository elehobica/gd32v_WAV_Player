#ifndef _FF_UTIL_H_
#define _FF_UTIL_H_

#include "ff.h"

FRESULT scan_files (char* path_in, int recursive);

void sort_entry_list(DIR *pt_dir_ob, FILINFO *pt_file_info_ob, char entry_list[][13],int32_t max_entry_cnt);
void entry_swap(char entry_list[][13],uint32_t entry_number1,uint32_t entry_number2);
int32_t my_strcmp(char *str1 , char *str2);
int32_t directory_sfn_Search(DIR *pt_dir_ob, char entry_list[][13],FILINFO *pt_file_info_ob);
FRESULT my_f_stat(DIR *pt_dir_ob,char *sfn, FILINFO *pt_file_info_ob);

#endif
