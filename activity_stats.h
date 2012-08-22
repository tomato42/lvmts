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

#define T_READ 1
#define T_WRITE 2

struct block_activity {
    uint64_t read_time;
    uint64_t write_time;
    float    read_score;
    float    write_score;
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
		    double mean_lifetime,
            double hit_score);

int add_block_write(struct activity_stats *activity,
		     int64_t off,
		     int64_t time,
		     double mean_lifetime,
             double hit_score);

/* print statistics to stdout */
void dump_activity_stats(struct activity_stats *activity);
void print_block_scores(struct block_scores *bs, size_t size);

int write_activity_stats(struct activity_stats *activity, char *file);

int read_activity_stats(struct activity_stats **activity, char *file);

int get_best_blocks(struct activity_stats *activity, struct block_scores **bs,
    size_t size, int read_multiplier, int write_multiplier,
    double mean_lifetime);

int get_best_blocks_with_max_score(struct activity_stats *activity,
		struct block_scores **bs, size_t size, int read_multiplier,
		int write_multiplier, double mean_lifetime, float max_score);

/**
 * returns reference to activity stats from single block
 */
struct block_activity* get_block_activity(struct activity_stats *activity,
        off_t off);

/**
 * return raw read score, not adjusted for current time
 */
float get_raw_block_read_score(struct block_activity *ba);

/**
 * return time of last read access to block
 */
time_t get_last_read_time(struct block_activity *ba);

/**
 * return raw write score, not adjusted for current time
 */
float get_raw_block_write_score(struct block_activity *ba);

/**
 * return time of last write access to block
 */
time_t get_last_write_time(struct block_activity *ba);

/**
 * calculate block score at provided time
 */
float calculate_score(  float read_score,
                        time_t read_time,
                        float read_multiplier,
                        float write_score,
                        time_t write_time,
                        float write_multiplier,
                        time_t curr_time,
                        float exp);

/**
 * Return raw read and write scores for single block
 */
float get_block_activity_raw_score(struct block_activity *block, int type);

#endif
