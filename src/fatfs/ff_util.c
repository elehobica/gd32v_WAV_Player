#include "./fatfs/ff_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

FRESULT scan_files (char* path_in, int recursive)
{
    // http://irtos.sourceforge.net/FAT32_ChaN/doc/ja/readdir.html
    FRESULT res;
    FILINFO fno;
    DIR dir;
    int i;
    char *fn;

#if FF_USE_LFN
    char path[FF_MAX_LFN*4];
    strncpy(path, path_in, FF_MAX_LFN*4);
#else
    char path[256];
    strncpy(path, path_in, 256);
#endif

    res = f_opendir(&dir, path);
    if (res == FR_OK) {
        i = strlen(path);
        for (;;) {
            res = f_readdir(&dir, &fno);
            if (res != FR_OK || fno.fname[0] == 0) break;
            if (fno.fname[0] == '.') continue;
#if FF_USE_LFN
            fn = *fno.fname ? fno.fname : fno.altname;
#else
            fn = fno.fname;
#endif
            if (fno.fattrib & AM_DIR) {
                sprintf(&path[i], "/%s", fn);
                if (recursive) {
                    res = scan_files(path, recursive);                   
                    if (res != FR_OK) break;
                } else {
                    printf("[%s/]\n\r", path);
                }
                path[i] = 0;
            } else {
                printf("%s/%s\n\r", path, fn);
            }
        }
    }

    return res;
}

static void idx_entry_swap(uint16_t entry_list[], uint32_t entry_number1, uint32_t entry_number2)
{
	uint16_t tmp_entry;
	tmp_entry = entry_list[entry_number1];
	entry_list[entry_number1] = entry_list[entry_number2];
	entry_list[entry_number2] = tmp_entry;
}

/* Compare LFN of up to 256 characters (2 bytes). The return value is the comparison result.
   Returns positive if str1 is large, negative if str2 is large, and 0 if the strings are the same.
*/
static int32_t my_strcmp(char *str1 , char *str2)
{
	int32_t result = 0;

	// Compare until strings do not match
	while (result == 0) {
		result = *str1 - *str2;
		if ((*str1 == '\0') || (*str2 == '\0')) break;	//Comparing also ends when reading to the end of the string
		str1++;
		str2++;
	}
	return result;
}

uint16_t idx_get_max(DIR *pt_dir_ob, int target, FILINFO *fno)
{
	int16_t cnt = 0;
	// Rewind directory index
	f_readdir(pt_dir_ob, 0);
	// Directory search completed with null character
	for (;;) {
		f_readdir(pt_dir_ob, fno);
		if (fno->fname[0] == '\0') break;
		if (fno->fname[0] == '.') continue;
		if (!(target & TGT_DIRS)) { // File Only
			if (fno->fattrib & AM_DIR) continue;
		} else if (!(target & TGT_FILES)) { // Dir Only
			if (!(fno->fattrib & AM_DIR)) continue;
		}
		cnt++;
	}
	// Returns the number of entries read
	return cnt;
}

static void idx_qsort_entry_list_by_lfn_with_key(DIR *pt_dir_ob, int target, FILINFO *fno, FILINFO *fno_temp, uint16_t entry_list[], char fast_fname_list[][FFL_SZ], uint16_t max_entry_cnt)
{
	int result;
	/*
	printf("\n\r");
	for (int k = 0; k < max_entry_cnt; k++) {
		idx_f_stat(pt_dir_ob, target, entry_list[k], fno);
		printf("before[%d] %d %s\n\r", k, entry_list[k], fno->fname);
	}
	*/
	if (max_entry_cnt <= 2) {
		// try fast_fname_list compare
		result = my_strcmp(fast_fname_list[entry_list[0]], fast_fname_list[entry_list[1]]);
		if (result > 0) {
			idx_entry_swap(entry_list, 0, 1);
		} else if (result < 0) {
			// do nothing
		} else {
			// full name compare
			idx_f_stat(pt_dir_ob, target, entry_list[0], fno);
			idx_f_stat(pt_dir_ob, target, entry_list[1], fno_temp);
			result = my_strcmp(fno->fname, fno_temp->fname);
			if (result >= 0) {
				idx_entry_swap(entry_list, 0, 1);
			}
		}
	} else {
		int top = 0;
		int bottom = max_entry_cnt - 1;
		uint16_t key_idx = entry_list[max_entry_cnt/2];
		idx_f_stat(pt_dir_ob, target, key_idx, fno_temp);
		//printf("key %s\n\r", fno_temp->fname);
		while (1) {
			// try fast_fname_list compare
			result = my_strcmp(fast_fname_list[entry_list[top]], fast_fname_list[key_idx]);
			if (result < 0) {
				top++;
			} else if (result > 0) {
				idx_entry_swap(entry_list, top, bottom);
				bottom--;				
			} else {
				// full name compare
				idx_f_stat(pt_dir_ob, target, entry_list[top], fno);
				result = my_strcmp(fno->fname, fno_temp->fname);
				if (result < 0) {
					top++;
				} else {
					idx_entry_swap(entry_list, top, bottom);
					bottom--;
				}
			}
			if (top > bottom) break;
		}
		/*
		for (int k = 0; k < top; k++) {
			idx_f_stat(pt_dir_ob, target, entry_list[k], fno);
			printf("top[%d] %d %s\n\r", k, entry_list[k], fno->fname);
		}
		for (int k = top; k < max_entry_cnt; k++) {
			idx_f_stat(pt_dir_ob, target, entry_list[k], fno);
			printf("bottom[%d] %d %s\n\r", k, entry_list[k], fno->fname);
		}
		*/
		if (top > 1) {
			idx_qsort_entry_list_by_lfn_with_key(pt_dir_ob, target, fno, fno_temp, &entry_list[0], fast_fname_list, top);
		}
		if (max_entry_cnt - top > 1) {
			idx_qsort_entry_list_by_lfn_with_key(pt_dir_ob, target, fno, fno_temp, &entry_list[top], fast_fname_list, max_entry_cnt - top);
		}
	}
}

