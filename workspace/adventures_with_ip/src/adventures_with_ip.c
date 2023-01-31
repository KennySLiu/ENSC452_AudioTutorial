/*
 * adventures_with_ip.c
 *
 * Main source file. Contains main() and menu() functions.
 */
#include "adventures_with_ip.h"

//const int * KENNY_AUDIO_MEM_START = (int *) 0x00200000;
//const int * KENNY_AUDIO_MEM_END   = (int *) 0x3fffffff;
//const int * KENNY_AUDIO_MEM_END   = (int *) 0x3FFFFFFF;

const int AUDIO_SAMPLE_RATE = 48000;
const int AUDIO_CHANNELS = 2;
const int MAX_RECORD_SEC = 8;
const int KENNY_AUDIO_MAX_SAMPLES = AUDIO_SAMPLE_RATE * MAX_RECORD_SEC * AUDIO_CHANNELS;

const int SAMPLE_SLEEP_USEC = 21;
int PlaybackSpeedAdjustment = 0;


void kenny_UpdatePlaybackSpeedFromGpios()
{
	u32 btns;
	btns = XGpio_DiscreteRead(&Gpio, BUTTON_CHANNEL);

	if (btns == 1){
		PlaybackSpeedAdjustment = 0;
		xil_printf("Speed reset to default time\r\n");
	}
	else if (btns == 4){
		xil_printf("Playback speed decreased \r\n");
		PlaybackSpeedAdjustment++;
	}
	else if (btns == 2){
		PlaybackSpeedAdjustment = SAMPLE_SLEEP_USEC;
		xil_printf("Playback speed reset to half-time\r\n");
	}
	else if (btns == 8){
		if (PlaybackSpeedAdjustment <= -SAMPLE_SLEEP_USEC + 1){
			xil_printf("Playback speed cannot increase more \r\n");
		}
		else{
			--PlaybackSpeedAdjustment;
			xil_printf("Playback speed increased\r\n");
		}
	}
	else if (btns == 16){
		PlaybackSpeedAdjustment = -SAMPLE_SLEEP_USEC/2;
		xil_printf("Playback speed reset to double-time\r\n");
	}
	if (btns != 0){
		usleep(250000);
	}
}


int kenny_sinehelper_UpdateFrequencyFromGpios(double * target_freq)
{
	u32 btns;
	btns = XGpio_DiscreteRead(&Gpio, BUTTON_CHANNEL);
	const float DEFAULT_FREQUENCY = 220.0;
	const double MIN_FREQ = 40.0;
	const double MAX_FREQ = 10000.0;

	if (btns == 1){
		*target_freq = DEFAULT_FREQUENCY;
		xil_printf("Target Frequency reset to %f Hz\r\n", DEFAULT_FREQUENCY);
	}
	else if (btns == 4){
		if (*target_freq > MAX_FREQ){
			xil_printf("Target Frequency cannot go higher\r\n");
		}
		else{
			// twelfth root of 2, aka one semitone in 12TET
			*target_freq *= 1.05946309436;
			xil_printf("Increasing frequency by 1 semitone\r\n");
		}
	}
	else if (btns == 2){
		if (*target_freq > MAX_FREQ){
			xil_printf("Target Frequency cannot go higher\r\n");
		}
		else{
			// octave jump
			*target_freq *= 2;
			xil_printf("Increasing frequency by 1 octave\r\n");
		}
	}
	else if (btns == 8){
		if (*target_freq < MIN_FREQ){
			xil_printf("Target Frequency cannot go lower\r\n");
		}
		else{
			// inverse of the 12th root of 2
			*target_freq *= 0.94387431268;
			xil_printf("Decreasing frequency by 1 semitone\r\n");
		}
	}
	else if (btns == 16){
		if (*target_freq < MIN_FREQ){
			xil_printf("Target Frequency cannot go lower\r\n");
		}
		else{
			// twelfth root of 2, aka one semitone in 12TET
			*target_freq /= 2;
			xil_printf("Decreasing frequency by 1 octave\r\n");
		}
	}

	if (btns == 0) {
		return 0;
	} else {
		return 1;
	}
}

