#include "ST7735screen.h"
#include "main.h"

//external spi handle
extern SPI_HandleTypeDef hspi1;


//pin control functions
 void LCD_CS_Low(void)
{
	HAL_GPIO_WritePin(LCD_CS2_GPIO_Port, LCD_CS2_Pin, GPIO_PIN_RESET);


}

 void LCD_CS_High(void)
{
	HAL_GPIO_WritePin(LCD_CS2_GPIO_Port, LCD_CS2_Pin, GPIO_PIN_SET);


}

static void LCD_DC_Low(void)
{
	HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_RESET);


}

 void LCD_DC_High(void)
{
	HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET);


}

static void LCD_RST_Low(void)
{
	HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_RESET);


}

static void LCD_RST_High(void)
{
	HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_SET);


}


//send one byte over spi ;-;
static void SPI_Write(uint8_t data)
{
	HAL_SPI_Transmit(&hspi1, &data, 1, HAL_MAX_DELAY);
}


static void ST7735_WriteCommand(uint8_t cmd)
{
LCD_CS_Low();  //CS LOW telling LCD we talking to him
LCD_DC_Low();    //DC LOW = command
SPI_Write(cmd);
LCD_CS_High(); //cs high telling LCD we are no longer talking to him
}

static void ST7735_WriteData(uint8_t data)
{
LCD_CS_Low();
LCD_DC_High();    //DC HIGH - DATA MODE!!
SPI_Write(data);
LCD_CS_High();
}

void ST7735_Init(void)
{   //init sequence for screen
	LCD_RST_Low();
	HAL_Delay(10);
	LCD_RST_High();
	HAL_Delay(120);

	//pull rst low -> then high to reset controller, delay needed
	//delay in datasheet

	ST7735_WriteCommand(0x11);
	HAL_Delay(120);
	// SLPOUT 0X11 wakes it up and starts clocks have to wait 120 ms after wake

	ST7735_WriteCommand(0x3A);  //COLMOD 0x3A sets how many bits per pixel
	ST7735_WriteData(0x05);  // 0x06 -> 16 bits per pixel RB565

	ST7735_WriteCommand(0x36); /* MADCTL — memory access control, controls display orientation and RGB/BGR order */
	ST7735_WriteData(0x68); // 0x00 normal orientation

	ST7735_WriteCommand(0x29); /* DISPON — display on */ // 0x29 display on
	HAL_Delay(10);


	HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, GPIO_PIN_SET); //enable back light power
}

/* ============================================
   Set drawing window — tells ST7735 which
   region of its RAM we're about to write to
   ============================================ */

void ST7735_SetWindow(uint16_t x0, uint16_t y0,
                              uint16_t x1, uint16_t y1)
{
    /* CASET — Column Address Set (X range) */
    ST7735_WriteCommand(0x2A);
    // 16 bit address 0x0000 to 0x0083
    ST7735_WriteData(0x00); // x start - high byte
    ST7735_WriteData(x0);      /* start column low byte */
    ST7735_WriteData(0x00);	// x end high byte
    ST7735_WriteData(x1);      /* end column high byte, low byte */

    /* RASET — Row Address Set (Y range) */
    ST7735_WriteCommand(0x2B);  //set y range
    ST7735_WriteData(0x00 );
    ST7735_WriteData(y0);      /* start row */
    ST7735_WriteData(0x00);
    ST7735_WriteData(y1);      /* end row */

    //ex setwindow(10, 20, 50, 100) columns 10 to 50, rows 20 to 100

    /* RAMWR — Memory Write, data bytes that follow
       go into display RAM starting at (x0,y0) */
    ST7735_WriteCommand(0x2C);
    //every pixel we send after this command, fills the next pixel in the window
    // when we reach the end of the row (x1) we wrap to next row
    // when we reach end column y1 wrap back to x0 y0
}

void ST7735_FillScreen(uint16_t color)
{
    /* Set window to entire display */
    ST7735_SetWindow(0, 0, ST7735_WIDTH - 1, ST7735_HEIGHT - 1);

    /* Split 16-bit color into two bytes
       ST7735 receives color MSB first then LSB */
    uint8_t hi = color >> 8;    /* upper 8 bits */
    uint8_t lo = color & 0xFF;  /* lower 8 bits */

    /* Write color for every pixel
       128 × 160 = 20,480 pixels × 2 bytes = 40,960 bytes total */
    LCD_CS_Low();
    LCD_DC_High();   /* data mode for pixel writes */
    for (uint32_t i = 0; i < ST7735_WIDTH * ST7735_HEIGHT; i++)
    {
        SPI_Write(hi);
        SPI_Write(lo);
    }
    LCD_CS_High();
}


