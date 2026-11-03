#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gpiohdr.h"

struct gpiod_chip *gpiohdr_open_chip(void)
{
	const char *forced = getenv("PIDP11_GPIOCHIP");
	char path[32];
	int n;

	if (forced)
		return gpiod_chip_open(forced);

	for (n = 0; n < 64; n++) {
		struct gpiod_chip *chip;
		struct gpiod_chip_info *info;
		int match = 0;

		snprintf(path, sizeof path, "/dev/gpiochip%d", n);
		chip = gpiod_chip_open(path);
		if (!chip)
			continue;
		info = gpiod_chip_get_info(chip);
		if (info) {
			const char *label = gpiod_chip_info_get_label(info);
			match = label && strncmp(label, "pinctrl-", 8) == 0
				&& gpiod_chip_info_get_num_lines(info) >= 28;
			gpiod_chip_info_free(info);
		}
		if (match)
			return chip;
		gpiod_chip_close(chip);
	}
	return NULL;
}

static struct gpiod_line_config *make_config(const unsigned int *offsets, size_t n,
		enum gpiod_line_direction dir, enum gpiod_line_bias bias)
{
	struct gpiod_line_settings *ls = gpiod_line_settings_new();
	struct gpiod_line_config *lc = gpiod_line_config_new();

	if (!ls || !lc)
		goto fail;
	gpiod_line_settings_set_direction(ls, dir);
	gpiod_line_settings_set_bias(ls, bias);
	if (dir == GPIOD_LINE_DIRECTION_OUTPUT)
		gpiod_line_settings_set_output_value(ls, GPIOD_LINE_VALUE_INACTIVE);
	if (gpiod_line_config_add_line_settings(lc, offsets, n, ls) < 0)
		goto fail;
	gpiod_line_settings_free(ls);
	return lc;
fail:
	gpiod_line_settings_free(ls);
	gpiod_line_config_free(lc);
	return NULL;
}

struct gpiod_line_request *gpiohdr_request(struct gpiod_chip *chip,
		const unsigned int *offsets, size_t n,
		enum gpiod_line_direction dir, enum gpiod_line_bias bias)
{
	struct gpiod_line_config *lc = make_config(offsets, n, dir, bias);
	struct gpiod_request_config *rc = gpiod_request_config_new();
	struct gpiod_line_request *req = NULL;

	if (lc && rc) {
		gpiod_request_config_set_consumer(rc, GPIOHDR_CONSUMER);
		req = gpiod_chip_request_lines(chip, rc, lc);
	}
	gpiod_request_config_free(rc);
	gpiod_line_config_free(lc);
	return req;
}

int gpiohdr_reconfigure(struct gpiod_line_request *req,
		const unsigned int *offsets, size_t n,
		enum gpiod_line_direction dir, enum gpiod_line_bias bias)
{
	struct gpiod_line_config *lc = make_config(offsets, n, dir, bias);
	int ret;

	if (!lc)
		return -1;
	ret = gpiod_line_request_reconfigure_lines(req, lc);
	gpiod_line_config_free(lc);
	return ret;
}
