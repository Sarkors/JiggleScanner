#ifndef ST7735screen_H
#define ST7735screen_H

#include "main.h"
#include <stdio.h>

//displays dimensions
#define ST7735_WIDTH 160
#define ST7735_HEIGHT 128

#define ST7735_X_OFFSET 1
#define ST7735_Y_OFFSET 1

//define colors, Red 5 upper bits , green 6 mid bits, blue 5 lower bits
#define ST7735_BLACK 0x0000
#define ST7735_WHITE 0xFFFF
#define ST7735_RED 0xF800
#define ST7735_GREEN 0x07E0
#define ST7735_BLUE 0X001F
#define ST7735_YELLOW 0xFFE0

//function decleration
void ST7735_Init(void);
void ST7735_FillScreen(uint16_t color);
void ST7735_DrawPixel(uint16_t X, uint16_t Y, uint16_t color);
void ST7735_DrawString(int16_t x, int16_t y, const char *str, uint16_t color, uint16_t bg, uint8_t size);
void ST7735_DrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
void ST7735_DrawRect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
void ST7735_FillRect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
void ST7735_DrawChar(uint16_t x, uint16_t y, char c, uint16_t color, uint16_t bg, uint8_t size);
void ST7735_DrawSpectrumBar(uint16_t x, uint8_t bar_h,uint16_t bar_color, uint16_t bg_color);
void Draw_FreqAxis(uint8_t span);
void ST7735_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void ST7735_StartColumnDMA(void);
void LCD_CS_Low(void);
void LCD_CS_High(void);
void LCD_DC_High(void);
#endif

//16 bit color per pixel * 128 * 160 pixels = 40,960 bytes of display RAM
