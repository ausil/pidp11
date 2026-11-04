/*
 * paneltest: exercise the PiDP-11 front panel without SIMH or the server.
 *
 * Steps a single lit LED through all 72 LEDs (6 rows x 12 columns) while
 * continuously scanning the 3 x 12 switch matrix. Whenever a switch changes
 * state it is reported, along with the current state of all three rows.
 *
 *   paneltest [-s ms] [-a] [-n] [-h]
 *     -s ms   milliseconds per LED step (default 200)
 *     -a      light all LEDs instead of chasing one
 *     -n      no LEDs, only watch the switches
 *
 * Stop the server first (systemctl stop pidp11-server): only one program can
 * hold the GPIO lines. Ctrl-C releases them again.
 *
 * Switch row 0 and 1 hold the SR switches and the ADDR/DATA select etc.;
 * row 2 bits 8-11 are the two rotary encoders, whose gray-code states are
 * shown raw. A set bit in the output means "switch closed", as in scansw.
 */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "../gpiohdr/gpiohdr.h"

#define N_LEDROWS 6
#define N_ROWS    3
#define N_COLS    12

static const unsigned int ledrows[N_LEDROWS] = {20, 21, 22, 23, 24, 25};
static const unsigned int rows[N_ROWS]       = {16, 17, 18};
static const unsigned int cols[N_COLS]       = {26,27,4, 5,6,7, 8,9,10, 11,12,13};

static volatile sig_atomic_t stop;
static void on_signal(int sig) { (void)sig; stop = 1; }

static void pause_ns(long ns)
{
	nanosleep((struct timespec[]){{0, ns}}, NULL);
}

static double now_s(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec + t.tv_nsec / 1e9;
}

/* Scan the switch matrix; cols must already be inputs with pull-ups. */
static int scan_switches(struct gpiod_line_request *col_req,
		struct gpiod_line_request *row_req[N_ROWS], unsigned int scan[N_ROWS])
{
	enum gpiod_line_value v[N_COLS];
	int i, j;

	for (i = 0; i < N_ROWS; i++) {
		if (gpiohdr_reconfigure(row_req[i], &rows[i], 1, GPIOHDR_OUT) < 0)
			return -1;
		pause_ns(20000);	// let the column settle
		if (gpiod_line_request_get_values(col_req, v) < 0)
			return -1;
		scan[i] = 0;
		for (j = 0; j < N_COLS; j++)
			if (v[j] == GPIOD_LINE_VALUE_INACTIVE)	// pulled low = closed
				scan[i] |= 1u << j;
		if (gpiohdr_reconfigure(row_req[i], &rows[i], 1, GPIOHDR_IN_PULLUP) < 0)
			return -1;
	}
	return 0;
}

static void print_bits(unsigned int v)
{
	int j;
	for (j = N_COLS - 1; j >= 0; j--)
		putchar(v & (1u << j) ? '1' : '.');
}

static void usage(const char *prog)
{
	fprintf(stderr, "usage: %s [-s ms] [-a] [-n]\n", prog);
}

