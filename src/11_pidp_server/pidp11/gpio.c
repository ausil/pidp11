/* gpio.c: the real-time process that handles multiplexing

 Copyright (c) 2015-2016, Oscar Vermeulen & Joerg Hoppe
 j_hoppe@t-online.de, www.retrocmp.com

 Permission is hereby granted, free of charge, to any person obtaining a
 copy of this software and associated documentation files (the "Software"),
 to deal in the Software without restriction, including without limitation
 the rights to use, copy, modify, merge, publish, distribute, sublicense,
 and/or sell copies of the Software, and to permit persons to whom the
 Software is furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 JOERG HOPPE BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 18-Dec-2023  OV    new GPIO interface using the code of pinctrl, needed for Pi 5
 14-Aug-2019  OV    fix for Raspberry Pi 4's different pullup configuration
 01-Jul-2017  MH    remove INP_GPIO before OUT_GPIO and change knobValue
 01-Apr-2016  OV    almost perfect before VCF SE
 15-Mar-2016  JH    display patterns for brightness levels
 16-Nov-2015  JH    acquired from Oscar


 gpio.c from Oscar Vermeules PiDP8-sources.
 Slightest possible modification by Joerg.
 See www.obsolescenceguaranteed.blogspot.com

 The only communication with the main program (simh):
 external variable ledstatus is read to determine which leds to light.
 external variable switchstatus is updated with current switch settings.

 */

#define _GPIO_C_

#include <time.h>
#include <pthread.h>
#include <stdint.h>
//#include "gpio.h"
#include "gpiopattern.h"


#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include "../gpiohdr/gpiohdr.h"

typedef unsigned int uint32;
typedef signed int int32;
typedef unsigned short uint16;

void short_wait(void); // used as pause between clocked GPIO changes
extern int knobValue[2];	// value for knobs. 0=ADDR, 1=DATA. see main.c.
void check_rotary_encoders(int switchscan);

// long intervl = 300000; // light each row of leds this long
long intervl = 50000; // light each row of leds 50 us
// almost flickerfree at 32 phases

// PART 1 - GPIO and RT process stuff ----------------------------------


// PART 2 - the multiplexing logic driving the front panel -------------

/* GPIO numbers on the 40-pin header (BCM numbering = line offset on the
 * header GPIO chip). */
#define N_LEDROWS 6
#define N_ROWS    3
#define N_COLS    12
static const unsigned int ledrows[N_LEDROWS] = {20, 21, 22, 23, 24, 25};
static const unsigned int rows[N_ROWS]       = {16, 17, 18};
static const unsigned int cols[N_COLS]       = {26,27,4, 5,6,7, 8,9,10, 11,12,13};

void *blink(int *terminate)
{
	struct gpiod_chip *chip;
	struct gpiod_line_request *ledrow_req, *col_req, *row_req[N_ROWS];
	enum gpiod_line_value colvals[N_COLS];
	int i, j, k, switchscan;
	int failed = 0;

	chip = gpiohdr_open_chip();
	if (!chip) {
		printf("No GPIO chips found\n");
		return (void *)-1;
	}

	// LED rows: outputs, off. Columns: outputs, low. Switch rows: inputs with pull-up.
	ledrow_req = gpiohdr_request(chip, ledrows, N_LEDROWS, GPIOHDR_OUT);
	col_req = gpiohdr_request(chip, cols, N_COLS, GPIOHDR_OUT);
	for (i = 0; i < N_ROWS; i++)
		row_req[i] = gpiohdr_request(chip, &rows[i], 1, GPIOHDR_IN_PULLUP);

	if (!ledrow_req || !col_req || !row_req[0] || !row_req[1] || !row_req[2]) {
		printf("Failed to request GPIO lines - %s\n", strerror(errno));
		failed = 1;
		goto out;
	}

	// set thread to real time priority -----------------
	struct sched_param sp;
	sp.sched_priority = 98; // maybe 99, 32, 31?
	if (pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp))
	{
		fprintf(stderr, "warning: failed to set RT priority\n");
	}

	printf("\nPiDP-11 FP 2023\n");

	while (*terminate == 0 && !failed) {
		unsigned phase;

		// display all phases circular
		for (phase = 0; phase < GPIOPATTERN_LED_BRIGHTNESS_PHASES && !failed; phase++) {
			// each phase must be eact same duration, so include switch scanning here

			// the original gpio_ledstatus[8] runs trough all phases
			volatile uint32_t *gpio_ledstatus =
					gpiopattern_ledstatus_phases[gpiopattern_ledstatus_phases_readidx][phase];

			// prepare for lighting LEDs by setting col pins to output
			if (gpiohdr_reconfigure(col_req, cols, N_COLS, GPIOHDR_OUT) < 0) {
				failed = 1;
				break;
			}

			// light up 6 rows of 12 LEDs each
			for (i = 0; i < N_LEDROWS; i++) {

				// Set the columns for this ledrow (which LEDs should be on (CLR = on))
				for (k = 0; k < N_COLS; k++)
					colvals[k] = (gpio_ledstatus[i] & (1 << k)) == 0
							? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE;
				if (gpiod_line_request_set_values(col_req, colvals) < 0)
					failed = 1;

				// Toggle this ledrow on
				if (gpiod_line_request_set_value(ledrow_req, ledrows[i],
						GPIOD_LINE_VALUE_ACTIVE) < 0)
					failed = 1;

				nanosleep((struct timespec[]
				)	{	{	0, intervl}}, NULL);

				// Toggle ledrow off
				if (gpiod_line_request_set_value(ledrow_req, ledrows[i],
						GPIOD_LINE_VALUE_INACTIVE) < 0)
					failed = 1;

				usleep(10); // waste of cpu cycles but may help against udn2981 ghosting, not flashes though
			}

			// prepare for reading switches: flip columns to input, pull-ups on
			if (gpiohdr_reconfigure(col_req, cols, N_COLS, GPIOHDR_IN_PULLUP) < 0) {
				failed = 1;
				break;
			}

			// read three rows of switches
			for (i = 0; i < N_ROWS; i++)
			{
				// on one row pin,
				// output 0V to overrule the built-in pull-up from column input pin
				// to read this row of switches
				if (gpiohdr_reconfigure(row_req[i], &rows[i], 1, GPIOHDR_OUT) < 0) {
					failed = 1;
					break;
				}

				nanosleep((struct timespec[]) { { 0, intervl / 100}}, NULL);
				// probably unnecessary long wait

				if (gpiod_line_request_get_values(col_req, colvals) < 0) {
					failed = 1;
					break;
				}
				switchscan = 0;
				for (j = 0; j < N_COLS; j++) // 12 switches in each row
					if (colvals[j] == GPIOD_LINE_VALUE_ACTIVE)
						switchscan += 1 << j;

				// stop sinking current from this row of switches
				if (gpiohdr_reconfigure(row_req[i], &rows[i], 1, GPIOHDR_IN_PULLUP) < 0) {
					failed = 1;
					break;
				}

				if (i==2)
					check_rotary_encoders(switchscan);	// translate raw encoder data to switch position

				gpio_switchstatus[i] = switchscan;

			}
			// a full scan of all switch rows is now in gpio_switchstatus[]
			if (!failed)
				gpio_hw_ready = 1;
		}
	}
	if (failed) {
		gpio_hw_ready = 0;
		printf("GPIO error - %s\n", strerror(errno));
	}

