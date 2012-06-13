/*
 * Copyright (C) 2012 Hubert Kario <kario@wsisiz.edu.pl>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 *
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <inttypes.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <getopt.h>
#include <signal.h>
#include "activity_stats.h"

static int programEnd = 0;

struct trace_point {
	int8_t dev_major;
	int8_t dev_minor;
	int16_t cpu_id;
	int64_t sequence_no;
	int64_t nanoseconds;
	int32_t process_id;
	char action[20];
	char rwbs_data[20];
	int64_t block;
	int64_t len;
};

#define BASE_10 10

int
parse_trace_line(char *line, struct trace_point *ret) {

	assert(ret);

	int n;
	int64_t nl;
	char *nptr = line;
	char *endptr;

	memset(ret, 0, sizeof(struct trace_point));

	/*
	 * block device number
	 */
	errno = 0;
	n = strtol(nptr, &endptr, BASE_10);
	// an obviously non blktrace line, or blktrace summary lines
	if (endptr == nptr || errno || *endptr != ',')
		return 1;

	ret->dev_major = n;

	nptr = endptr + 1;

	n = strtol(nptr, &endptr, BASE_10);
	assert(endptr != nptr);
	assert(!errno);

	ret->dev_minor = n;

	nptr = endptr + 1;

	/*
	 * cpu number
	 */
	errno = 0;
	n = strtol(nptr, &endptr, BASE_10);
	assert(nptr != endptr);
	assert(!errno);

	ret->cpu_id = n;

	nptr = endptr + 1;

	/*
	 * sequence number
	 */
	errno = 0;
	nl = strtoll(nptr, &endptr, BASE_10);
	assert(nptr != endptr);
	assert(!errno);

	ret->sequence_no = nl;

	nptr = endptr + 1;

	/*
	 * time
	 */
	errno = 0;
	nl = strtoll(nptr, &endptr, BASE_10);
	assert(nptr != endptr);
	assert(*endptr == '.');
	assert(!errno);

	ret->nanoseconds = nl * 1000000000L; /* ns is s */

	nptr = endptr + 1;

	nl = strtoll(nptr, &endptr, BASE_10);
	assert(nptr != endptr);
	assert(!errno);

	ret->nanoseconds += nl;

	nptr = endptr + 1;

	/*
	 * process ID
	 */
	errno = 0;
	n = strtol(nptr, &endptr, BASE_10);
	assert(nptr != endptr);
	assert(!errno);

	ret->process_id = n;

	nptr = endptr + 1;

	/*
	 * action
	 */
	while(isspace(*nptr)) {
		nptr++;
	}
	n = 0;
	while(!isspace(*nptr)) {
		ret->action[n] = *nptr;
		n++;
		nptr++;
		assert(n < sizeof(ret->rwbs_data));
	}
	ret->action[n] = '\0';

	/*
	 * rwbs data
	 */
	while(isspace(*nptr)) {
		nptr++;
	}
	n = 0;
	while(!isspace(*nptr)) {
		ret->rwbs_data[n] = *nptr;
		n++;
		nptr++;
		assert(n < sizeof(ret->rwbs_data));
	}
	ret->rwbs_data[n] = '\0';

	/*
	 * block number
	 */
	while(isspace(*nptr)){
		nptr++;
	} // special treatment for sync() requests
	if (*nptr == '[')
		return 0;

	errno = 0;
	nl = strtoll(nptr, &endptr, BASE_10);
	assert(nptr != endptr);
	assert(!errno);
	assert(*endptr == ' ');

	ret->block = nl;

	nptr = endptr + 1;

	if (*nptr == '[') // sync request completions don't have len
		return 0;
	assert(*nptr == '+');

	nptr++;
	nl = strtoll(nptr, &endptr, BASE_10);
	assert(nptr != endptr);
	assert(!errno);

	ret->len = nl;

	// only process name left, which we ignore

	return 0;
}

int64_t
div_ceil(int64_t num, int64_t denum) {
	return (num - 1)/denum + 1;
}

/**
 * Converts blktrace blocks to PV extents
 *
 * @param[in,out] offset block where the IO started
 * @param[in,out] len number of blocks in IO
 * @param[out] extent converted block to extent number
 * @param ssize sector size (in bytes)
 * @param esize extent size (in bytes)
 * @return 0 if whole IO fits into extent, 1 if function needs to be run again
 */
