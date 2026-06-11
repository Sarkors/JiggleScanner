/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "ST7735screen.h"
#include "app_state.h"
#include "dsp.h"
#include "spectrum_display.h"
#include "buttons.h"
#include <stdio.h>


/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */


//struct definition
/*typedef struct { //typedef lets us write just app_state_foo
	uint8_t running; //1 sampling active, 0 paused
	uint8_t scale_mode; //0 auto scale, 1 fixed scale
	uint8_t span; // frequency span index
	uint8_t needs_redraw; // display handler should repaint frame
	float peak_pow; //peak magnitude to scale display
}app_state_t; */




/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/*#define ADC_BUF_LEN 256 //length of our ADC buffer
#define ADC_BUF_HALF (ADC_BUF_LEN >> 1) //divide by 2
#define ADC_FS_HZ 8000 //sampling rate


#define PEAK_HOLD_FRAMES 25  //frames to hold before decaying
#define PEAK_DECAY_RATE 1  //pixels to drop per frame after hold

#define FULL_SCALE_POW  1.701e10f
#define NOISE_FLOOR_POW 1.9e8f //min full scale power - raises noise floor

//spectrum display gemometry
/*#define SPEC_Y_TOP 20u   //first row of spectrum area */
//#define SPEC_Y_BOT 115u  //last row
//#define SPEC_HEIGHT 96u //spec_y_bot - spec_y_top + 1 pxel
//#define SPEC_COLS 160u // one column per screen pixel


//#define COL_BUF_BYTES (SPEC_HEIGHT * 2u) //column buffer in bytes, one DMA transfer sends one vertical strip */



/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
//handle structs, to track state of each peripheral, global so our app can refrence them
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

SPI_HandleTypeDef hspi1;
DMA_HandleTypeDef hdma_spi1_tx;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim3;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
// 1u << 0 = 0b00000001  event 0
#define EVENT_ADC_READY   (1U << 0)
#define EVENT_BTN_RUN     (1U << 1)
#define EVENT_BTN_SCALE   (1U << 2)
#define EVENT_BTN_SPAN_UP (1U << 3)
#define EVENT_BTN_SPAN_DN (1U << 4)
#define EVENT_DSP_READY (1U << 5) //fft complete magnitude[] done
#define EVENT_SPI_IDLE (1U << 6) //dma transfer complete

//#define BTN_DEBOUNCE_MS 50U //ignore retriggers within this window


volatile uint32_t event_flags = 0; // 32 bit variable where every bit represents an event


/*static app_state_t app_state = {
		.running       =1, //start in running so display live on boot
		.scale_mode    =0, // all other fields 0
		.span          =0,
		.needs_redraw  =0,
}; */








//Last press timestamps for debounce
//HAL_GetTick() returns ms since boot as a uint32_t, we store the tick value at the moement each button last fired
//EXTI callback compares current tick against stored tick
//if less than BTN_DEBOUNCE_MS has passed, event discarded (button moves up and down after initial press)
//static volatile uint32_t btn_run_last_tick = 0;  //volatile cuz r/w in ISR context, shouldnt optimize but defensive
//static volatile uint32_t btn_scale_last_tick = 0;
//static volatile uint32_t btn_span_up_last_tick = 0;
//static volatile uint32_t btn_span_dn_last_tick = 0;


//FFT magnitude for each usable bin, written by app_handle_dsp
//read by app_handle_display
//static float magnitude[128];


//96 pix * 2 bytes = 192bytes fits in uint8_t
//state file scope so array lives for entire program lifetime
//static uint8_t col_buf[COL_BUF_BYTES]; //DMA reads from here while CPU calc next col


//SPI DMA state flag
//written in spi call back, read in app_handle_display
//static volatile uint8_t spi_dma_busy = 0; //1 DMA transfer in progress ... 0 DMA idle safe to write col_buf start next transfer
// says that magnitude[] has fresh data for display handler







//DMA writes ADC results here
uint16_t adc_buf[ADC_BUF_LEN]; //ADC data register 16 bits wide,
// only 12 bit resolution, upper 4 bits will be 0 (right aligned in dma ioc)


//static uint32_t capture_count = 0;
//how many completet buffers have been processing, not volatile only main loop read and writes to this

//volatile uint8_t adc_buf_ready = 0; //any variable shared between an ISR and the main loop must be volatile
//uint_8t is safer than a bool, bool might expand to int internally


//PRECOMPUTED TABLES


//we use float to multiply samples by these weights and feed results into a float FF, pipeline in float avoids rounding errors from integer scaling
//ram cost 256 x 4 bytes -> 1024
//lives in ram != not const, cosf() is runtime function C cant evaluate at compile time
//compute once in hanning_init()