void ST7735_DrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
	//gaurds against drawing outside of bounds,
	if (x >= ST7735_WIDTH || y >= ST7735_HEIGHT) return;

	ST7735_SetWindow(x, y, x, y);

	//send two color bytes for that pixel
	LCD_CS_Low();
	LCD_DC_High();
	SPI_Write(color >> 8); //shifts upper 8 bits down into lower 8 bit position so spi-write can send as one byte
	SPI_Write(color & 0xFF); //masks zeroes everything except bottom 8 bits, so we send 0xF800 as 0xF8 then 0x00
	LCD_CS_High();

}

void ST7735_DrawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color) {
	// simple vertical line - most useful for spectrum bars
	if (x0 == x1) {
		for (uint16_t y = y0; y <= y1; y++) {
			ST7735_DrawPixel(x0, y, color);
		}
			return;
		}



	// simple horizontal line - most useful for spectrum bars
	if (y0 == y1)
	{
		for (uint16_t x = x0; x <= x1; x++) {
			ST7735_DrawPixel(x, y0, color);
		}
		return;
		}
	}


void ST7735_DrawRect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color) {


	for (uint16_t y = y0; y <= y1; y++) {
		for (uint16_t x = x0; x <= x1; x++) {
			ST7735_DrawPixel(x, y, color);
		}
	}
	return;
}

void ST7735_FillRect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color) {
	uint8_t hi = color >> 8;
	uint8_t lo = color & 0xFF;

	//set window for rectangle
	ST7735_SetWindow(x0, y0, x1, y1);

	uint32_t count = (uint32_t)(x1-x0 +1) * (y1-y0 +1);
	LCD_CS_Low();
	LCD_DC_High();
	for (uint32_t i = 0; i< count; i++)
	{
		SPI_Write(hi);
		SPI_Write(lo);

	}
	LCD_CS_High();

}


