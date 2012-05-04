#ifndef _ACTIVITY_STATS_H_
#define _ACTIVITY_STATS_H_
#include <stdint.h>
#include <stdlib.h>

#define HISTORY_LEN 10
#define TIME_MAX ((1<<51)-1)
#define HITS_MAX ((1<<11)-1)

struct rw_activity {
	uint64_t time:52;
	uint64_t hits:12;
};

struct block_activity {
	struct rw_activity reads[HISTORY_LEN];
	struct rw_activity writes[HISTORY_LEN];
};

struct activity_stats {
	struct block_activity *block;
	int64_t len;
};

struct activity_stats* new_activity_stats();
struct activity_stats* new_activity_stats_s(size_t);

void destroy_activity_stats(struct activity_stats *);

int add_block_read(struct activity_stats *activity,
		    int64_t off,
		    int64_t time,
		    int granularity);

int add_block_write(struct activity_stats *activity,
		     int64_t off,
		     int64_t time,
		     int granularity);

/* print statistics to stdout */
void dump_activity_stats(struct activity_stats *activity);

int write_activity_stats(struct activity_stats *activity, char *file);

int read_activity_stats(struct activity_stats **activity, char *file);

#endif