//we are not computing dat shit too expensive precomputed TABLE HAAHAHAHAHAA
/*static const float hanning[ADC_BUF_LEN] = {
    0.00000000f, 0.00015177f, 0.00060700f, 0.00136541f, 0.00242654f, 0.00378975f, 0.00545420f, 0.00741888f,
    0.00968261f, 0.01224402f, 0.01510153f, 0.01825343f, 0.02169779f, 0.02543253f, 0.02945537f, 0.03376389f,
    0.03835545f, 0.04322727f, 0.04837640f, 0.05379971f, 0.05949390f, 0.06545553f, 0.07168096f, 0.07816643f,
    0.08490798f, 0.09190154f, 0.09914286f, 0.10662753f, 0.11435102f, 0.12230863f, 0.13049554f, 0.13890677f,
    0.14753723f, 0.15638166f, 0.16543470f, 0.17469085f, 0.18414450f, 0.19378990f, 0.20362120f, 0.21363243f,
    0.22381751f, 0.23417027f, 0.24468440f, 0.25535354f, 0.26617120f, 0.27713082f, 0.28822574f, 0.29944923f,
    0.31079447f, 0.32225458f, 0.33382260f, 0.34549150f, 0.35725421f, 0.36910357f, 0.38103240f, 0.39303346f,
    0.40509945f, 0.41722306f, 0.42939692f, 0.44161365f, 0.45386582f, 0.46614600f, 0.47844673f, 0.49076055f,
    0.50307997f, 0.51539753f, 0.52770574f, 0.53999713f, 0.55226423f, 0.56449961f, 0.57669583f, 0.58884548f,
    0.60094120f, 0.61297564f, 0.62494149f, 0.63683150f, 0.64863843f, 0.66035512f, 0.67197446f, 0.68348940f,
    0.69489294f, 0.70617816f, 0.71733821f, 0.72836632f, 0.73925578f, 0.75000000f, 0.76059244f, 0.77102668f,
    0.78129638f, 0.79139530f, 0.80131732f, 0.81105641f, 0.82060666f, 0.82996227f, 0.83911756f, 0.84806697f,
    0.85680508f, 0.86532657f, 0.87362627f, 0.88169914f, 0.88954029f, 0.89714494f, 0.90450850f, 0.91162647f,
    0.91849455f, 0.92510857f, 0.93146450f, 0.93755849f, 0.94338684f, 0.94894602f, 0.95423264f, 0.95924349f,
    0.96397554f, 0.96842592f, 0.97259191f, 0.97647100f, 0.98006082f, 0.98335920f, 0.98636414f, 0.98907380f,
    0.99148655f, 0.99360092f, 0.99541563f, 0.99692957f, 0.99814183f, 0.99905166f, 0.99965853f, 0.99996206f,
    0.99996206f, 0.99965853f, 0.99905166f, 0.99814183f, 0.99692957f, 0.99541563f, 0.99360092f, 0.99148655f,
    0.98907380f, 0.98636414f, 0.98335920f, 0.98006082f, 0.97647100f, 0.97259191f, 0.96842592f, 0.96397554f,
    0.95924349f, 0.95423264f, 0.94894602f, 0.94338684f, 0.93755849f, 0.93146450f, 0.92510857f, 0.91849455f,
    0.91162647f, 0.90450850f, 0.89714494f, 0.88954029f, 0.88169914f, 0.87362627f, 0.86532657f, 0.85680508f,
    0.84806697f, 0.83911756f, 0.82996227f, 0.82060666f, 0.81105641f, 0.80131732f, 0.79139530f, 0.78129638f,
    0.77102668f, 0.76059244f, 0.75000000f, 0.73925578f, 0.72836632f, 0.71733821f, 0.70617816f, 0.69489294f,
    0.68348940f, 0.67197446f, 0.66035512f, 0.64863843f, 0.63683150f, 0.62494149f, 0.61297564f, 0.60094120f,
    0.58884548f, 0.57669583f, 0.56449961f, 0.55226423f, 0.53999713f, 0.52770574f, 0.51539753f, 0.50307997f,
    0.49076055f, 0.47844673f, 0.46614600f, 0.45386582f, 0.44161365f, 0.42939692f, 0.41722306f, 0.40509945f,
    0.39303346f, 0.38103240f, 0.36910357f, 0.35725421f, 0.34549150f, 0.33382260f, 0.32225458f, 0.31079447f,
    0.29944923f, 0.28822574f, 0.27713082f, 0.26617120f, 0.25535354f, 0.24468440f, 0.23417027f, 0.22381751f,
    0.21363243f, 0.20362120f, 0.19378990f, 0.18414450f, 0.17469085f, 0.16543470f, 0.15638166f, 0.14753723f,
    0.13890677f, 0.13049554f, 0.12230863f, 0.11435102f, 0.10662753f, 0.09914286f, 0.09190154f, 0.08490798f,
    0.07816643f, 0.07168096f, 0.06545553f, 0.05949390f, 0.05379971f, 0.04837640f, 0.04322727f, 0.03835545f,
    0.03376389f, 0.02945537f, 0.02543253f, 0.02169779f, 0.01825343f, 0.01510153f, 0.01224402f, 0.00968261f,
    0.00741888f, 0.00545420f, 0.00378975f, 0.00242654f, 0.00136541f, 0.00060700f, 0.00015177f, 0.00000000f
};

//W_N^k = e^(-j 2πk/N) = cos(-2πk/N) + j·sin(-2πk/N)  N being adc buf size, k ranges from 0 to 127, k is just a rotation factor
// the 128 complex values wont change  so we precompute them , mathlibrary to big!!
//tw_cos[0] = 1.0
static const float tw_cos[128] = {
      1.00000000f,   0.99969882f,   0.99879546f,   0.99729046f,   0.99518473f,   0.99247953f,   0.98917651f,   0.98527764f,
      0.98078528f,   0.97570213f,   0.97003125f,   0.96377607f,   0.95694034f,   0.94952818f,   0.94154407f,   0.93299280f,
      0.92387953f,   0.91420976f,   0.90398929f,   0.89322430f,   0.88192126f,   0.87008699f,   0.85772861f,   0.84485357f,
      0.83146961f,   0.81758481f,   0.80320753f,   0.78834643f,   0.77301045f,   0.75720885f,   0.74095113f,   0.72424708f,
      0.70710678f,   0.68954054f,   0.67155895f,   0.65317284f,   0.63439328f,   0.61523159f,   0.59569930f,   0.57580819f,
      0.55557023f,   0.53499762f,   0.51410274f,   0.49289819f,   0.47139674f,   0.44961133f,   0.42755509f,   0.40524131f,
      0.38268343f,   0.35989504f,   0.33688985f,   0.31368174f,   0.29028468f,   0.26671276f,   0.24298018f,   0.21910124f,
      0.19509032f,   0.17096189f,   0.14673047f,   0.12241068f,   0.09801714f,   0.07356456f,   0.04906767f,   0.02454123f,
      0.00000000f,  -0.02454123f,  -0.04906767f,  -0.07356456f,  -0.09801714f,  -0.12241068f,  -0.14673047f,  -0.17096189f,
     -0.19509032f,  -0.21910124f,  -0.24298018f,  -0.26671276f,  -0.29028468f,  -0.31368174f,  -0.33688985f,  -0.35989504f,
     -0.38268343f,  -0.40524131f,  -0.42755509f,  -0.44961133f,  -0.47139674f,  -0.49289819f,  -0.51410274f,  -0.53499762f,
     -0.55557023f,  -0.57580819f,  -0.59569930f,  -0.61523159f,  -0.63439328f,  -0.65317284f,  -0.67155895f,  -0.68954054f,
     -0.70710678f,  -0.72424708f,  -0.74095113f,  -0.75720885f,  -0.77301045f,  -0.78834643f,  -0.80320753f,  -0.81758481f,
     -0.83146961f,  -0.84485357f,  -0.85772861f,  -0.87008699f,  -0.88192126f,  -0.89322430f,  -0.90398929f,  -0.91420976f,
     -0.92387953f,  -0.93299280f,  -0.94154407f,  -0.94952818f,  -0.95694034f,  -0.96377607f,  -0.97003125f,  -0.97570213f,
     -0.98078528f,  -0.98527764f,  -0.98917651f,  -0.99247953f,  -0.99518473f,  -0.99729046f,  -0.99879546f,  -0.99969882f
};


static const float tw_sin[128] = {
     -0.00000000f,  -0.02454123f,  -0.04906767f,  -0.07356456f,  -0.09801714f,  -0.12241068f,  -0.14673047f,  -0.17096189f,
     -0.19509032f,  -0.21910124f,  -0.24298018f,  -0.26671276f,  -0.29028468f,  -0.31368174f,  -0.33688985f,  -0.35989504f,
     -0.38268343f,  -0.40524131f,  -0.42755509f,  -0.44961133f,  -0.47139674f,  -0.49289819f,  -0.51410274f,  -0.53499762f,
     -0.55557023f,  -0.57580819f,  -0.59569930f,  -0.61523159f,  -0.63439328f,  -0.65317284f,  -0.67155895f,  -0.68954054f,
     -0.70710678f,  -0.72424708f,  -0.74095113f,  -0.75720885f,  -0.77301045f,  -0.78834643f,  -0.80320753f,  -0.81758481f,
     -0.83146961f,  -0.84485357f,  -0.85772861f,  -0.87008699f,  -0.88192126f,  -0.89322430f,  -0.90398929f,  -0.91420976f,
     -0.92387953f,  -0.93299280f,  -0.94154407f,  -0.94952818f,  -0.95694034f,  -0.96377607f,  -0.97003125f,  -0.97570213f,
     -0.98078528f,  -0.98527764f,  -0.98917651f,  -0.99247953f,  -0.99518473f,  -0.99729046f,  -0.99879546f,  -0.99969882f,
     -1.00000000f,  -0.99969882f,  -0.99879546f,  -0.99729046f,  -0.99518473f,  -0.99247953f,  -0.98917651f,  -0.98527764f,
     -0.98078528f,  -0.97570213f,  -0.97003125f,  -0.96377607f,  -0.95694034f,  -0.94952818f,  -0.94154407f,  -0.93299280f,
     -0.92387953f,  -0.91420976f,  -0.90398929f,  -0.89322430f,  -0.88192126f,  -0.87008699f,  -0.85772861f,  -0.84485357f,
     -0.83146961f,  -0.81758481f,  -0.80320753f,  -0.78834643f,  -0.77301045f,  -0.75720885f,  -0.74095113f,  -0.72424708f,
     -0.70710678f,  -0.68954054f,  -0.67155895f,  -0.65317284f,  -0.63439328f,  -0.61523159f,  -0.59569930f,  -0.57580819f,
     -0.55557023f,  -0.53499762f,  -0.51410274f,  -0.49289819f,  -0.47139674f,  -0.44961133f,  -0.42755509f,  -0.40524131f,
     -0.38268343f,  -0.35989504f,  -0.33688985f,  -0.31368174f,  -0.29028468f,  -0.26671276f,  -0.24298018f,  -0.21910124f,
     -0.19509032f,  -0.17096189f,  -0.14673047f,  -0.12241068f,  -0.09801714f,  -0.07356456f,  -0.04906767f,  -0.02454123f
};

//in SRAM so change every frame, (static outside of a function means they work in this files scope only)
//static prevents other .c files from linking against these
//CMSIS DSP FFT operates on float32,
static float fft_buf[ADC_BUF_LEN];


//FFT operates on real and imaginary, adc signal is real,
//so we start with im_buf[i] = 0, after fft runs, im_buf[i] has imaginary part of frequency domain output
static float im_buf[ADC_BUF_LEN];   /* imaginary part, zero before each FFT */