//font table
static const uint8_t font5x7[96][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /* 0x20  (space) */
    {0x00,0x00,0x5F,0x00,0x00}, /* 0x21  !       */
    {0x00,0x07,0x00,0x07,0x00}, /* 0x22  "       */
    {0x14,0x7F,0x14,0x7F,0x14}, /* 0x23  #       */
    {0x24,0x2A,0x7F,0x2A,0x12}, /* 0x24  $       */
    {0x23,0x13,0x08,0x64,0x62}, /* 0x25  %       */
    {0x36,0x49,0x55,0x22,0x50}, /* 0x26  &       */
    {0x00,0x05,0x03,0x00,0x00}, /* 0x27  '       */
    {0x00,0x1C,0x22,0x41,0x00}, /* 0x28  (       */
    {0x00,0x41,0x22,0x1C,0x00}, /* 0x29  )       */
    {0x14,0x08,0x3E,0x08,0x14}, /* 0x2A  *       */
    {0x08,0x08,0x3E,0x08,0x08}, /* 0x2B  +       */
    {0x00,0x50,0x30,0x00,0x00}, /* 0x2C  ,       */
    {0x08,0x08,0x08,0x08,0x08}, /* 0x2D  -       */
    {0x00,0x60,0x60,0x00,0x00}, /* 0x2E  .       */
    {0x20,0x10,0x08,0x04,0x02}, /* 0x2F  /       */
    {0x3E,0x51,0x49,0x45,0x3E}, /* 0x30  0       */
    {0x00,0x42,0x7F,0x40,0x00}, /* 0x31  1       */
    {0x42,0x61,0x51,0x49,0x46}, /* 0x32  2       */
    {0x21,0x41,0x45,0x4B,0x31}, /* 0x33  3       */
    {0x18,0x14,0x12,0x7F,0x10}, /* 0x34  4       */
    {0x27,0x45,0x45,0x45,0x39}, /* 0x35  5       */
    {0x3C,0x4A,0x49,0x49,0x30}, /* 0x36  6       */
    {0x01,0x71,0x09,0x05,0x03}, /* 0x37  7       */
    {0x36,0x49,0x49,0x49,0x36}, /* 0x38  8       */
    {0x06,0x49,0x49,0x29,0x1E}, /* 0x39  9       */
    {0x00,0x36,0x36,0x00,0x00}, /* 0x3A  :       */
    {0x00,0x56,0x36,0x00,0x00}, /* 0x3B  ;       */
    {0x08,0x14,0x22,0x41,0x00}, /* 0x3C  <       */
    {0x14,0x14,0x14,0x14,0x14}, /* 0x3D  =       */
    {0x00,0x41,0x22,0x14,0x08}, /* 0x3E  >       */
    {0x02,0x01,0x51,0x09,0x06}, /* 0x3F  ?       */
    {0x32,0x49,0x79,0x41,0x3E}, /* 0x40  @       */
    {0x7E,0x11,0x11,0x11,0x7E}, /* 0x41  A       */
    {0x7F,0x49,0x49,0x49,0x36}, /* 0x42  B       */
    {0x3E,0x41,0x41,0x41,0x22}, /* 0x43  C       */
    {0x7F,0x41,0x41,0x22,0x1C}, /* 0x44  D       */
    {0x7F,0x49,0x49,0x49,0x41}, /* 0x45  E       */
    {0x7F,0x09,0x09,0x09,0x01}, /* 0x46  F       */
    {0x3E,0x41,0x49,0x49,0x7A}, /* 0x47  G       */
    {0x7F,0x08,0x08,0x08,0x7F}, /* 0x48  H       */
    {0x00,0x41,0x7F,0x41,0x00}, /* 0x49  I       */
    {0x20,0x40,0x41,0x3F,0x01}, /* 0x4A  J       */
    {0x7F,0x08,0x14,0x22,0x41}, /* 0x4B  K       */
    {0x7F,0x40,0x40,0x40,0x40}, /* 0x4C  L       */
    {0x7F,0x02,0x0C,0x02,0x7F}, /* 0x4D  M       */
    {0x7F,0x04,0x08,0x10,0x7F}, /* 0x4E  N       */
    {0x3E,0x41,0x41,0x41,0x3E}, /* 0x4F  O       */
    {0x7F,0x09,0x09,0x09,0x06}, /* 0x50  P       */
    {0x3E,0x41,0x51,0x21,0x5E}, /* 0x51  Q       */
    {0x7F,0x09,0x19,0x29,0x46}, /* 0x52  R       */
    {0x46,0x49,0x49,0x49,0x31}, /* 0x53  S       */
    {0x01,0x01,0x7F,0x01,0x01}, /* 0x54  T       */
    {0x3F,0x40,0x40,0x40,0x3F}, /* 0x55  U       */
    {0x1F,0x20,0x40,0x20,0x1F}, /* 0x56  V       */
    {0x3F,0x40,0x38,0x40,0x3F}, /* 0x57  W       */
    {0x63,0x14,0x08,0x14,0x63}, /* 0x58  X       */
    {0x07,0x08,0x70,0x08,0x07}, /* 0x59  Y       */
    {0x61,0x51,0x49,0x45,0x43}, /* 0x5A  Z       */
    {0x00,0x7F,0x41,0x41,0x00}, /* 0x5B  [       */
    {0x02,0x04,0x08,0x10,0x20}, /* 0x5C  \       */
    {0x00,0x41,0x41,0x7F,0x00}, /* 0x5D  ]       */
    {0x04,0x02,0x01,0x02,0x04}, /* 0x5E  ^       */
    {0x40,0x40,0x40,0x40,0x40}, /* 0x5F  _       */
    {0x00,0x01,0x02,0x04,0x00}, /* 0x60  `       */
    {0x20,0x54,0x54,0x54,0x78}, /* 0x61  a       */
    {0x7F,0x48,0x44,0x44,0x38}, /* 0x62  b       */
    {0x38,0x44,0x44,0x44,0x20}, /* 0x63  c       */
    {0x38,0x44,0x44,0x48,0x7F}, /* 0x64  d       */
    {0x38,0x54,0x54,0x54,0x18}, /* 0x65  e       */
    {0x08,0x7E,0x09,0x01,0x02}, /* 0x66  f       */
    {0x0C,0x52,0x52,0x52,0x3E}, /* 0x67  g       */
    {0x7F,0x08,0x04,0x04,0x78}, /* 0x68  h       */
    {0x00,0x44,0x7D,0x40,0x00}, /* 0x69  i       */
    {0x20,0x40,0x44,0x3D,0x00}, /* 0x6A  j       */
    {0x7F,0x10,0x28,0x44,0x00}, /* 0x6B  k       */
    {0x00,0x41,0x7F,0x40,0x00}, /* 0x6C  l       */
    {0x7C,0x04,0x18,0x04,0x78}, /* 0x6D  m       */
    {0x7C,0x08,0x04,0x04,0x78}, /* 0x6E  n       */
    {0x38,0x44,0x44,0x44,0x38}, /* 0x6F  o       */
    {0x7C,0x14,0x14,0x14,0x08}, /* 0x70  p       */
    {0x08,0x14,0x14,0x18,0x7C}, /* 0x71  q       */
    {0x7C,0x08,0x04,0x04,0x08}, /* 0x72  r       */
    {0x48,0x54,0x54,0x54,0x20}, /* 0x73  s       */
    {0x04,0x3F,0x44,0x40,0x20}, /* 0x74  t       */
    {0x3C,0x40,0x40,0x40,0x7C}, /* 0x75  u       */
    {0x1C,0x20,0x40,0x20,0x1C}, /* 0x76  v       */
    {0x3C,0x40,0x30,0x40,0x3C}, /* 0x77  w       */
    {0x44,0x28,0x10,0x28,0x44}, /* 0x78  x       */
    {0x0C,0x50,0x50,0x50,0x3C}, /* 0x79  y       */
    {0x44,0x64,0x54,0x4C,0x44}, /* 0x7A  z       */
    {0x00,0x08,0x36,0x41,0x00}, /* 0x7B  {       */
    {0x00,0x00,0x7F,0x00,0x00}, /* 0x7C  |       */
    {0x00,0x41,0x36,0x08,0x00}, /* 0x7D  }       */
    {0x10,0x08,0x08,0x10,0x08}, /* 0x7E  ~       */
    {0x00,0x00,0x00,0x00,0x00}, /* 0x7F  DEL (blank fallback) */
};