int main(int argc, char **argv)
{
	struct gpiod_chip *chip;
	struct gpiod_line_request *ledrow_req, *col_req, *row_req[N_ROWS];
	enum gpiod_line_value colvals[N_COLS];
	unsigned int scan[N_ROWS], last[N_ROWS] = {0, 0, 0};
	int step_ms = 200, all = 0, leds = 1, opt, i, j, rc = 1;
	int led = 0, have_last = 0;
	double next_step;

	while ((opt = getopt(argc, argv, "s:anh")) != -1) {
		switch (opt) {
		case 's': step_ms = atoi(optarg); if (step_ms < 1) step_ms = 1; break;
		case 'a': all = 1; break;
		case 'n': leds = 0; break;
		default: usage(argv[0]); return opt == 'h' ? 0 : 2;
		}
	}

	chip = gpiohdr_open_chip();
	if (!chip) {
		fprintf(stderr, "No Raspberry Pi GPIO header chip found\n");
		return 1;
	}

	ledrow_req = gpiohdr_request(chip, ledrows, N_LEDROWS, GPIOHDR_OUT);
	col_req = gpiohdr_request(chip, cols, N_COLS, GPIOHDR_IN_PULLUP);
	for (i = 0; i < N_ROWS; i++)
		row_req[i] = gpiohdr_request(chip, &rows[i], 1, GPIOHDR_IN_PULLUP);
	if (!ledrow_req || !col_req || !row_req[0] || !row_req[1] || !row_req[2]) {
		fprintf(stderr, "Failed to request GPIO lines - %s\n"
		        "(is pidp11-server still running?)\n", strerror(errno));
		goto out;
	}

	signal(SIGINT, on_signal);
	signal(SIGTERM, on_signal);

	printf("PiDP-11 panel test. Ctrl-C to quit.\n");
	if (leds)
		printf("%s\n", all ? "All LEDs on." : "Chasing one LED through all 72.");
	printf("Switches: '1' = closed. Rows are printed as bits 11..0.\n");

	next_step = now_s();
	while (!stop) {
		int lr = led / N_COLS, lc = led % N_COLS;

		// 1. LEDs: columns as outputs, drive them for a few ms
		if (leds) {
			if (gpiohdr_reconfigure(col_req, cols, N_COLS, GPIOHDR_OUT) < 0)
				goto fail;
			if (all) {
				// time-multiplex the six rows
				for (i = 0; i < N_LEDROWS; i++) {
					for (j = 0; j < N_COLS; j++)
						colvals[j] = GPIOD_LINE_VALUE_ACTIVE;
					if (gpiod_line_request_set_values(col_req, colvals) < 0 ||
					    gpiod_line_request_set_value(ledrow_req, ledrows[i],
							GPIOD_LINE_VALUE_ACTIVE) < 0)
						goto fail;
					pause_ns(500000);
					if (gpiod_line_request_set_value(ledrow_req, ledrows[i],
							GPIOD_LINE_VALUE_INACTIVE) < 0)
						goto fail;
					usleep(10);
				}
			} else {
				for (j = 0; j < N_COLS; j++)
					colvals[j] = j == lc ? GPIOD_LINE_VALUE_ACTIVE
					                     : GPIOD_LINE_VALUE_INACTIVE;
				if (gpiod_line_request_set_values(col_req, colvals) < 0 ||
				    gpiod_line_request_set_value(ledrow_req, ledrows[lr],
						GPIOD_LINE_VALUE_ACTIVE) < 0)
					goto fail;
				pause_ns(3000000);
				if (gpiod_line_request_set_value(ledrow_req, ledrows[lr],
						GPIOD_LINE_VALUE_INACTIVE) < 0)
					goto fail;
				usleep(10);
			}
			if (gpiohdr_reconfigure(col_req, cols, N_COLS, GPIOHDR_IN_PULLUP) < 0)
				goto fail;
		} else {
			usleep(3000);
		}

		// 2. switches
		if (scan_switches(col_req, row_req, scan) < 0)
			goto fail;
		if (!have_last) {
			memcpy(last, scan, sizeof last);
			have_last = 1;
			printf("initial   row0 "); print_bits(scan[0]);
			printf("  row1 "); print_bits(scan[1]);
			printf("  row2 "); print_bits(scan[2]); putchar('\n');
		}
		for (i = 0; i < N_ROWS; i++) {
			unsigned int diff = scan[i] ^ last[i];
			for (j = 0; j < N_COLS; j++)
				if (diff & (1u << j))
					printf("row %d col %2d (bit %2d) %s   row0 ", i, j, j,
					       scan[i] & (1u << j) ? "closed" : "open  "),
					print_bits(scan[0]), printf("  row1 "),
					print_bits(scan[1]), printf("  row2 "),
					print_bits(scan[2]), putchar('\n');
			last[i] = scan[i];
		}
		fflush(stdout);

		// 3. advance the chase
		if (leds && !all && now_s() >= next_step) {
			led = (led + 1) % (N_LEDROWS * N_COLS);
			next_step += step_ms / 1000.0;
		}
	}
	rc = 0;
	goto out;

fail:
	fprintf(stderr, "GPIO error - %s\n", strerror(errno));
out:
	// releasing the requests hands the lines back as inputs: LEDs go dark
	if (ledrow_req) gpiod_line_request_release(ledrow_req);
	if (col_req) gpiod_line_request_release(col_req);
	for (i = 0; i < N_ROWS; i++)
		if (row_req[i]) gpiod_line_request_release(row_req[i]);
	gpiod_chip_close(chip);
	return rc;
}
