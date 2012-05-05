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
#include "activity_stats.h"

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

int64_t inline
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
#define TRACE_APP "btrace"
collect_trace_points(char *device,
		     struct activity_stats *activity,
		     int64_t granularity,
		     size_t esize) {
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

	while(getline(&line, &line_len, trace) != -1) {
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

int
main(int argc, char **argv) {
	int ret = 0;

	if (collect_trace_points("/dev/test/test")) {
		fprintf(stderr, "Error while tracing");
		ret = 1;
	}

	return ret;
}
