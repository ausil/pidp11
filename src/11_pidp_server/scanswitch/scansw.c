/*
 * Scan switches for PiDP-11 front panel
 *
 * www.obsolescenceguaranteed.blogspot.com
 *
 * v20231218; libgpiod version 20261103
 *
 * Prints the state of the SR switches (rows 0 and 1) as
 * (row1 << 12) | row0, with a set bit meaning "switch on".
 * Errors go to stderr so stdout is always just the number.
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "../gpiohdr/gpiohdr.h"

#define N_ROWS_SCANNED 2	// row 2 carries the rotary encoders, not switches
#define N_COLS 12

static const unsigned int rows[3] = {16, 17, 18};
static const unsigned int cols[N_COLS] = {26,27,4, 5,6,7, 8,9,10, 11,12,13};

long intervl = 1000000;		// pause

int main(void)
{
	struct gpiod_chip *chip;
	struct gpiod_line_request *col_req, *row_req[N_ROWS_SCANNED];
	enum gpiod_line_value colvals[N_COLS];
	int switchscan[N_ROWS_SCANNED] = {0, 0};
	int row, j;

	chip = gpiohdr_open_chip();
	if (!chip) {
		fprintf(stderr, "No GPIO chips found\n");
		return -1;
	}

	// columns and rows idle as inputs with pull-ups
	col_req = gpiohdr_request(chip, cols, N_COLS, GPIOHDR_IN_PULLUP);
	for (row = 0; row < N_ROWS_SCANNED; row++)
		row_req[row] = gpiohdr_request(chip, &rows[row], 1, GPIOHDR_IN_PULLUP);
	if (!col_req || !row_req[0] || !row_req[1]) {
		fprintf(stderr, "Failed to request GPIO lines - %s\n", strerror(errno));
		return -1;
	}

	for (row = 0; row < N_ROWS_SCANNED; row++) {
		// drive one row low; a pressed switch pulls its column low
		if (gpiohdr_reconfigure(row_req[row], &rows[row], 1, GPIOHDR_OUT) < 0)
			goto fail;

		nanosleep((struct timespec[]){{0, intervl/100}}, NULL); // probably unnecessary long wait

		if (gpiod_line_request_get_values(col_req, colvals) < 0)
			goto fail;
		for (j = 0; j < N_COLS; j++)	// 12 switches in each row
			if (colvals[j] == GPIOD_LINE_VALUE_INACTIVE)
				switchscan[row] += 1 << j;

		if (gpiohdr_reconfigure(row_req[row], &rows[row], 1, GPIOHDR_IN_PULLUP) < 0)
			goto fail;
	}

	printf("%d\n", switchscan[1] << 12 | switchscan[0]);
	return 0;

fail:
	fprintf(stderr, "GPIO error - %s\n", strerror(errno));
	return -1;
}