void ST7735_DrawChar(uint16_t x, uint16_t y, char c, uint16_t color, uint16_t bg, uint8_t size)
{
	if (c <0x20 || c > 0x7F) c = 0x7F; //only handle in ASCII range

	//font array starts at 0x20 -> character code - 0x020
	const uint8_t *glyph = font5x7[(uint8_t)c - 0x20];

	for (int8_t col = 0; col < 5; col++)
	{
		uint8_t col_data = glyph[col];

		for (int8_t row = 0; row < 7; row++)
		{
			//example col_data = 0x7E = 0111 1110      row=0 (0x7E >> 0) & 1 = 0  so we make that background
			uint16_t pixel_color = (col_data >> row) & 1 ? color : bg;
			// draws a nxn rect = single pixel depending on size n
			ST7735_FillRect(x + col * size, y + row * size, x+ col* size + size -1,y + row + size - 1, pixel_color);
		}
	}

	//draw 6th column as pure background for a gap between characters
	ST7735_FillRect(x+5 * size, y, x+5 * size + size - 1,y + 7 * size-1, bg);
}

// x , y,
void ST7735_DrawString(int16_t x, int16_t y, const char *str, uint16_t color, uint16_t bg, uint8_t size)
{
	int16_t x0 = x; //remember og x in case new line wrap around
	int16_t char_w = 6 * size; //width of one character cell 5 col + 1 gap
	int16_t char_h = 8 * size; //width of one character cell 7 row + 1 gap

	while (*str) //loop until null terminator
	{
		if (*str == '\n') {

			x = x0; // carriage return to original x
			y+= char_h; //line feed down one char height
		}
		else
		{


			if (x + char_w > 160) //if char would run off x edge move to next line
			{
				x = x0;
				y+= char_h;
			}
		if (y + char_h > 128) break;

		ST7735_DrawChar(x, y, *str, color, bg, size);

		x+= char_w;  //advance cursor right by one char cell

		}
	str++;
	}

}
//DRAW ONE VERTICAL COLUMN OF THE SPECTRUM DISPLAY IN A SINGLE SPI BURST, X column pixel, bar_h height of the signal bar in pixels
//display area y = 20 top,, y=127 bottom
void ST7735_DrawSpectrumBar(uint16_t x, uint8_t bar_h,uint16_t bar_color, uint16_t bg_color) {
	if (x >= ST7735_WIDTH) return;
	if (bar_h > 95) bar_h = 95;

	ST7735_SetWindow(x, 20, x, 115);

	//shift upper bits down so spi can send as one byte and masks zeroes
	//splits 16 bit color into two bytes
	uint8_t bar_hi = bar_color >> 8, bar_lo = bar_color & 0xFF;
	uint8_t bg_hi = bg_color >> 8, bg_lo = bg_color & 0xFF;

	LCD_CS_Low();
	LCD_DC_High();

	uint8_t empty = 95 - bar_h; //send black background for space aove bar
	for (uint8_t p = 0; p < empty; p++) {
		SPI_Write(bg_hi);
		SPI_Write(bg_lo);
	}

	for (uint8_t p = 0; p < bar_h; p++) {
		SPI_Write(bar_hi);
		SPI_Write(bar_lo);
	}

	LCD_CS_High();
}