/*


//uint_8t because bar heightsa are 0-95, fit in 0-255  && timer counts up to peak hold frames fits in uint8
static uint8_t peak_hold[160];  //current peak heigh for each x column
static uint8_t peak_hold_timer[160];  //frames remaining until peak starts decaying
//keep them invis to other .c files
*/


/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_ADC1_Init(void);
static void MX_SPI1_Init(void);
static void MX_TIM1_Init(void);
static void MX_TIM3_Init(void);
static void MX_USART2_UART_Init(void);



/* USER CODE BEGIN PFP */
//application handler, one per event type
//static void App_Handle_ADC(app_state_t *state);
//static void App_Handle_DSP(app_state_t *state); // FFT + magnitude
//static void App_Handle_Display(app_state_t *state); //draw frame
//static void App_Handle_Btn_RunPause(app_state_t *state);
//static void App_Handle_Btn_Scale(app_state_t *state);
//static void App_Handle_Btn_SpanUp(app_state_t *state);
//static void App_Handle_Btn_SpanDn(app_state_t *state);

//fft function prototype
//void FFT256(float *re, float *im);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// computes the 256-point hanning window into hanning[]
// n=0  -> w=0    n=64 w=0.5 halfway     n=127 w =1.000 peak weight       n=191  w=0.5    n=255  w=0
//were giving a weight to each sample depending on its location within the buffer
//now we are just using a table so we dont have use the math.h library


