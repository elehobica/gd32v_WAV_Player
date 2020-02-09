#include "./fatfs/ff_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static DIR dir;
static FILINFO fno, fno_temp;
int target = TGT_DIRS;  // TGT_DIRS | TGT_FILES
uint16_t max_entry_cnt;
uint16_t *entry_list;
uint32_t *sorted_flg;
char (*fast_fname_list)[FFL_SZ];
uint16_t last_idx;

static void idx_entry_swap(uint16_t entry_list[], uint32_t entry_number1, uint32_t entry_number2)
{
	uint16_t tmp_entry;
	tmp_entry = entry_list[entry_number1];
	entry_list[entry_number1] = entry_list[entry_number2];
	entry_list[entry_number2] = tmp_entry;
}

static int32_t my_strcmp(char *str1 , char *str2)
{
	int32_t result = 0;

	// Compare until strings do not match
	while (result == 0) {
		result = *str1 - *str2;
		if (*str1 == '\0' || *str2 == '\0') break;	//Comparing also ends when reading to the end of the string
		str1++;
		str2++;
	}
	return result;
}

static int32_t my_strncmp(char *str1 , char *str2, int size)
{
	int32_t result = 0;
	int32_t len = 0;

	// Compare until strings do not match
	while (result == 0) {
		result = *str1 - *str2;
		len++;
		if (*str1 == '\0' || *str2 == '\0' || len >= size) break;	//Comparing also ends when reading to the end of the string
		str1++;
		str2++;
	}
	return result;
}

static uint16_t idx_get_max(DIR *pt_dir_ob, int target, FILINFO *fno)
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

