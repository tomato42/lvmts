#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>

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

int
collect_trace_points(char *device) {
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

		if (!strcmp(tp->action, "Q")) {
			if (strchr(tp->rwbs_data, 'R') != NULL) {
				printf("R: %"PRIi64" - %"PRIi64"\n", tp->block,
						tp->len);
			} else {
				printf("W: %"PRIi64" - %"PRIi64"\n", tp->block,
						tp->len);
			}
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