static app_state_t app_state = { .running = 1 };
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
	
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_SPI1_Init();
  MX_TIM1_Init();
  MX_TIM3_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */


  ST7735_Init();
  ST7735_FillScreen(ST7735_BLACK);
  
  ST7735_DrawString(0, 0, "ADC+DMA init...", ST7735_WHITE, ST7735_BLACK,1);


  //printf("Hanning: w[0]=%d  w[127]=%d  w[255]=%d  (x10000)\r\n",(int)(hanning[0]   * 10000),(int)(hanning[127] * 10000),(int)(hanning[255] * 10000));


  printf("=== jigglescanner stage 1.2 ===\r\n");
      printf("Buffer:    %d samples (uint16_t)\r\n", ADC_BUF_LEN);
      printf("Fs:        %d Hz\r\n", ADC_FS_HZ);

      printf("Nyquist:   %d Hz (max visible freq)\r\n", ADC_FS_HZ / 2);


      //corrects for internal offset/gain errors
  if (HAL_ADCEx_Calibration_Start(&hadc1) != HAL_OK) //this runs built in self-calibration sequence
  {
	  printf("ERROR: ADC calibration no bueno!\r\n");
	  Error_Handler();
  }
  printf("ADC calibration complete. \r\n");


  //start TIM3 -> TGRO EVENT -> ADC trigger
  //TIM3 period = 18, tuned to get sample rate = 8000Hz
  if (HAL_TIM_Base_Start(&htim3) != HAL_OK)
  { //Starts counter in basic mode, TRGO output fire on eahch update event, sets TIM3_CR1.CEN=1
//update event -> TIM3-CR2.MMS = 010
	  printf("ERROR: TIM3 start notchy very goode");
	  Error_Handler();
  }
  printf("TIM3 started  (ADC trigger, Fs=%d Hz)\r\n", ADC_FS_HZ);


  //TIM1 - PWM 1kHz test signal
  if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1) != HAL_OK) {  //TIM1 start for test signal
	  printf("ERROR: TIM1 PWM start failed!\r\n");
	         Error_Handler();
	     }
  printf("TIM1 PWM started  (1kHz test signal on PA8)\r\n");




  //HAL_ADC_Start_DMA enables ADC peripheral (set ADC_CR.ADEN=1)
  /*  configures DMA1_Channel1 w/
   * src address = ADC1->DR   adc data register
   *  dest address = adc_buf (our buffer in static ram)
   * transfer count = ADC_BUF_LEN
   * mode = circuilar
   *
   * were casting (uint32_t*) cause HAL function takes that for compatiblity across diff ADC resolutions wierd cause we have 12 bit resolution ;-;
   */
  if (HAL_ADC_Start_DMA(&hadc1, (uint32_t*) adc_buf, ADC_BUF_LEN) != HAL_OK)
  {
	  printf("ERROR: ADC DMA start failed!\r\n");
	          Error_Handler();
  }
  printf("ADC+DMA started.\r\n");
  printf("Wire PA8 to PA7 if not done, then watch for samples...\r\n\r\n");

  ST7735_FillScreen(ST7735_BLACK);
  ST7735_DrawString(0, 0, "Sampling...", ST7735_GREEN, ST7735_BLACK, 1);
  Draw_FreqAxis(app_state.span);

  HAL_GPIO_WritePin(LED_ACTIVITY_GPIO_Port, LED_ACTIVITY_Pin, GPIO_PIN_SET);


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  	  while (1)
  	  {
    /* USER CODE END WHILE */
  		  //ADC raw data ready -> run dsp
  		if (event_flags & EVENT_ADC_READY)
  		    {
  		        event_flags &= ~EVENT_ADC_READY;
  		        if (app_state.running)
  		        {
  		            App_Handle_DSP(&app_state);
  		        }
  		    }

  		  //DSP complete -> draw frame
  		  if (event_flags & EVENT_DSP_READY)
  		  {
  			  event_flags &= ~EVENT_DSP_READY;
  			  App_Handle_Display(&app_state);
  		  }


  		  //SPI DMA col complete
  		  //spi_dma_busy flag, clearing event_spi_idle here keeps flag register clear
  		  if (event_flags & EVENT_SPI_IDLE)
  		  {
  			  event_flags &= ~EVENT_SPI_IDLE;
  		  }


  		/* Buttons — handled after ADC so DSP is never delayed by UI.
  		     * Priority order: run/pause first (most important), then the rest. */
  		    if (event_flags & EVENT_BTN_RUN)
  		    {
  		        event_flags &= ~EVENT_BTN_RUN;
  		        App_Handle_Btn_RunPause(&app_state);
  		    }

  		    if (event_flags & EVENT_BTN_SCALE)
  		    {
  		        event_flags &= ~EVENT_BTN_SCALE;
  		        App_Handle_Btn_Scale(&app_state);
  		    }

  		    if (event_flags & EVENT_BTN_SPAN_UP)
  		    {
  		        event_flags &= ~EVENT_BTN_SPAN_UP;
  		        App_Handle_Btn_SpanUp(&app_state);
  		    }

  		    if (event_flags & EVENT_BTN_SPAN_DN)
  		    {
  		        event_flags &= ~EVENT_BTN_SPAN_DN;
  		        App_Handle_Btn_SpanDn(&app_state);
  		    }
    /* USER CODE BEGIN 3 */
  	  }
}