static FRESULT idx_f_stat(DIR *pt_dir_ob, int target, uint16_t idx, FILINFO *fno)
{
	static int16_t cnt = 0;
	static int target_prv = 0;
	FRESULT res = FR_OK;
	int error_count = 0;
	if (target != target_prv || cnt == 0 || cnt > idx) {
		// Rewind directory index
		f_readdir(pt_dir_ob, 0);
		cnt = 0;
	}
	target_prv = target;
	for (;;) {
		f_readdir(pt_dir_ob, fno);
		if (fno->fname[0] == '\0') {
			printf("invalid\n\r");
			res = FR_INVALID_NAME;
			if (error_count++ > 10) {
				printf("ERROR: idx_f_stat invalid name\n\r");
				break;
			}
			f_readdir(pt_dir_ob, 0);
			cnt = 0;
			continue;
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

static void set_sorted(uint32_t *sorted_flg, uint16_t ofs)
{
	*(sorted_flg + (ofs/32)) |= 1<<(ofs%32);
}

static uint32_t get_sorted(uint32_t *sorted_flg, uint16_t ofs)
{
	return *(sorted_flg + (ofs/32)) & 1<<(ofs%32);
}

static int get_range_full_sorted(uint32_t *sorted_flg, uint16_t start, uint16_t end_1)
{
	int res = 1;
	for (int i = start; i < end_1; i++) {
		res &= (get_sorted(sorted_flg, i) != 0);
	}
	return res;
}

int get_range_full_unsorted(uint32_t *sorted_flg, uint16_t start, uint16_t end_1)
{
	int res = 0;
	for (int i = start; i < end_1; i++) {
		res |= (get_sorted(sorted_flg, i) != 0);
	}
	return !res;
}

void idx_qsort_entry_list_by_lfn_with_key(DIR *pt_dir_ob, int target, FILINFO *fno, FILINFO *fno_temp, uint16_t entry_list[], uint32_t *sorted_flg, char fast_fname_list[][FFL_SZ], uint16_t r_start, uint16_t r_end_1, uint16_t start, uint16_t end_1)
{
	int result;
	if (r_start < start) r_start = start;
	if (r_end_1 > end_1) r_end_1 = end_1;
	if (get_range_full_sorted(sorted_flg, r_start, r_end_1)) return;
	if (get_range_full_sorted(sorted_flg, start, end_1)) return;
	if (!get_range_full_unsorted(sorted_flg, start, end_1)) {
		int start_next = start;
		while (get_sorted(sorted_flg, start_next)) {
			start_next++;
		}
		int end_1_next = start_next+1;
		while (!get_sorted(sorted_flg, end_1_next) && end_1_next < end_1) {
			end_1_next++;
		}
		printf("partial %d %d %d\n\r", start_next, end_1_next, end_1);
		idx_qsort_entry_list_by_lfn_with_key(pt_dir_ob, target, fno, fno_temp, entry_list, sorted_flg, fast_fname_list, r_start, r_end_1, start_next, end_1_next);
		if (end_1_next < end_1) {
			idx_qsort_entry_list_by_lfn_with_key(pt_dir_ob, target, fno, fno_temp, entry_list, sorted_flg, fast_fname_list, r_start, r_end_1, end_1_next, end_1);
		}
		return;
	}
	///*
	//printf("\n\r");
	/*
	for (int k = start; k < end_1; k++) {
		idx_f_stat(pt_dir_ob, target, entry_list[k], fno);
		printf("before[%d] %d %s\n\r", k, entry_list[k], fno->fname);
	}
	*/
	//*/
	if (end_1 - start <= 2) {
		// try fast_fname_list compare
		result = my_strncmp(fast_fname_list[entry_list[start]], fast_fname_list[entry_list[start+1]], FFL_SZ);
		//printf("fast_fname_list %s %s %d, %d\n\r", fast_fname_list[entry_list[0]], fast_fname_list[entry_list[1]], entry_list[0], entry_list[1]);
		if (result > 0) {
			idx_entry_swap(entry_list, start, start+1);
		} else if (result < 0) {
			// do nothing
		} else {
			// full name compare
			idx_f_stat(pt_dir_ob, target, entry_list[start], fno);
			idx_f_stat(pt_dir_ob, target, entry_list[start+1], fno_temp);
			result = my_strcmp(fno->fname, fno_temp->fname);
			if (result >= 0) {
				idx_entry_swap(entry_list, start, start+1);
			}
		}
		set_sorted(sorted_flg, start);
		set_sorted(sorted_flg, start+1);
	} else {
		int top = start;
		int bottom = end_1 - 1;
		uint16_t key_idx = entry_list[start+(end_1-start)/2];
		idx_f_stat(pt_dir_ob, target, key_idx, fno_temp);
		//printf("key %s\n\r", fno_temp->fname);
		while (1) {
			// try fast_fname_list compare
			result = my_strncmp(fast_fname_list[entry_list[top]], fast_fname_list[key_idx], FFL_SZ);
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
		if (key_idx == 245) {
		for (int k = 0; k < top; k++) {
			idx_f_stat(pt_dir_ob, target, entry_list[k], fno);
			printf("top[%d] %d %s\n\r", k, entry_list[k], fno->fname);
		}
		for (int k = top; k < max_entry_cnt; k++) {
			idx_f_stat(pt_dir_ob, target, entry_list[k], fno);
			printf("bottom[%d] %d %s\n\r", k, entry_list[k], fno->fname);
		}
		}
		*/
		if ((r_start < top && r_end_1 > start) && !get_range_full_sorted(sorted_flg, start, top)) {
			if (top - start > 1) {
				idx_qsort_entry_list_by_lfn_with_key(pt_dir_ob, target, fno, fno_temp, entry_list, sorted_flg, fast_fname_list, r_start, r_end_1, start, top);
			} else {
				set_sorted(sorted_flg, start);
			}
		}
		if ((r_start < end_1 && r_end_1 > top) && !get_range_full_sorted(sorted_flg, top, end_1)) {
			if (end_1 - top > 1) {
				idx_qsort_entry_list_by_lfn_with_key(pt_dir_ob, target, fno, fno_temp, entry_list, sorted_flg, fast_fname_list, r_start, r_end_1, top, end_1);
			} else {
				set_sorted(sorted_flg, top);
			}
		}
	}
}

static void file_menu_sort_entry(uint16_t scope_start, uint16_t scope_end_1)
{
	uint16_t wing;
	uint16_t wing_start, wing_end_1;
	if (scope_start >= scope_end_1) return;
	if (scope_start > max_entry_cnt - 1) scope_start = max_entry_cnt - 1;
	if (scope_end_1 > max_entry_cnt) scope_end_1 = max_entry_cnt;
	wing = (scope_end_1 - scope_start)*2;
	wing_start = (scope_start > wing) ? scope_start - wing : 0;
	wing = (scope_end_1 - scope_start)*4 - (scope_start - wing_start);
	wing_end_1 = (scope_end_1 + wing < max_entry_cnt) ? scope_end_1 + wing : max_entry_cnt;
	//printf("scope_start %d %d %d %d\n\r", scope_start, scope_end_1, wing_start, wing_end_1);
	if (!get_range_full_sorted(sorted_flg, scope_start, scope_end_1)) {
		idx_qsort_entry_list_by_lfn_with_key(&dir, target, &fno, &fno_temp, entry_list, sorted_flg, fast_fname_list, wing_start, wing_end_1, 0, max_entry_cnt);
	}
}

// For implicit sort all entries
void file_menu_idle(void)
{
	static int up_down = 0;
	uint16_t r_start = 0;
	uint16_t r_end_1 = 0;
	if (get_range_full_sorted(sorted_flg, 0, max_entry_cnt)) return;
	for (;;) {
		if (up_down & 0x1) {
			r_start = last_idx + 1;
			while (get_range_full_sorted(sorted_flg, last_idx, r_start) && r_start < max_entry_cnt) {
				r_start++;
			}
			r_start--;
			r_end_1 = r_start + 1;
			while (get_range_full_unsorted(sorted_flg, r_start, r_end_1) && r_end_1 <= max_entry_cnt && r_end_1 - r_start <= 5) {
				r_end_1++;
			}
			r_end_1--;
			up_down++;
		} else {
			if (last_idx > 0) {
				r_end_1 = last_idx - 1;
				while (get_range_full_sorted(sorted_flg, r_end_1, last_idx+1) && r_end_1 != 0) {
					r_end_1--;
				}
				r_end_1++;
				r_start = r_end_1 - 1;
				while (get_range_full_unsorted(sorted_flg, r_start, r_end_1) && r_start != 0 && r_end_1 - r_start <= 5) {
					r_start--;
				}				
			}
			up_down++;
		}
		break;
	}
	
	printf("implicit sort %d %d\n\r", r_start, r_end_1);
	idx_qsort_entry_list_by_lfn_with_key(&dir, target, &fno, &fno_temp, entry_list, sorted_flg, fast_fname_list, r_start, r_end_1, 0, max_entry_cnt);
}

FRESULT file_menu_get_fname(uint16_t idx, char *str, uint16_t size)
{
	FRESULT fr = FR_INVALID_PARAMETER;     /* FatFs return code */
	file_menu_sort_entry(idx, idx+5);
	if (idx < max_entry_cnt) {
		fr = idx_f_stat(&dir, target, entry_list[idx], &fno);
		strncpy(str, fno.fname, size);
		last_idx = idx;
	}
	return fr;
}

uint16_t file_menu_get_max(void)
{
	return max_entry_cnt;
}

FRESULT file_menu_open_dir(const TCHAR *path)
{
	int i, k;
	FRESULT fr;     /* FatFs return code */

	fr = f_opendir(&dir, path);
	if (fr == FR_OK) {
		max_entry_cnt = idx_get_max(&dir, target, &fno);
		entry_list = (uint16_t *) malloc(sizeof(uint16_t) * max_entry_cnt);
		for (i = 0; i < max_entry_cnt; i++) entry_list[i] = i;
		sorted_flg = (uint32_t *) malloc(sizeof(uint32_t) * (max_entry_cnt+31)/32);
		memset(sorted_flg, 0, sizeof(uint32_t) * (max_entry_cnt+31)/32);
		fast_fname_list = (char (*)[FFL_SZ]) malloc(sizeof(char[FFL_SZ]) * max_entry_cnt);
		for (i = 0; i < max_entry_cnt; i++) {
			idx_f_stat(&dir, target, i, &fno);
			for (k = 0; k < FFL_SZ; k++) {
				fast_fname_list[i][k] = fno.fname[k];
			}
		}
	}
	return fr;
}

void file_menu_close_dir(void)
{
    free(entry_list);
    free(sorted_flg);
    free(fast_fname_list);
}

/*
int is_all_same_fast_fname_list(char fast_fname_list[][FFL_SZ], uint16_t start, uint16_t end_1)
{
	int res = 1;
	for (int i = start; i < end_1-1; i++) {
		res &= (strncmp(fast_fname_list[i], fast_fname_list[i+1], FFL_SZ) == 0);
	}
	return res;
}
*/
    /*
    char entry_list[128][13];
    int32_t max_entry_cnt = sfn_make_list(&dir, entry_list, TGT_FILES, &fno);
    sfn_sort_entry_list_by_lfn(&dir, &fno, entry_list, max_entry_cnt);
    for (int i = 0; i < max_entry_cnt; i++) {
        fr = sfn_f_stat(&dir, entry_list[i], &fno);
        printf("%s\n\r", fno.fname);
    }
    */

#if 0
void idx_qsort_range_entry_list_by_lfn(DIR *pt_dir_ob, int target, uint16_t entry_list[], uint32_t *sorted_flg, uint16_t r_start, uint16_t r_end_1, uint16_t max_entry_cnt)
{
	int i, k;
	FILINFO fno0, fno1; // for work
	// prepare default entry list
	for (i = 0; i < max_entry_cnt; i++) entry_list[i] = i;
	// prepare fast_fname_list
	char (*fast_fname_list)[FFL_SZ] = (char (*)[FFL_SZ]) malloc(sizeof(char[FFL_SZ]) * max_entry_cnt);
	for (i = 0; i < max_entry_cnt; i++) {
		idx_f_stat(pt_dir_ob, target, i, &fno0);
		for (k = 0; k < FFL_SZ; k++) {
			fast_fname_list[i][k] = fno0.fname[k];
		}
		//printf("fast_fname_list[%d] = %s\n\r", i, fast_fname_list[i]);
	}
	/*
	for (i = 0; i < max_entry_cnt; i++) {
		printf("fast_fname_list[%d] = %s\n\r", i, fast_fname_list[i]);
	}
	*/
	idx_qsort_entry_list_by_lfn_with_key(pt_dir_ob, target, &fno0, &fno1, entry_list, sorted_flg, fast_fname_list, r_start, r_end_1, 0, max_entry_cnt);
	/*
	for (i = 0; i < max_entry_cnt; i++) {
		printf("fast_fname_list[%d] = %s\n\r", i, fast_fname_list[i]);
	}
	*/
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
#endif