void kenny_PlaySineWave()
{
	const int GPIO_CTR_MAX = 10000;
	const int PEAK_AMPLITUDE = 16777216/4;
	int gpio_timeout_ctr = GPIO_CTR_MAX;
	int gpios_activated = 0;
	int sample_num = 0;
	double target_freq = 220;

	while (1)
	{
		if (!XUartPs_IsReceiveData(UART_BASEADDR)){

			if (gpio_timeout_ctr == GPIO_CTR_MAX){
				gpios_activated = kenny_sinehelper_UpdateFrequencyFromGpios(&target_freq);
				if (gpios_activated) {
					gpio_timeout_ctr = 0;
				}
			} else {
				gpios_activated = 0;
			}

			if (gpio_timeout_ctr != GPIO_CTR_MAX) {
				++gpio_timeout_ctr;
			}

			double sine_arg = sample_num++ * 6.28 * target_freq/AUDIO_SAMPLE_RATE;

			double out_value_float = sin(sine_arg)*PEAK_AMPLITUDE;
			s32 out_value = out_value_float;
			//xil_printf("%d\r\n", out_value);

			// Write audio data to audio codec
			Xil_Out32(I2S_DATA_TX_L_REG, out_value);
			Xil_Out32(I2S_DATA_TX_R_REG, out_value);
			usleep(SAMPLE_SLEEP_USEC);
		}
		else if (XUartPs_ReadReg(UART_BASEADDR, XUARTPS_FIFO_OFFSET) == 'q'){
			break;
		}
	}

}



void kenny_PlaybackAudioFromMem(const int* KENNY_AUDIO_MEM_PTR)
{
	u32  in_left, in_right;
	int * cur_ptr = KENNY_AUDIO_MEM_PTR;
	u32 num_samples_recorded = 0;

	while (1)
	{
		if (!XUartPs_IsReceiveData(UART_BASEADDR)){
			// Read audio data from memory
			in_left  = *(cur_ptr++);
			in_right = *(cur_ptr++);
			num_samples_recorded += 2;

			// Write audio data to audio codec
			Xil_Out32(I2S_DATA_TX_L_REG, in_left);
			Xil_Out32(I2S_DATA_TX_R_REG, in_right);

			usleep(SAMPLE_SLEEP_USEC + PlaybackSpeedAdjustment);

			if (num_samples_recorded >= KENNY_AUDIO_MAX_SAMPLES-1){
				break;
			}
		}
		else if (XUartPs_ReadReg(UART_BASEADDR, XUARTPS_FIFO_OFFSET) == 'q'){
			break;
		}
	}
}


void kenny_RecordAudioIntoMem(const int* KENNY_AUDIO_MEM_PTR)
{
	u32  in_left, in_right;
	int * cur_ptr = KENNY_AUDIO_MEM_PTR;
	u32 num_samples_recorded = 0;

	//memset(KENNY_AUDIO_MEM_START, 0, (KENNY_AUDIO_MEM_END - KENNY_AUDIO_MEM_START));

	/*
	for ( int* i = KENNY_AUDIO_MEM_START; i < KENNY_AUDIO_MEM_END; ++i)
	{
		*(i) = 0;
	}
	*/

	while (1){
		if (!XUartPs_IsReceiveData(UART_BASEADDR)){
			// Read audio input from codec
			in_left = Xil_In32(I2S_DATA_RX_L_REG);
			in_right = Xil_In32(I2S_DATA_RX_R_REG);
			// Save to memory
			*(cur_ptr++) = in_left;
			*(cur_ptr++) = in_right;
			num_samples_recorded += 2;

			usleep(SAMPLE_SLEEP_USEC);

			if (num_samples_recorded >= KENNY_AUDIO_MAX_SAMPLES-1){
				break;
			}
		}
		else if (XUartPs_ReadReg(UART_BASEADDR, XUARTPS_FIFO_OFFSET) == 'q'){
			break;
		}
	}
}

void kenny_GPIOTest(){
	while(1){
		u32 btns;
		u32 sws;
		btns = XGpio_DiscreteRead(&Gpio, BUTTON_CHANNEL);
		xil_printf("Buttons State: %d \r\n", btns);

		sws = XGpio_DiscreteRead(&Gpio, SWITCH_CHANNEL);
		xil_printf("Push Buttons State: %d \r\n", sws);
	}
}