/*



	/*static void App_Handle_ADC(app_state_t *state)
	  {


				  capture_count++; //iterate how many buffers we got

		  uint16_t min_val = 0XFFFFu;//start high will only decrease
		  uint16_t max_val = 0; //start low will only increase
		  uint32_t sum = 0;  // uint32_t to avoid overflow  max value 4095 x 256 > more than uint16_t max

		  //get the max voltage and min and their sum for stats
		  for (int i = 0; i < ADC_BUF_LEN; i++) {
			  if (adc_buf[i] < min_val) min_val = adc_buf[i];
			  if (adc_buf[i] > max_val) max_val = adc_buf[i];
			  sum += adc_buf[i];
		  }

		  //more stats
		  uint16_t mean_val  = (uint16_t)(sum / ADC_BUF_LEN);
		  uint16_t amplitude = max_val - min_val;

		  //print stats
		  if (capture_count <= 5 || (capture_count % 30) ==0)
		  {
		  printf("[%4lu] min=%4u  max=%4u  mean=%4u  amp=%4u\r\n",capture_count, min_val, max_val, mean_val, amplitude);
		  }
		  //cast our mean to float
		  float mean_f = (float)mean_val;

		  		  //hanning window, samples near edges get * by 0 SILENCED.. near the middle get multiply by 1~ let in
		  		  for (int i = 0; i < ADC_BUF_LEN; i++)
		  		  {
		  			  fft_buf[i] = ((float)adc_buf[i] - mean_f) * hanning[i];
		  			  im_buf[i] = 0.0f; //imaginary part must be zeroed at each fram, our prev FFT output leaks into next FFT calc
		  		  }

		  		 FFT256(fft_buf, im_buf);
		  		 //bin i represents frequency i * (Fs/ N) = i * 31.25 Hz

		  		 uint32_t peak_bin = 1;
		  		 float peak_pow = 0.0f ;

		  //we skip bin 0, (thats the DC) and only store bins 1-127
		  for (int i = 1; i < ADC_BUF_LEN/2; i++)
		  		 {
		  		 float power = fft_buf[i]*fft_buf[i] + im_buf[i]*im_buf[i];
		  		 magnitude[i-1] = power; //magnitude[0] = bin 1
		  			 if (power > peak_pow)
		  			 {
		  				 peak_pow = power;   //getting peak power and peak bin
		  			     peak_bin = i;
		  			}
		  		 }
		  	  //store peak power in state so display handler can scale from it
		  	  //we add to app_state
		  	  state->peak_pow = (peak_pow > NOISE_FLOOR_POW) ? peak_pow : NOISE_FLOOR_POW;

		  	  event_flags |= EVENT_DSP_READY;
	  }



		  //print first 16 raw samples
		  //print them on UART
		  if (capture_count <= 5 || (capture_count % 30) == 0)
		  {
			 uint32_t peak_hz = (uint32_t)peak_bin * ADC_FS_HZ / ADC_BUF_LEN;
			 printf("[%4lu] amp=%4u  peak=bin%lu(%luHz)  pow=%lu(x1M\r\n", //if peak is bin 1 or 2 DC not removed
			                    capture_count,
			                    amplitude,
			                    peak_bin,
			                    peak_hz,
			                    (uint32_t)(peak_pow /1000000));
	  } */

