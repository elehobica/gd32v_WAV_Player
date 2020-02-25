#include "lcd/my_lcd.h"
#include "lcd/iconfont.h"

extern const u8 asc2_1608[1520];

void LCD_ShowIcon(u16 x,u16 y,u8 index,u16 color)
{
	u8 i,j;
	u8 *temp,size1;
	u8 size = 16;
	temp=Icon16;
  LCD_Address_Set(x,y,x+size-1,y+size-1); //设置一个汉字的区域
  size1=size*size/8;//一个汉字所占的字节
	temp+=index*size1;//写入的起始位置
	for(j=0;j<size1;j++)
	{
		for(i=0;i<8;i++)
		{
			if((*temp&(1<<i))!=0)//从数据的低位开始读
			{
				LCD_WR_DATA(color);//点亮
			}
			else
			{
				LCD_WR_DATA(BACK_COLOR);//不点亮
			}
		}
		temp++;
	 }
}

// Show Partial Charactor within rectanble (x_min, y_min)-(x_max, y_max) from the point (x, y)
// num: char code
// mode: 0: non-overlay, 1: overlay
// color: Color
void LCD_ShowPartialChar(int16_t x,int16_t y,u16 x_min,u16 x_max,u16 y_min,u16 y_max,u8 num,u8 mode,u16 color)
{
    u8 temp;
    u8 pos,t;
	u16 x0=x;
	if(x<-8+1||y<-16+1)return;
    if(x>LCD_W-1||y>LCD_H-1)return;
	num=num-' ';
	x_min = (x >= x_min) ? x : x_min;
	y_min = (y >= y_min) ? y : y_min;
	x_max = (x+8-1 <= x_max) ? x+8-1 : x_max;
	y_max = (y+16-1 <= y_max) ? y+16-1 : y_max;
	LCD_Address_Set(x_min,y_min,x_max,y_max);
	if(!mode)
	{
		for(pos=0;pos<16;pos++)
		{ 
			temp=asc2_1608[(u16)num*16+pos];
			for(t=0;t<8;t++)
		    {
				if (x >= x_min && x <= x_max && y >= y_min && y <= y_max) {
					if(temp&0x01)LCD_WR_DATA(color);
					else LCD_WR_DATA(BACK_COLOR);
				}
				temp>>=1;
				x++;
		    }
			x=x0;
			y++;
		}	
	}else
	{
		for(pos=0;pos<16;pos++)
		{
		    temp=asc2_1608[(u16)num*16+pos];
			for(t=0;t<8;t++)
		    {                 
				if (x+t >= x_min && x+t <= x_max && y+pos >= y_min && y+pos <= y_max) {
			        if(temp&0x01)LCD_DrawPoint(x+t,y+pos,color);
				}
		        temp>>=1; 
		    }
		}
	}   	   	 	  
}

// Show String within one line from x_min to x_max
// return: 0: column not overflow, column overflowed
u16 LCD_ShowStringLn(int16_t x,int16_t y,u16 x_min,u16 x_max,const u8 *p,u16 color)
{
	u16 res = 0;
    while(*p!='\0')
    {
        LCD_ShowPartialChar(x,y,x_min,x_max,0,LCD_H-1,*p,0,color);
        if(x>x_max-7){res=1;break;}
        x+=8;
        p++;
    }
	return res;
}

// Show String within one line from x_min to x_max with Overlay
// return: 0: column not overflow, column overflew
u16 LCD_ShowStringLnOL(int16_t x,int16_t y,u16 x_min,u16 x_max,const u8 *p,u16 color)
{         
	u16 res = 0;
    while(*p!='\0')
    {       
        LCD_ShowPartialChar(x,y,x_min,x_max,0,LCD_H-1,*p,1,color);
		if(x>x_max-7){res=1;break;}
        x+=8;
        p++;
    }
	return res;
}

// Show String within one line from x_min to x_max with Auto-Scroll
// When string is within one line, write in overlay (Non-scroll)
// Otherwise, write in non-overlay (Scroll)
void LCD_Scroll_ShowString(int16_t x, int16_t y, u16 x_min, u16 x_max, u8 *p, u16 color, uint16_t *sft_val, uint32_t tick)
{
    if (*sft_val == 0) { // Head display
        if (LCD_ShowStringLnOL(x,  y, x_min, x_max, (u8 *) p, color) && (tick % 16 == 0)) {
            LCD_ShowStringLn(x,  y, x_min, x_max, (u8 *) p, color);
            (*sft_val)++;
        }
    } else {
        if (((x + *sft_val)%8) != 0) {
            // delete head & tail  gabage
            LCD_ShowString(x, y, (u8 *) " ", color);
            LCD_ShowString(LCD_W-8, y, (u8 *) " ", color);
        }
		if (LCD_ShowStringLn(x - (*sft_val)%8, y, x_min, x_max, (u8 *) &p[(*sft_val)/8], color)) {
            (*sft_val)++;
        } else if (tick % 16 == 7) { // Tail display returns back to Head display
            LCD_ShowStringLn(x,  y, x_min, x_max, (u8 *) p, color);
            *sft_val = 0;
        }
    }
}

// dim: 0(dark) ~ 255(original)
void LCD_ShowDimPicture(u16 x1,u16 y1,u16 x2,u16 y2, u8 dim)
{
	int i;
	LCD_Address_Set(x1,y1,x2,y2);
	u16 val;
	u16 r, g, b;
	for(i=0;i<12800;i+=2)
	{
		val = ((u16) image[i] << 8) | ((u16) image[i+1]);
		r = (u16) ((((u32) val & 0xf800) * dim / 255) & 0xf800);
		g = (u16) ((((u32) val & 0x07e0) * dim / 255) & 0x07e0);
		b = (u16) ((((u32) val & 0x001f) * dim / 255) & 0x001f);
		val = r | g | b;
		/*
		r = ((val >> 11) & 0x1f) * dim / 255;
		g = ((val >> 5) & 0x3f)  * dim / 255;
		b = (val & 0x1f) * dim / 255;
		val = ((r&0x1f)<<11) | ((g&0x3f)<<5) | (b&0x1f);
		*/
		LCD_WR_DATA8((val >> 8)&0xff);
		LCD_WR_DATA8(val & 0xff);
	}			
}