void idx_qsort_entry_list_by_lfn(DIR *pt_dir_ob, int target, uint16_t entry_list[], uint16_t max_entry_cnt)
{
	int i, k;
	FILINFO fno0, fno1; // for work
	// prepare default entry list
	for (i = 0; i < max_entry_cnt; i++) entry_list[i] = i;
	// prepare fast_fname_list
	char (*fast_fname_list)[FFL_SZ] = (char (*)[FFL_SZ]) malloc(sizeof(char[FFL_SZ]) * max_entry_cnt);
	for (i = 0; i < max_entry_cnt; i++) {
		idx_f_stat(pt_dir_ob, target, i, &fno0);
		for (k = 0; k < FFL_SZ-1; k++) {
			fast_fname_list[i][k] = fno0.fname[k];
		}
		fast_fname_list[i][k] = '\0';
		//printf("fast_fname_list[%d] = %s\n\r", i, fast_fname_list[i]);
	}
	idx_qsort_entry_list_by_lfn_with_key(pt_dir_ob, target, &fno0, &fno1, entry_list, fast_fname_list, max_entry_cnt);
	free(fast_fname_list);
}

void idx_sort_entry_list_by_lfn(DIR *pt_dir_ob, int target, FILINFO *fno, uint16_t entry_list[], uint16_t max_entry_cnt)
{
	int i, j;
	int k = 0;
	int result =0, min_number = 0;
	static char temp_min_str[512] = {"0"};
	// prepare default entry list
	for (i = 0; i < max_entry_cnt; i++) entry_list[i] = i;
	// sort start
	for (i = 0; i < max_entry_cnt; i++) {
		idx_f_stat(pt_dir_ob, target, entry_list[i], fno);
		printf("i = %d\n\r", i);
		// Temporarily save the first element as the smallest LFN string
		if (fno->fname[0] == '\0') { /* ERROR */ }
		for (k = 0; fno->fname[k] != '\0'; k++) {
			temp_min_str[k] = fno->fname[k];
		}
		temp_min_str[k] = '\0';
		//Make the current item the minimum value.
		min_number = i;
		for (j = i + 1; j < max_entry_cnt; j++) {
			//Load the LFN string from the SFN entry list in the order of the youngest item and compare it with the temporary minimum item
			idx_f_stat(pt_dir_ob, target, entry_list[j], fno);
			if (fno->fname[0] == '\0') { /* ERROR */ }
			result = my_strcmp(fno->fname, &temp_min_str[0]);
			//When the minimum item is updated, update temp_min_str and register the minimum number.
			if (result < 0) {
				if (fno->fname[0] == '\0') { /* ERROR */ }
				for (k = 0; fno->fname[k] != '\0'; k++) {
					temp_min_str[k] = fno->fname[k];
				}
				temp_min_str[k] = '\0';
				min_number = j;
			}
		}
		//If there is an update, the start value and the minimum value of the entry list are swapped.
		if (min_number != i) {
			idx_entry_swap(entry_list, min_number, i);
		}
	}
}

FRESULT idx_f_stat(DIR *pt_dir_ob, int target, uint16_t idx, FILINFO *fno)
{
	static int16_t cnt = 0;
	static int target_prv = 0;
	FRESULT res = FR_OK;
	if (target != target_prv || cnt == 0 || cnt > idx) {
		// Rewind directory index
		f_readdir(pt_dir_ob, 0);
		cnt = 0;
	}
	target_prv = target;
	for (;;) {
		f_readdir(pt_dir_ob, fno);
		if (fno->fname[0] == '\0') {
			res = FR_INVALID_NAME;
			break;
		}
		if (fno->fname[0] == '.') continue;
		if (!(target & TGT_DIRS)) { // File Only
			if (fno->fattrib & AM_DIR) continue;
		} else if (!(target & TGT_FILES)) { // Dir Only
			if (!(fno->fattrib & AM_DIR)) continue;
		}
		if (cnt++ >= idx) break;
	}
	return res;
}