/*static void App_Handle_Display(app_state_t *state) {

		  //spectrum bar display
		  //we map 127 FFT bins to 160 LCD pixels
		  //bar_h is in range of 0 to 95  vertical

		  //this way tallest bin always fills display weaker bin relative to tallest
		  //tune noise floor pow, raise to suppress more noise, lower it to see more noise


		  //float effective_peak = (peak_pow > NOISE_FLOOR_POW) ? peak_pow : NOISE_FLOOR_POW;
		  float scale = 95.0f / state->peak_pow;

		  for (int x = 0; x < (int)SPEC_COLS; x++)
		  {

			  //spread bins 1 through 127 across 160px
			  int bin = 1 + (x * 127) / 160;
			  if (bin > 126) bin = 126; //bin 127 is nyquist


			  float power = magnitude[bin];  //power = re{k}^2 + im{k}^2
			  //scaling the bar_h
			  int bar_h = (int)(power * scale);
			  if (bar_h > 95) bar_h = 95;
			  if (bar_h < 0) bar_h = 0;


			  //if this frames bar is taller than the stored peak , reset peak else count down the hold timer and start decaying
			  if ((uint8_t)bar_h >= peak_hold[x])
			  {
				  peak_hold[x] = (uint8_t)bar_h;
				  peak_hold_timer[x] = PEAK_HOLD_FRAMES;
			  }
			  else
			  {
				  if (peak_hold_timer[x] > 0)
				  {
					  peak_hold_timer[x]--; //still holding dont decay
				  }
				  else if (peak_hold[x] > 0) //hold ended start decaying
				  {
					  peak_hold[x] -= PEAK_DECAY_RATE;
				  }
			  }

			  //col_buf holds SPEC_HEIGHT pixels for one colun top to bottom
			  //top section bg... bottom section bar color... peak dot one white pixel where the peak hold is
			  uint8_t bar_hi = ST7735_GREEN >> 8;
			  uint8_t bar_lo = ST7735_GREEN & 0XFF;
			  uint8_t bg_hi = ST7735_BLACK >> 8;
			  uint8_t bg_lo = ST7735_BLACK & 0XFF;
			  uint8_t pk_hi = ST7735_GREEN >> 8;
			  uint8_t pk_lo = ST7735_GREEN & 0XFF;

              //SPEC_HEIGHT screen column height,
			  uint8_t empty = (uint8_t)(SPEC_HEIGHT - (uint8_t)bar_h);
			  int dot_row = (int)SPEC_HEIGHT - 1 - (int) peak_hold[x];

			  for (int row=0; row < (int)SPEC_HEIGHT; row++)
			  {
				  uint8_t hi, lo;
				  if (row == dot_row && peak_hold[x] > (uint8_t)bar_h)
				  {
					  hi = pk_hi; lo = pk_lo; //peak hold dot
				  }
				  else if (row < (int)empty)
				  {
					  hi = bg_hi; lo = bg_lo; //baackground above bar
				  }
				  else
				  {
					  hi = bar_hi; lo = bar_lo; //bar
				  }
				  col_buf[row *2] = hi;
				  col_buf[row *2 + 1] = lo;
			  }

			  //buffer hazard gaurd, if prev col DMA still reading col_buf dont overwrite yet
			  //dma asynchronus, spinning on flag rather than checking once
			  while (spi_dma_busy) { }

			  //setting window for this column and start dma
				  ST7735_SetWindow(x, SPEC_Y_TOP, x, SPEC_Y_BOT);

				  spi_dma_busy = 1;

				  ST7735_StartColumnDMA();
				  HAL_SPI_Transmit_DMA(&hspi1, col_buf, COL_BUF_BYTES); //returns immediately after setting up DMA
				  //we raise CS in the call back because DMA still sending
		  }
		  //waiting for the last column DMA to finish before drawing freq axis overlay,
		  while (spi_dma_busy) {}

		  char status_buf[32];
		  		  snprintf(status_buf, sizeof(status_buf), "A:%4u #%lu", (uint16_t)0, capture_count);  //writes data into a char buffer
		  		  ST7735_DrawString(0, 0, status_buf, ST7735_YELLOW, ST7735_BLACK, 1);
}
				  //draw peak hold dot, white pixel one row above peak level, only draw if peak is above current bar,
				  if (peak_hold[x] > (uint8_t)bar_h && peak_hold[x] > 0)
				  {
					  //convert bar_h to y pixel, bar fills from y=115 upward,
					  int dot_y = 115 - (int)peak_hold[x]; //peak_hold[x] pixels tall -> top of peak at y = 115 - peak_hold[x]
					  if (dot_y >= 20) ST7735_DrawPixel(x, dot_y, ST7735_WHITE);
				  }

		  }



		  //check to see if hanning init ran or not, if fft_buf[0] not near zero SOEMTHING WRONG!!
		  if (capture_count <= 3)
		      {
			  printf("  fft_buf: [0]=%d  [127]=%d  [255]=%d\r\n",
			         (int)fft_buf[0],
			         (int)fft_buf[127],
			         (int)fft_buf[255]);
		      }

		  char status_buf[32];
		  snprintf(status_buf, sizeof(status_buf), "A:%4u #%lu", amplitude, capture_count);  //writes data into a char buffer
		  ST7735_DrawString(0, 0, status_buf, ST7735_YELLOW, ST7735_BLACK, 1);


		  state->needs_redraw = 0; //go to address states point to and change the needs_redraw field
  } */






