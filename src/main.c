/*
 * SPDX-License-Identifier: WTFPL
 * SPDX-FileCopyrightText: 2021 Anton Kikin <a.kikin@tano-systems.com>
 *
 * PWM tool
 * Copyright © 2021 Anton Kikin <a.kikin@tano-systems.com>
 *
 * This work is free. You can redistribute it and/or modify it under the
 * terms of the Do What The Fuck You Want To Public License, Version 2,
 * as published by Sam Hocevar. See the COPYING file for more details.
 */

/**
 * @file
 * @brief PWM tool main source file
 *
 * @author Anton Kikin <a.kikin@tano-systems.com>
 */

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#include "pwm.h"

/* ----------------------------------------------------------------------- */

#ifndef DEFAULT_PWM_CHIP

/** Default PWM chip number */
#define DEFAULT_PWM_CHIP  0
#endif

#ifndef DEFAULT_PWM_CHANNEL

/** Default PWM channel number */
#define DEFAULT_PWM_CHANNEL  0
#endif

#ifndef DEFAULT_PWM_FREQUENCY_HZ

/** Default frequency in Hz */
#define DEFAULT_PWM_FREQUENCY_HZ  1000
#endif

#ifndef DEFAULT_PWM_DURATION_MS

/** Default duration in milliseconds */
#define DEFAULT_PWM_DURATION_MS  250
#endif

/* ----------------------------------------------------------------------- */

/**
 * @brief Configuration data structure
 */
typedef struct config {

	/** PWM chip number.
	 *  Default value specified in @ref DEFAULT_PWM_CHIP. */
	unsigned int chip;

	/** PWM channel number.
	 *  Default value specified in @ref DEFAULT_PWM_CHANNEL. */
	unsigned int channel;

	/** PWM frequency in Hz
	 *  Default value specified in @ref DEFAULT_PWM_FREQUENCY_HZ. */
	unsigned int frequency_hz;

	/** PWM duration in ms
	 *  Default value specified in @ref DEFAULT_PWM_DURATION_MS. */
	unsigned int duration_ms;

	/** PWM duty cycle value (see pwm_enable_duty for format) */
	unsigned int duty_val;

	/** If set, PWM will remain enabled on exit. */
	int keep_enabled;

	char *script;

} config_t;

/* ----------------------------------------------------------------------- */

/** Global exit flag (used in script mode) */
static int exit_flag = 0;

/**
 * @brief Global configuration structure
 */
static config_t config = {
	.chip         = DEFAULT_PWM_CHIP,
	.channel      = DEFAULT_PWM_CHANNEL,
	.frequency_hz = DEFAULT_PWM_FREQUENCY_HZ,
	.duration_ms  = DEFAULT_PWM_DURATION_MS,
	.duty_val     = PWM_DUTY_DEFAULT,
	.keep_enabled = 0,
};

/**
 * @brief Short command line options list
 */
static const char *opts_str = "hp:c:f:d:D:s:k";

/**
 * @brief Long command line options list
 */
static const struct option opts[] = {
	{ .name = "help",         .val = 'h' },
	{ .name = "chip",         .val = 'p', .has_arg = 1 },
	{ .name = "channel",      .val = 'c', .has_arg = 1 },
	{ .name = "frequency",    .val = 'f', .has_arg = 1 },
	{ .name = "duration",     .val = 'd', .has_arg = 1 },
	{ .name = "duty",         .val = 'D', .has_arg = 1 },
	{ .name = "script",       .val = 's', .has_arg = 1 },
	{ .name = "keep-enabled", .val = 'k' },
	{ .name = "version",      .val = 'V' },
	{ 0 }
};

/**
 * Display program usage help
 */
