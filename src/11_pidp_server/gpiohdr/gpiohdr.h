/* gpiohdr: libgpiod helpers shared by the PiDP-11 panel server and scansw.
 *
 * Everything works on the GPIO chip behind the Raspberry Pi 40-pin header,
 * through /dev/gpiochipN. No /dev/mem or /dev/gpiomem access is needed.
 */
#ifndef GPIOHDR_H
#define GPIOHDR_H

#include <stddef.h>
#include <gpiod.h>

#define GPIOHDR_CONSUMER "pidp11"

/* Direction + pull pairs for the common cases (pass as the last two args). */
#define GPIOHDR_IN_PULLUP  GPIOD_LINE_DIRECTION_INPUT,  GPIOD_LINE_BIAS_PULL_UP
#define GPIOHDR_OUT        GPIOD_LINE_DIRECTION_OUTPUT, GPIOD_LINE_BIAS_AS_IS

/* Find the chip carrying the 40-pin header. Its number differs between Pi
 * models (gpiochip4 on a Pi 5, gpiochip0 on a Pi 4), so look for a
 * "pinctrl-*" chip with at least 28 lines. PIDP11_GPIOCHIP=/dev/gpiochipN
 * overrides the search. Returns NULL if there is no such chip (e.g. on a
 * host without a Pi header). */
struct gpiod_chip *gpiohdr_open_chip(void);

/* Request lines (BCM numbers = offsets on the header chip). Output lines
 * start low. Returns NULL on error with errno set. */
struct gpiod_line_request *gpiohdr_request(struct gpiod_chip *chip,
		const unsigned int *offsets, size_t n,
		enum gpiod_line_direction dir, enum gpiod_line_bias bias);

/* Change direction/pull of every line in a request. Output lines start low.
 * Returns -1 on error. */
int gpiohdr_reconfigure(struct gpiod_line_request *req,
		const unsigned int *offsets, size_t n,
		enum gpiod_line_direction dir, enum gpiod_line_bias bias);

#endif