int
trace_blocks_to_extents(int64_t *offset, int64_t *len, int64_t *extent, size_t ssize, size_t esize) {
	assert(offset);
	assert(*offset >= 0);
	assert(len);
	assert(*len > 0);
	assert(extent);
	assert(ssize > 0);
	assert(esize > 0);

	int64_t s_in_e = div_ceil(esize, ssize);

	*extent = *offset / s_in_e;

	if ((*offset) / s_in_e == (*offset + *len - 1) / s_in_e) {
		return 0;
	}

	*len -= (*extent + 1) * s_in_e - *offset - 1;
	*offset += (*extent + 1) * s_in_e - *offset;
	return 1;
}

int
collect_trace_points(char *device,
		     struct activity_stats *activity,
		     int64_t granularity,
		     size_t esize,
		     int *ender) {
#define TRACE_APP "btrace"
	FILE *trace;
	char *command;
	int ret = 0;
	int n;
	size_t line_len = 4096;
	char *line = malloc(line_len);
	assert(line);
	struct trace_point *tp = malloc(sizeof(struct trace_point));
	assert(tp);
	size_t ssize = 512; // sector size: 0.5KiB
/** nanoseconds in second */
#define NS_IN_S 1000000000L
	int64_t tim;
	int64_t trace_start = time(NULL);
	int64_t extent;

	n = asprintf(&command, TRACE_APP " %s", device);
	if (n <= 0)
		return 1;

	trace = popen(command, "re");
	if (trace == NULL)
		return 1;

	while(!*ender && getline(&line, &line_len, trace) != -1) {
		n = parse_trace_line(line, tp);
		if (n)
			continue;

		if (!strcmp(tp->action, "Q") && tp->len) {
			tim = trace_start + tp->nanoseconds / NS_IN_S;
			if (strchr(tp->rwbs_data, 'R') != NULL) {
				while(trace_blocks_to_extents(&tp->block,
							&tp->len, &extent,
							ssize, esize))
					add_block_read(activity, extent, tim,
							granularity);
				add_block_read(activity, extent, tim,
						granularity);
			} else if (strchr(tp->rwbs_data, 'W') != NULL ) {
				while(trace_blocks_to_extents(&tp->block,
							&tp->len, &extent,
							ssize, esize))
					add_block_write(activity, extent, tim,
							granularity);
				add_block_write(activity, extent, tim,
						granularity);
			} // ignore other types of operations
		}
	}

	free(command);
	pclose(trace);
	free(line);
	free(tp);
	return ret;
}

struct thread_param {
	struct activity_stats *activ;
	int32_t delay;
	char *file;
	int *ender;
};

static char *
create_temp_file_name(char *file) {
	char *ret = NULL;
	if (strrchr(file, '/') == NULL) {
		asprintf(&ret, ".%s", file);
		return ret;
	}

	char *tmp = strdup(file);
	char *last_slash = strrchr(tmp, '/');
	*last_slash = '\0';
	last_slash++;

	if (asprintf(&ret, "%s/.%s", tmp, last_slash) == -1) {
		free(tmp);
		return NULL;
	}

	free(tmp);
	return ret;
}

void *
disk_write_worker(void *in) {
	struct thread_param *tp = (struct thread_param *)in;

	char *tmp_file = create_temp_file_name(tp->file);

	for (;!*tp->ender;) {
		sleep(tp->delay);

		if (write_activity_stats(tp->activ, tmp_file)) {
			fprintf(stderr, "Error writing activity stats"
					" to file %s\n", tmp_file);
			unlink(tmp_file);
			continue;
		}

		rename(tmp_file, tp->file);
	}

	free(tp);
	free(tmp_file);
	return NULL;
}

void
signalHandler(int dummy) {
	programEnd = 1;
}

void
ignoreHandler(int dummy) {
}

struct program_params {
	size_t esize; /**< extent size */
	int64_t granularity;
	char *file;
	int64_t delay;
	char *lv_dev_name;
	int daemonize;
	int show_help;
};

void
usage(char *name) {
	printf("LVM TS collector daemon\n");
	printf("\n");
	printf("Usage: %s -f <stats-file> -l <LV-device> [OPTIONS]\n\n", name);
	printf("\t--extent-size n  Assume extent size of monitored device to `n` bytes\n");
	printf("\t--granularity m  Compact together io happening in `m` second intervals\n");
	printf("\t-f,--file f      Save gathered statistics to file `f`\n");
	printf("\t-l,--lv-dev d    Monitor device `d`\n");
	printf("\t-d,--debug       Don't daemonize, run in forground\n");
	printf("\t--delay l        How often write statistics to file (in seconds)\n");
	printf("\t-?,--help        This message\n");
}