static void display_usage(void)
{
	fprintf(stdout,
		"\n"
		"PWM tool " PWM_VERSION "\n"
		"Copyright (c) 2021 Anton Kikin <a.kikin@tano-systems.com>\n"
		"\n"
		"Usage: pwm [options]\n"
		"\n"
		"Options:\n"
		"  -h, --help\n"
		"        Display this help text.\n"
		"\n"
		"  -p, --chip <chip>\n"
		"        Select PWM chip number.\n"
		"        Default: %u\n"
		"\n"
		"  -c, --channel <channel>\n"
		"        Select PWM chip channel number.\n"
		"        Default: %u\n"
		"\n"
		"  -f, --frequency <frequency_in_hz>\n"
		"        Set PWM frequency in Hz.\n"
		"        Default: %u\n"
		"\n"
		"  -d, --duration <duration_in_ms>\n"
		"        Set PWM duration in milliseconds.\n"
		"        Default: %u\n"
		"\n"
		"  -D, --duty <value>\n"
		"        Set PWM duty cycle. Value can be:\n"
		"        - 1-255: raw value (duty = period * value / 255)\n"
		"        - 1-100 followed by '%%': percentage (e.g., 50%% = 50%%)\n"
		"        Default: 50%%\n"
		"\n"
		"  -k, --keep-enabled\n"
		"        If specified, PWM will remain enabled on exit.\n"
		"        Default: disable PWM on exit\n"
		"\n"
		"  -s, --script <script>\n"
		"        Run PWM commands script.\n"
		"\n"
		"  --version\n"
		"        Display PWM tool version.\n"
		"\n",
		DEFAULT_PWM_CHIP,
		DEFAULT_PWM_CHANNEL,
		DEFAULT_PWM_FREQUENCY_HZ,
		DEFAULT_PWM_DURATION_MS
	);
}

/**
 * Parse command line arguments into @ref config global structure
 *
 * @param[in] argc  Number of arguments
 * @param[in] argv  Array of the pointers to the arguments
 *
 * @return 0 on success
 * @return <0 on error
 */
static int parse_cli_args(int argc, char *argv[])
{
	int opt;

	while((opt = getopt_long(argc, argv, opts_str, opts, NULL)) != EOF) {
		switch(opt) {
			case '?': /* Invalid option */
				return -EINVAL;

			case 'h': /* --help */
				display_usage();
				exit(0);

			case 'p': /* --chip */
				config.chip =
					(unsigned int)strtoul(optarg, NULL, 0);
				break;

			case 'c': /* --channel */
				config.channel =
					(unsigned int)strtoul(optarg, NULL, 0);
				break;

			case 'f': /* --frequency */
				config.frequency_hz =
					(unsigned int)strtoul(optarg, NULL, 0);
				break;

			case 'd': /* --duration */
				config.duration_ms =
					(unsigned int)strtoul(optarg, NULL, 0);
				break;

			case 'D': /* --duty */ {
				size_t len = strlen(optarg);
				if (len > 0 && optarg[len - 1] == '%') {
					unsigned int percent =
						(unsigned int)strtoul(optarg, NULL, 10);
					config.duty_val = PWM_DUTY_PERCENT_FLAG | percent;
				}
				else {
					config.duty_val =
						(unsigned int)strtoul(optarg, NULL, 0);
				}
				break;
			}

			case 's': /* --script */
				config.script = strdup(optarg);
				if (!config.script) {
					fprintf(stderr, "ERROR: Out of memory");
					exit(-ENOMEM);
				}
				break;

			case 'k': /* --keep-enabled */
				config.keep_enabled = 1;
				break;

			case 'V': /* --version */
				fprintf(stdout, "%s\n", PWM_VERSION);
				exit(0);

			default:
				break;
		}
	}

	return 0;
}

/**
 * Signal handler
 */
static void handle_signal(int signal)
{
	exit_flag = 1;
}

/**
 * Cleanup
 */
void cleanup(void)
{
	if (config.script)
		free(config.script);
}

/**
 * Program start point
 *
 * @param[in] argc  Number of arguments
 * @param[in] argv  Array of the pointers to the arguments
 *
 * @return 0 on success
 * @return <0 on error
 */
int main(int argc, char *argv[])
{
	pwm_status_t ret = 0;
	pwm_t pwm;

	if (parse_cli_args(argc, argv)) {
		display_usage();
		return EINVAL;
	}

	atexit(cleanup);

	signal(SIGINT, handle_signal);

	ret = pwm_open(&pwm, config.chip, config.channel, PWM_FLAG_EXPORT);
	if (ret != PWM_E_OK) {
		fprintf(stderr,
			"ERROR: Can't open PWM channel %u of chip %u: %s\n",
			config.channel, config.chip, pwm_strstatus(ret));
		exit(ret);
	}

	pwm_execute_config_t pwm_execute_config = {
		.script               =  config.script,
		.default_frequency_hz =  config.frequency_hz,
		.default_duration_ms  =  config.duration_ms,
		.default_duty_val     =  config.duty_val,
		.stop_flag            = &exit_flag,
	};

	if (!config.script) {
		if (config.keep_enabled)
			pwm_execute_config.script = "fduk";
		else
			pwm_execute_config.script = "fdu";
	}

	ret = pwm_execute(&pwm, &pwm_execute_config);

	pwm_close(&pwm);

	exit(ret);
}