//============================================================
// http://axid.web.fc2.com/file/sort.c

/* Swap specified line of 2D array containing 13 characters (8 +. + 3 + null characters) SFN */
static void sfn_entry_swap(char entry_list[][13], uint32_t entry_number1, uint32_t entry_number2)
{
	char tmp_entry[13] = {"0"};
	int i;
	for (i = 0; i < 13; i++) {
		tmp_entry[i] = entry_list[entry_number1][i];
		entry_list[entry_number1][i] = entry_list[entry_number2][i];
		entry_list[entry_number2][i] = tmp_entry[i];
	}
}

/* Calls LFN from the given entry list (two-dimensional array containing SFN) and compares the strings.
* Sort the entry list from the comparison result in ascending order.
*/
void sfn_sort_entry_list_by_lfn(DIR *pt_dir_ob, FILINFO *fno, char entry_list[][13], int32_t max_entry_cnt)
{
	int i, j;
	int k = 0;
	int result =0, min_number = 0;
	static char temp_min_str[512] = {"0"};
	for (i = 0; i < max_entry_cnt; i++) {
		sfn_f_stat(pt_dir_ob, entry_list[i], fno);
		printf("i = %d\n\r", i);
		// Temporarily save the first element as the smallest LFN string
		if (fno->fname[0] != '\0') { //If fname is other than '\ 0', that is, LFN, read LFN.
			for (k = 0; fno->fname[k] != '\0'; k++) {
				temp_min_str[k] = fno->fname[k];
			}
		}
		temp_min_str[k] = '\0';
		//Make the current item the minimum value.
		min_number = i;
		for (j = i + 1; j < max_entry_cnt; j++) {
			//Load the LFN string from the SFN entry list in the order of the youngest item and compare it with the temporary minimum item
			sfn_f_stat(pt_dir_ob, &entry_list[j][0], fno);
			if (fno->fname[0] != '\0') {
				result = my_strcmp(fno->fname, &temp_min_str[0]);
			}
			//When the minimum item is updated, update temp_min_str and register the minimum number.
			if (result < 0) {
				if (fno->fname[0] != '\0') {
					for (k = 0; fno->fname[k] != '\0'; k++) {
						temp_min_str[k] = fno->fname[k];
					}
				}
				temp_min_str[k] = '\0';
				min_number = j;
			}	
		}
		//If there is an update, the start value and the minimum value of the entry list are swapped.
		if (min_number != i) {
			sfn_entry_swap(entry_list, min_number, i);
		}
	}
}

/* Search for files in the specified directory and dump the SFN to the entry list.
* For the relative path mode, skip "." And ".." SFN.
   The return value is the total number of files in the directory
*/
int32_t sfn_make_list(DIR *pt_dir_ob, char entry_list[][13], int target, FILINFO *fno)
{	  
	int i;
	int cnt = 0;
	// Rewind directory index
	f_readdir(pt_dir_ob, 0);
	// Directory search completed with null character
	for (;;) {
		f_readdir(pt_dir_ob, fno);
		if (fno->fname[0] == '\0') break;
		if (fno->fname[0] == '.') continue;
		if (!(target & TGT_DIRS)) { // File Only
			if (fno->fattrib & AM_DIR) continue;
		} else if (!(target & TGT_FILES)) { // Dir Only
			if (!(fno->fattrib & AM_DIR)) continue;
		}
		for (i = 0; i < 13; i++) {
			entry_list[cnt][i] = fno->altname[i];
		}
		cnt++;
	}
	// Returns the number of entries read
	return cnt;
}

/* Search the directory for the FILEINFO structure of the specified SFN.
* LFN information can be obtained from the FILEINFO structure.
* CHAN's f_stat omitted LFN for some reason, so I made this function.
*/
FRESULT sfn_f_stat(DIR *pt_dir_ob, char *sfn, FILINFO *fno)
{
	int i;
	FRESULT res;
	// Rewind directory index
	f_readdir(pt_dir_ob, 0);
	for (;;) {
		f_readdir(pt_dir_ob, fno);
		//If the directory entry has been read to the end, the search fails.
		if (fno->fname[0] == '\0') {
			res = FR_INVALID_NAME;
			return res;
		}
		if (fno->fname[0] == '.') continue;
		for (i = 0; i < 13; i++) {
			if (sfn[i] != fno->altname[i]) break;	//sfn doesn't match
			//All sfn matches → search target files. The caller uses FILEINFO at this point.
			if ((sfn[i] == '\0') && (fno->altname[i] == '\0')) {
				res = FR_OK;
				return res;
			}
		}
	}
}