out:
	// Releasing the requests hands the lines back to the kernel as inputs.
	if (ledrow_req)
		gpiod_line_request_release(ledrow_req);
	if (col_req)
		gpiod_line_request_release(col_req);
	for (i = 0; i < N_ROWS; i++)
		if (row_req[i])
			gpiod_line_request_release(row_req[i]);
	gpiod_chip_close(chip);

	return failed ? (void *)-1 : 0;
}

void short_wait(void) // creates pause required in between clocked GPIO settings changes
{
	fflush(stdout); //
	usleep(1); // suggested as alternative for asm which c99 does not accept
}



void check_rotary_encoders(int switchscan)
{
	// 2 rotary encoders. Each has two switch pins. Normally, both are 0 - no rotation.
	// encoder 1: row1, bits 8,9. Encoder 2: row1, bits 10,11
	// Gray encoding: rotate up sequence   = 11 -> 01 -> 00 -> 10 -> 11
	// Gray encoding: rotate down sequence = 11 -> 10 -> 00 -> 01 -> 11

	static int lastCode[2] = {3,3};
	int code[2];
	int i;

	code[0] = (switchscan & 0x300) >> 8;
	code[1] = (switchscan & 0xC00) >> 10;
	switchscan = switchscan & 0xff;	// set the 4 bits to zero

//printf("code 0 = %d, code 1 = %d\n", code[0], code[1]);

	// detect rotation
	for (i=0;i<2;i++)
	{
		if ((code[i]==1) && (lastCode[i]==3))
			lastCode[i]=code[i];
		else if ((code[i]==2) && (lastCode[i]==3))
			lastCode[i]=code[i];
	}

	// detect end of rotation
	for (i=0;i<2;i++)
	{
		if ((code[i]==3) && (lastCode[i]==1))
		{
			lastCode[i]=code[i];
			switchscan = switchscan + (1<<((i*2)+8));
//			printf("%d end of UP %d %d\n",i, switchscan, (1<<((i*2)+8)));
			knobValue[i]++;	//bugfix 20181225

		}
		else if ((code[i]==3) && (lastCode[i]==2))
		{
			lastCode[i]=code[i];
			switchscan = switchscan + (2<<((i*2)+8));
//			printf("%d end of DOWN %d %d\n",i,switchscan, (2<<((i*2)+8)));
			knobValue[i]--;	// bugfix 20181225
		}
	}


//	if (knobValue[0]>7)
//		knobValue[0] = 0;
//	if (knobValue[1]>3)
//		knobValue[1] = 0;
//	if (knobValue[0]<0)
//		knobValue[0] = 7;
//	if (knobValue[1]<0)
//		knobValue[1] = 3;

	knobValue[0] = knobValue[0] & 7;
	knobValue[1] = knobValue[1] & 3;

	// end result: bits 8,9 are 00 normally, 01 if UP, 10 if DOWN. Same for bits 10,11 (knob 2)
	// these bits are not used, actually. Status is communicated through global variable knobValue[i]

}