/* ---------------------------------------------------------------------------- *
 * 									main()										*
 * ---------------------------------------------------------------------------- *
 * Runs all initial setup functions to initialise the audio codec and IP
 * peripherals, before calling the interactive menu system.
 * ---------------------------------------------------------------------------- */
int main(void)
{
	xil_printf("Entering Main\r\n");
	//Configure the IIC data structure
	IicConfig(XPAR_XIICPS_0_DEVICE_ID);


	//Configure the Audio Codec's PLL
	AudioPllConfig();

	//Configure the Line in and Line out ports.
	//Call LineInLineOutConfig() for a configuration that
	//enables the HP jack too.
	AudioConfigureJacks();

	xil_printf("ADAU1761 configured\n\r");

	/* Initialise GPIO and NCO peripherals */
	gpio_init();
	//nco_init(&Nco);

	xil_printf("GPIO and NCO peripheral configured\r\n");

	/* Display interactive menu interface via terminal */
	menu();

	//kenny_GPIOTest();

    return 1;
}



/* ---------------------------------------------------------------------------- *
 * 									menu()										*
 * ---------------------------------------------------------------------------- *
 * Presented at system startup. Allows the user to select between three
 * options by pressing certain keys on the keyboard:
 * 		's' - 	Audio loopback streaming
 * 		'n' - 	Tonal noise is generated by an NCO and added to the audio
 * 				being captured from the audio codec.
 * 		'f' - 	The audio + tonal noise is passed to an adaptive LMS noise
 * 				cancellation filter which will use the tonal noise estimate
 * 				to remove the noise from the audio.
 *
 * 	This menu is shown upon exiting from any of the above options.
 * ---------------------------------------------------------------------------- */
void printMenu(){
	xil_printf("\r\n\r\n");
	xil_printf("Enter 's' to stream pure audio. \r\n");
	xil_printf("Enter 'r' or 'p' to record/playback, respectively. \r\n");
	xil_printf("Enter 't' to play a sine wave. \r\n");
	xil_printf("Use the push-buttons to configure the speed of the playback. \r\n");
	xil_printf("----------------------------------------\r\n");
}

void menu(){
	int * KENNY_AUDIO_MEM_PTR = malloc(sizeof(int) * KENNY_AUDIO_MAX_SAMPLES);

	xil_printf("audio ptr = %x\r\n", (int)KENNY_AUDIO_MEM_PTR);

	u8 inp = 0x00;
	u32 CntrlRegister;

	/* Turn off all LEDs */
	Xil_Out32(LED_BASE, 0);

	CntrlRegister = XUartPs_ReadReg(UART_BASEADDR, XUARTPS_CR_OFFSET);

	XUartPs_WriteReg(UART_BASEADDR, XUARTPS_CR_OFFSET,
				  ((CntrlRegister & ~XUARTPS_CR_EN_DIS_MASK) |
				   XUARTPS_CR_TX_EN | XUARTPS_CR_RX_EN));

	printMenu();
	// Wait for input from UART via the terminal
	while (1){
		if (!XUartPs_IsReceiveData(UART_BASEADDR)){
			kenny_UpdatePlaybackSpeedFromGpios();
		}
		else
		{
			inp = XUartPs_ReadReg(UART_BASEADDR, XUARTPS_FIFO_OFFSET);
			// Select function based on UART input
			switch(inp){
				case 's':
					xil_printf("STREAMING AUDIO\r\n");
					xil_printf("Press 'q' to return to the main menu\r\n");
					audio_stream();
					break;
				case 'r':
					xil_printf("RECORDING AUDIO\r\n");
					xil_printf("Press 'q' to stop recording early and return to the main menu\r\n");
					kenny_RecordAudioIntoMem(KENNY_AUDIO_MEM_PTR);
					break;
				case 'p':
					xil_printf("PLAYING BACK RECORDED AUDIO\r\n");
					xil_printf("Press 'q' to stop playback early and return to the main menu\r\n");
					kenny_PlaybackAudioFromMem(KENNY_AUDIO_MEM_PTR);
					break;

				case 't':
					xil_printf("PLAYING SINE WAVE\r\n");
					xil_printf("Press 'q' to return to main menu\r\n");
					kenny_PlaySineWave();
				default:
					break;
			} // switch
			printMenu();
		}
	}

} // menu()


