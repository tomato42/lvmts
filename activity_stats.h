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
#ifndef _ACTIVITY_STATS_H_
#define _ACTIVITY_STATS_H_
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>

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
	pthread_mutex_t mutex;
};

struct block_scores {
    int64_t offset;
    float score;
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
void print_block_scores(struct block_scores *bs, size_t size);

int write_activity_stats(struct activity_stats *activity, char *file);

int read_activity_stats(struct activity_stats **activity, char *file);

int get_best_blocks(struct activity_stats *activity, struct block_scores **bs,
    size_t size, int read_multiplier, int write_multiplier);

#endif