/*	void FFT256(float *re, float *im)
	{

		//standard bit reverse, j tracks the bit reversed index of i, when i<j, swap - ensure each pair is swapped once
		//We just swap re[i]↔re[j] and im[i]↔im[j] for all i<j pairs.
		for (int i =1, j = 0; i < ADC_BUF_LEN; i++)
		{
			int bit = ADC_BUF_LEN >> 1;
			for (; j & bit; bit >>= 1) j ^= bit;
			j ^= bit;
			if (i < j)
			{
				float t;
				t= re[i]; re[i] = re[j]; re[j] = t;
				t = im[i]; im[i] = im[j]; im[j] = t;
			}
		}

		//len is the current butterfly size 2-> 4 -> 8
		//step is how far apart the twiddle indexes are 128 -> 64 -> 2
		// at len = 2, step =128, tw[0]= 1+0j    at len =4  step=64  tw[0] and tw[64] 1+0j and 0-1j
		// at len=256, step = 1: all 128 tw[i] used
		for (int len = 2; len <= ADC_BUF_LEN; len <<= 1)
		{
			int step = ADC_BUF_LEN /len; //twiddle table stride length

			for (int i = 0; i < ADC_BUF_LEN; i += len) //
			{
				for (int j = 0; j < len / 2; j++)
				{
					int tw_idx = j * step; //index into tw_cos/tw_sin

					//w = twiddle factor at this position
				float wr = tw_cos[tw_idx];
				float wi = tw_sin[tw_idx];

				//u top half of element
				float ur = re[i + j];
				float ui = im[i + j];

				//V = bottom half of element
				W*V = (wr + j*wi)(vr + j*vi) = wr*vr - wi*vi + j(wr*vi + wi*vr) */
		/*		float vr = re[i + j + len/2] * wr - im[i + j + len/2] * wi;
				float vi = re[i + j + len/2] * wi + im[i + j + len/2] * wr;

				// butterfly top = U + W*V, bottom U - W*V
				re[i + j]         = ur + vr;
				im[i + j]         = ui + vi;
				re[i + j + len/2] = ur - vr;
				im[i + j + len/2] = ui - vi;
				}
				}
			}
		}



	static void App_Handle_DSP(app_state_t *state)
	{
		  capture_count++; //iterate how many buffers we got

				  uint16_t min_val = 0XFFFFu;//start high will only decrease
				  uint16_t max_val = 0; //start low will only increase
				  uint32_t sum = 0;  // uint32_t to avoid overflow  max value 4095 x 256 > more than uint16_t max

				  //get the max voltage and min and their sum for stats
				  for (int i = 0; i < ADC_BUF_LEN; i++) {
					  if (adc_buf[i] < min_val) min_val = adc_buf[i];
					  if (adc_buf[i] > max_val) max_val = adc_buf[i];
					  sum += adc_buf[i];
				  }

				  //more stats
				  uint16_t mean_val  = (uint16_t)(sum / ADC_BUF_LEN);
				  uint16_t amplitude = max_val - min_val;

				  //print stats
				  if (capture_count <= 5 || (capture_count % 30) ==0)
				  {
				  printf("[%4lu] min=%4u  max=%4u  mean=%4u  amp=%4u\r\n",capture_count, min_val, max_val, mean_val, amplitude);
				  }
				  //cast our mean to float
				  float mean_f = (float)mean_val;

				  		  //hanning window, samples near edges get * by 0 SILENCED.. near the middle get multiply by 1~ let in
				  		  for (int i = 0; i < ADC_BUF_LEN; i++)
				  		  {
				  			  fft_buf[i] = ((float)adc_buf[i] - mean_f) * hanning[i];
				  			  im_buf[i] = 0.0f; //imaginary part must be zeroed at each fram, our prev FFT output leaks into next FFT calc
				  		  }

				  		 FFT256(fft_buf, im_buf);
				  		 //bin i represents frequency i * (Fs/ N) = i * 31.25 Hz

				  		 uint32_t peak_bin = 1;
				  		 float peak_pow = 0.0f ;

				  //we skip bin 0, (thats the DC) and only store bins 1-127
				  for (int i = 1; i < ADC_BUF_LEN/2; i++)
				  		 {
				  		 float power = fft_buf[i]*fft_buf[i] + im_buf[i]*im_buf[i];
				  		 magnitude[i-1] = power; //magnitude[0] = bin 1
				  			 if (power > peak_pow)
				  			 {
				  				 peak_pow = power;   //getting peak power and peak bin
				  			     peak_bin = i;
				  			}
				  		 }
				  	  //store peak power in state so display handler can scale from it
				  	  //we add to app_state
				  	  state->peak_pow = (peak_pow > NOISE_FLOOR_POW) ? peak_pow : NOISE_FLOOR_POW;

				  	  event_flags |= EVENT_DSP_READY;
	}



















	//XOR flips bit if on -> off, off -> on LED on if running, off if paused
	/*static void App_Handle_Btn_RunPause(app_state_t *state)
	{
	state->running ^= 1;
	HAL_GPIO_WritePin(LED_ACTIVITY_GPIO_Port, LED_ACTIVITY_Pin, state->running ? GPIO_PIN_SET : GPIO_PIN_RESET);

	printf(" BTN: %s\r\n", state->running ? "RUN" : "PAUSE");

	}

	static void App_Handle_Btn_Scale(app_state_t *state)
	{
		state->scale_mode = (state->scale_mode + 1) % 2;  //cycles through 0 -> 1 -> 0 just like x ^= 1
		printf("BTN: scale_mode=%u\r\n", state->scale_mode);
	}

	//span 0 is minimum 3 is the max, span feature not implemented yet
	static void App_Handle_Btn_SpanUp(app_state_t *state)
	{
		if ((state->span >= 0) && (state->span < 3)) state->span++;
		printf("BTN: span=%u\r\n", state->span);
	}

	static void App_Handle_Btn_SpanDn(app_state_t *state)
		{
			if (state->span > 0) state->span--;
			printf("BTN: span=%u\r\n", state->span);
		} */
  /* USER CODE END 3 */