int
parse_arguments(int argc, char **argv, struct program_params *pp) {
	assert(pp);
	int f_ret = 0;
	int c;

	// default parameters
	pp->esize = 4*1024*1024;
	pp->granularity = 60;
	pp->file = 0;
	pp->lv_dev_name = 0;
	pp->daemonize = 1;
	pp->show_help = 0;
	pp->delay = 60 * 5; // write dumps every 5 minutes

	struct option long_options[] = {
		{"extent-size",  required_argument, 0, 0 }, // 0
		{"granularity",  required_argument, 0, 0 }, // 1
		{"file",         required_argument, 0, 'f' }, // 2
		{"lv-dev",       required_argument, 0, 'l' }, // 3
		{"debug",        no_argument,       0, 'd' }, // 4
		{"help",         no_argument,       0, '?' }, // 5
		{"delay",        required_argument, 0, 0 }, // 6
		{0, 0, 0, 0}
	};

	int64_t tmp_lint;

	while(1) {
		int option_index = 0;

		c = getopt_long(argc, argv, "f:l:d?", long_options, &option_index);

		if (c == -1)
			break;

		switch(c) {
			case 0: /* long options */
				switch (option_index) {
					case 0: /* extent-size */
						tmp_lint = atoll(optarg);
						if (tmp_lint <= 0) {
							fprintf(stderr, "Invalid parameter to option `extent-size`\n");
							f_ret = 1;
							goto usage;
						}
						pp->esize = tmp_lint;
						break;
					case 1: /* granularity */
						tmp_lint = atoll(optarg);
						if (tmp_lint <= 0) {
							fprintf(stderr, "Invalid parameter to option `granularity`\n");
							f_ret = 1;
							goto usage;
						}
						pp->granularity = tmp_lint;
						break;
					case 6: /* delay */
						tmp_lint = atoll(optarg);
						if (tmp_lint <= 0) {
							fprintf(stderr, "Invalid parameter to option `delay`\n");
							f_ret = 1;
							goto usage;
						}
						pp->delay = tmp_lint;
						break;
					default:
						fprintf(stderr, "Unknown option %i\n",
								option_index);
						f_ret = -1;
						goto usage;
						break;
				}
				break;
			case 'f':
				pp->file = optarg;
				break;
			case 'l':
				pp->lv_dev_name = optarg;
				break;
			case 'd':
				pp->daemonize = 0;
				break;
			case '?':
				f_ret = 1;
				goto usage;
				break;
			default:
				f_ret = 1;
				fprintf(stderr, "Unknown option %c\n", c);
				goto usage;
		}
	}

	if (pp->file && pp->lv_dev_name)
		goto no_output;

	fprintf(stderr, "Must specify Logical Volume device name and path to "
			"statistics file\n");
	f_ret = 1;
usage:
	usage(argv[0]);

no_output:
	return f_ret;
}

void
daemonize() {

	if(daemon(1, 0)) {
		perror("Can't daemonize");
		exit(1);
	}
}

int
main(int argc, char **argv) {
	int ret = 0;
	struct activity_stats *activ = NULL;
	struct thread_param *tp = malloc(sizeof(struct thread_param));
	assert(tp);

	struct program_params pp = { 0 };

	if (parse_arguments(argc, argv, &pp)) {
		free(tp);
		return 1;
	}

	//activ = new_activity_stats_s(1<<10); // assume 2^11 extents (40GiB)
	if(read_activity_stats(&activ, pp.file)) {
		fprintf(stderr, "Can't read \"%s\". Ignoring.\n", pp.file);
		activ = new_activity_stats_s(1<<10); // assume 2^11 extents (40GiB)
	}

	if (pp.daemonize)
		daemonize();

	signal(SIGINT, signalHandler);
	signal(SIGTERM, signalHandler);
	signal(SIGHUP, ignoreHandler);

	tp->activ = activ;
	tp->delay = pp.delay;
	tp->file = pp.file;
	tp->ender = &programEnd;

	pthread_attr_t pt_attr;
	pthread_t thread;

	if (pthread_attr_init(&pt_attr)) {
		fprintf(stderr, "Can't initialize thread attribute\n");
		return 1;
	}

	if(pthread_create(&thread, &pt_attr, &disk_write_worker, tp)) {
		fprintf(stderr, "Can't create thread\n");
		return 1;
	}


	if (collect_trace_points(pp.lv_dev_name,
				 activ,
				 pp.granularity,
				 pp.esize,
				 &programEnd)) {
		fprintf(stderr, "Error while tracing");
		ret = 1;
	}

	fprintf(stderr, "Writing activity stats...");

	union sigval sigArg = {.sival_int=0};
	pthread_sigqueue(thread, SIGHUP, sigArg);

	void *thret;
	pthread_join(thread, &thret);

	destroy_activity_stats(activ);

	fprintf(stderr, "done\n");

	return ret;
}
