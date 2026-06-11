#include "dsp.h"
#include "main.h"
#include "app_state.h"
#include <stdio.h>   /* printf                              */
#include <stdint.h>
#include "events.h"

#define NOISE_FLOOR_POW  1.9e8f

/* External — adc_buf lives in main.c, DMA writes into it */
extern uint16_t adc_buf[ADC_BUF_LEN];

/* --- Shared output buffer --------------------------------------------- */
float magnitude[FFT_BINS];   /* extern declared in dsp.h */

/* --- Private working buffers ------------------------------------------ */
static float fft_buf[ADC_BUF_LEN];
static float im_buf[ADC_BUF_LEN];



//we use float to multiply samples by these weights and feed results into a float FF, pipeline in float avoids rounding errors from integer scaling
//ram cost 256 x 4 bytes -> 1024
//lives in ram != not const, cosf() is runtime function C cant evaluate at compile time
//compute once in hanning_init()

//we are not computing dat shit too expensive precomputed TABLE HAAHAHAHAHAA
static const float hanning[ADC_BUF_LEN] = {
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


void FFT256(float *re, float *im)
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
				/* W*V = (wr + j*wi)(vr + j*vi) = wr*vr - wi*vi + j(wr*vi + wi*vr) */
				float vr = re[i + j + len/2] * wr - im[i + j + len/2] * wi;
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


extern volatile uint32_t event_flags;


 void App_Handle_DSP(app_state_t *state)
	{


		  state->frame_count++; //iterate how many buffers we got


		  for (int i = 0; i < FFT_BINS; i++) magnitude[i] = 0.0f; //zero magnitude each frame, prevent stale value from last frame

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
				  if (state->frame_count <= 5 || (state->frame_count % 30) ==0)
				  {
				  printf("[%4lu] min=%4u  max=%4u  mean=%4u  amp=%4u\r\n",state->frame_count, min_val, max_val, mean_val, amplitude);
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

				  		 float peak_pow = 0.0f;

				  //we skip bin 0, (thats the DC) and only store bins 1-127
				  for (int i = 1; i < ADC_BUF_LEN/2; i++)
				  		 {
				  		 float power = fft_buf[i]*fft_buf[i] + im_buf[i]*im_buf[i];
				  		 magnitude[i] = power;
				  			 if (power > peak_pow)
				  			 {
				  				 peak_pow = power;   //getting peak power and peak bin

				  			}
				  		 }
				  	  //store peak power in state so display handler can scale from it
				  	  //we add to app_state
				  	  state->peak_pow = (peak_pow > NOISE_FLOOR_POW) ? peak_pow : NOISE_FLOOR_POW;

				  	  event_flags |= EVENT_DSP_READY;
	}
