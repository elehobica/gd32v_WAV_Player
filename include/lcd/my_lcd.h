#ifndef __MY_LCD_H
#define __MY_LCD_H		

#include "lcd.h"

void LCD_ShowPartialChar(int16_t x,int16_t y,u16 x_min,u16 x_max,u16 y_min,u16 y_max,u8 num,u8 mode,u16 color);
void LCD_ShowIcon(u16 x,u16 y,u8 index,u16 color);
u16 LCD_ShowStringLn(int16_t x,int16_t y,u16 x_min,u16 x_max,const u8 *p,u16 color);
u16 LCD_ShowStringLnOL(int16_t x,int16_t y,u16 x_min,u16 x_max,const u8 *p,u16 color);
void LCD_Scroll_ShowString(int16_t x, int16_t y, u16 x_min, u16 x_max, u8 *p, u16 color, uint16_t *sft_val, uint32_t tick);
void LCD_ShowDimPicture(u16 x1,u16 y1,u16 x2,u16 y2, u8 dim);

#define ICON16x16_TITLE     0
#define ICON16x16_ARTIST    1
#define ICON16x16_ALBUM     2
#define ICON16x16_FOLDER    3
#define ICON16x16_FILE      4
					  		 
#endif  
	 
	 



