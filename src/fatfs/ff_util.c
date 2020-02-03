#include "./fatfs/ff_util.h"
#include <stdio.h>
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

//============================================================
// http://axid.web.fc2.com/file/sort.c

/* Calls LFN from the given entry list (two-dimensional array containing SFN) and compares the strings.
* Sort the entry list from the comparison result in ascending order.
*/
void sort_entry_list(DIR *pt_dir_ob, FILINFO *pt_file_info_ob, char entry_list[][13],int32_t max_entry_cnt)
{
	int i = 0,j = 0,k = 0,result =0,min_number = 0;
	static char temp_min_str[512] = {"0"};
	for(i = 0;i < max_entry_cnt;i++)
	{
		my_f_stat(pt_dir_ob,&entry_list[i][0],pt_file_info_ob);
		// Temporarily save the first element as the smallest LFN string
		if(pt_file_info_ob -> fname[0] != '\0')	//If fname is other than '\ 0', that is, LFN, read LFN.
		{
			for(k = 0;pt_file_info_ob -> fname[k] != '\0';k++)
			{
				temp_min_str[k] = pt_file_info_ob -> fname[k];
			}
		}else	//When only the SFN is used, the SFN is read.
		{
			for(k = 0;pt_file_info_ob -> altname[k] != '\0';k++)
			{
				temp_min_str[k] = pt_file_info_ob -> altname[k];
			}	
		}
		temp_min_str[k] = '\0';
		//Make the current item the minimum value.
		min_number = i;
		for(j = i + 1;j < max_entry_cnt;j++)
		{
			//Load the LFN string from the SFN entry list in the order of the youngest item and compare it with the temporary minimum item
			my_f_stat(pt_dir_ob,&entry_list[j][0],pt_file_info_ob);
			if(pt_file_info_ob -> fname[0] != '\0')
			{
				result = my_strcmp(pt_file_info_ob -> fname, &temp_min_str[0]);
			}else
			{
				result = my_strcmp(pt_file_info_ob -> altname, &temp_min_str[0]);
			}

			//When the minimum item is updated, update temp_min_str and register the minimum number.
			if(result < 0)
			{
				if(pt_file_info_ob -> fname[0] != '\0')
				{
					for(k = 0;pt_file_info_ob -> fname[k] != '\0';k++)
					{
						temp_min_str[k] = pt_file_info_ob -> fname[k];
					}
				}else
				{
					for(k = 0;pt_file_info_ob -> altname[k] != '\0';k++)
					{
						temp_min_str[k] = pt_file_info_ob -> altname[k];
					}
				}
				temp_min_str[k] = '\0';
				min_number = j;
			}	
		}
		//If there is an update, the start value and the minimum value of the entry list are swapped.
		if(min_number != i)
		{
			entry_swap(entry_list, min_number, i);
		}
	}
}
/* Swap specified line of 2D array containing 13 characters (8 +. + 3 + null characters) SFN */
void entry_swap(char entry_list[][13],uint32_t entry_number1,uint32_t entry_number2)
{
	char tmp_entry[13] = {"0"};
	int i = 0;
	for(i = 0; i < 13; i++)
	{
		tmp_entry[i] = entry_list[entry_number1][i];
		entry_list[entry_number1][i] = entry_list[entry_number2][i];
		entry_list[entry_number2][i] = tmp_entry[i];
	}
}

/* Compare LFN of up to 256 characters (2 bytes). The return value is the comparison result.
   Returns positive if str1 is large, negative if str2 is large, and 0 if the strings are the same.
*/
int32_t my_strcmp(char *str1 , char *str2)
{
	int32_t result = 0;

	// Compare until strings do not match
	while(result == 0)
	{
		result = *str1 - *str2;
		if((*str1 == '\0') || (*str2 == '\0')) {break;}	//Comparing also ends when reading to the end of the string
		str1++;
		str2++;
	}
	return (result);
}

/* Search for files in the specified directory and dump the SFN to the entry list.
* For the relative path mode, skip "." And ".." SFN.
   The return value is the total number of files in the directory
*/
int32_t directory_sfn_Search(DIR *pt_dir_ob, char entry_list[][13],FILINFO *pt_file_info_ob)
{	  
	int i = 0,cnt = 0;
	// Rewind directory index
	f_readdir(pt_dir_ob,0);
	// Directory search completed with null character
	for(;;)
	{
		f_readdir(pt_dir_ob,pt_file_info_ob);
		if(pt_file_info_ob -> altname[0] == '\0') {break;}
		if(pt_file_info_ob -> altname[0] == '.') {continue;}
		for(i = 0; i < 13; i++)
		{
			entry_list[cnt][i] = pt_file_info_ob -> altname[i];
		}
		cnt++;
	}
	// Returns the number of entries read
	return (cnt);
} 

/* Search the directory for the FILEINFO structure of the specified SFN.
* LFN information can be obtained from the FILEINFO structure.
* CHAN's f_stat omitted LFN for some reason, so I made this function.
*/
FRESULT my_f_stat(DIR *pt_dir_ob,char *sfn, FILINFO *pt_file_info_ob)
{
	int i =0;
	FRESULT res = FR_OK;
	// Rewind directory index
	f_readdir(pt_dir_ob,0);
	for(;;)
	{
		f_readdir(pt_dir_ob,pt_file_info_ob);
		//If the directory entry has been read to the end, the search fails.
		if(pt_file_info_ob -> altname[0] == '\0') {res = FR_INVALID_NAME; return res;}
		if(pt_file_info_ob -> altname[0] == '.') {continue;}
		for(i = 0; i < 13; i++)
		{
			if(sfn[i] != pt_file_info_ob -> altname[i]) {break;}	//sfn���s��v
			//All sfn matches → search target files. The caller uses FILEINFO at this point.
			if((sfn[i] == '\0') && (pt_file_info_ob -> altname[i] == '\0')) {res = FR_OK; return res;} 
		}
	}
}