/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_FLASH_SET_LATENCY(FLASH_LATENCY_1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV2;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.ScanConvMode = ADC_SCAN_SEQ_FIXED;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.LowPowerAutoPowerOff = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIG_T3_TRGO;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
  hadc1.Init.DMAContinuousRequests = ENABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc1.Init.SamplingTimeCommon1 = ADC_SAMPLETIME_39CYCLES_5;
  hadc1.Init.OversamplingMode = DISABLE;
  hadc1.Init.TriggerFrequencyMode = ADC_TRIGGER_FREQ_HIGH;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_7;
  sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 47;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 999;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 499;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.BreakFilter = 0;
  sBreakDeadTimeConfig.BreakAFMode = TIM_BREAK_AFMODE_INPUT;
  sBreakDeadTimeConfig.Break2State = TIM_BREAK2_DISABLE;
  sBreakDeadTimeConfig.Break2Polarity = TIM_BREAK2POLARITY_HIGH;
  sBreakDeadTimeConfig.Break2Filter = 0;
  sBreakDeadTimeConfig.Break2AFMode = TIM_BREAK_AFMODE_INPUT;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 5;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 999;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
  /* DMA1_Channel2_3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel2_3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel2_3_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, LCD_DC_Pin|LCD_RST_Pin|LCD_CS2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LCD_BL_Pin|LED_EXTRA_Pin|LED_ACTIVITY_Pin|LED_POWER_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : LCD_DC_Pin LCD_RST_Pin */
  GPIO_InitStruct.Pin = LCD_DC_Pin|LCD_RST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : LCD_BL_Pin */
  GPIO_InitStruct.Pin = LCD_BL_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(LCD_BL_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : BTN_RUN_PAUSE_Pin */
  GPIO_InitStruct.Pin = BTN_RUN_PAUSE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(BTN_RUN_PAUSE_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : BTN_SCALE_Pin BTN_SPAN_UP_Pin BTN_SPAN_DOWN_Pin */
  GPIO_InitStruct.Pin = BTN_SCALE_Pin|BTN_SPAN_UP_Pin|BTN_SPAN_DOWN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : LED_EXTRA_Pin LED_ACTIVITY_Pin LED_POWER_Pin */
  GPIO_InitStruct.Pin = LED_EXTRA_Pin|LED_ACTIVITY_Pin|LED_POWER_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : LCD_CS2_Pin */
  GPIO_InitStruct.Pin = LCD_CS2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LCD_CS2_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI0_1_IRQn);

  HAL_NVIC_SetPriority(EXTI2_3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI2_3_IRQn);

  HAL_NVIC_SetPriority(EXTI4_15_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI4_15_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

//DMA transfer complete callback,
//first ISR ever!!
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
	if (hadc->Instance == ADC1)
	{
		event_flags |= EVENT_ADC_READY; //were setting the EVENT_ADC_READY bit in the uint32_t event_flags
	}
}

//newlib nano printf calls fwrite -> _write -> __io_putchar
// retargeting io putchar into HAL_UART_Transmit   makes printf send to UART
int __io_putchar(int ch)
{
	HAL_UART_Transmit(&huart2, (uint8_t*)&ch, 1, HAL_MAX_DELAY);
	return ch;
}


//multiple EXTI lines share one IRQ vector, GPI_Pin is a bitmask that tells us which pin triggered
/*void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)
{

	uint32_t now = HAL_GetTick(); //now = current tick

	if (GPIO_Pin == BTN_RUN_PAUSE_Pin)
	{
		if ((now - btn_run_last_tick) >= BTN_DEBOUNCE_MS) //current_tick - last_tick < 50ms ignore otherwise real press
		{
			btn_run_last_tick = now;
			event_flags |= EVENT_BTN_RUN;
		}
	}
	else if (GPIO_Pin == BTN_SCALE_Pin)
	    {
	        if ((now - btn_scale_last_tick) >= BTN_DEBOUNCE_MS)
	        {
	            btn_scale_last_tick = now;
	            event_flags |= EVENT_BTN_SCALE;
	        }
	    }
	    else if (GPIO_Pin == BTN_SPAN_UP_Pin)
	    {
	        if ((now - btn_span_up_last_tick) >= BTN_DEBOUNCE_MS)
	        {
	            btn_span_up_last_tick = now;
	            event_flags |= EVENT_BTN_SPAN_UP;
	        }
	    }
	    else if (GPIO_Pin == BTN_SPAN_DOWN_Pin)
	    {
	        if ((now - btn_span_dn_last_tick) >= BTN_DEBOUNCE_MS)
	        {
	            btn_span_dn_last_tick = now;
	            event_flags |= EVENT_BTN_SPAN_DN;
	        }
	    }
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
	if (hspi->Instance == SPI1)
	{
		spi_dma_busy = 0;
		event_flags |= EVENT_SPI_IDLE;
	}
} */
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