/*void Draw_FreqAxis(void)
{
	ST7735_FillRect(0, 116, 159, 116, ST7735_WHITE);

	//vertical line ticks to depict diff freq intervals
	const uint8_t tick_x[] = {0, 18, 39, 79, 119, 159};
	for (int i = 0; i < 6; i++) {
		ST7735_FillRect(tick_x[i], 117, tick_x[i], 119, ST7735_WHITE);
	}

		ST7735_DrawString(0,   120, "0",    ST7735_BLUE, ST7735_BLACK, 1);
	    ST7735_DrawString(33,  120, "1k",   ST7735_BLUE, ST7735_BLACK, 1);
	    ST7735_DrawString(73,  120, "2k",   ST7735_BLUE, ST7735_BLACK, 1);
	    ST7735_DrawString(113, 120, "3k",   ST7735_BLUE, ST7735_BLACK, 1);
	    ST7735_DrawString(150, 120, "4k",   ST7735_BLUE, ST7735_BLACK, 1);
} */

void Draw_FreqAxis(uint8_t span)
{
	//uint_t because 4000>255, uint16_t next biggest option that fits 4000
	uint16_t max_hz;

	if (span == 0) max_hz = 4000;
	else if (span == 1) max_hz = 2000;
	else if (span == 2) max_hz = 1000;
	else 					   max_hz = 500;

	ST7735_FillRect(0, 116, 159, 127, ST7735_BLACK); //clear label zone
	ST7735_FillRect(0, 116, 159, 116, ST7735_WHITE); //make white line

	/* tick_freq = actual hz value for each tick
	 * tick_x = which pixel column that freq maps to
	 *
	 * bin = tick_freq / 31.25 ... Hz per bin = sampling rate/N 8000/256
	 * pixel = (bin - 1) * 159 / span_width.. so we combine both pixel = (tick_freq/31.25 -1) * 159 / span_width
	 */

	uint16_t tick_interval = max_hz / 4; //how many hz between each of 5 ticks (4 gaps)

	for (int i = 0; i < 5; i++)
	{
		uint16_t tick_hz = i * tick_interval;

		//convert Hz to pixel position
		uint8_t tick_x;
		//avoiding dividing zero
		if (tick_hz == 0) {
			tick_x = 0;
		}
		else
		{
			//bin for this frequency
			uint16_t bin = tick_hz / 32; //31.25 ~= 32

			//span_width = bin_end - bin_start
			uint16_t span_width;
			if (span == 0) span_width = 126;
				else if (span == 1) span_width = 63;
				else if (span == 2) span_width = 31;
				else 					   span_width = 15;

			//pixel = (bin - 1) * 159 / span_width
			uint16_t px = (uint16_t)((bin - 1) * 159 / span_width);
			tick_x = (px > 159) ? 159 : (uint8_t)px;
		}
		//draw tick mark
		ST7735_FillRect(tick_x, 117, tick_x, 119, ST7735_WHITE);


		//build label string
		//below 1000Hz, show raw, above 100Hz sho Xk.
		char label[8];
		if(tick_hz == 0)
		{
			label[0] = '0';
			label[1] = '\0';
		}
		else if (tick_hz < 1000)
		{
			//snprintf into local buffer, because we ned null terminated string for DrawString(), stack allocation 8bytes deleted when loop iteration ends
			snprintf(label, sizeof(label), "%u", tick_hz);
		}
		else if (tick_hz % 1000 == 0)
		{
			snprintf(label, sizeof(label), "%uk", tick_hz / 1000);
		} else
		{
			snprintf(label, sizeof(label), "%u.%uk", tick_hz / 1000, (tick_hz % 1000) / 100); //1500 % 100 == 500 /100 -> 5... -> 1500 ->1.5k
		}

		//draw label and nudge left by 3 so it centers under tick
		// each char 6px wide, 2-char label 12 px,
				int16_t label_x = (int16_t)tick_x - 3;
		        if (label_x < 0) label_x = 0;          /* don't draw off left edge */

		        ST7735_DrawString(label_x, 120, label, ST7735_BLUE, ST7735_BLACK, 1);
	}


}

void ST7735_StartColumnDMA(void) {
	LCD_CS_Low();
	LCD_DC_High();
}

