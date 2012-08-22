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
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>
#include "activity_stats.h"

#define HALF_LIFE 24*60*60*3.0L
#define MEAN_LIFETIME (HALF_LIFE/logl(2))
#define HIT_SCORE 16.0L

struct activity_stats*
new_activity_stats() {

	struct activity_stats *ret;

	ret = calloc(sizeof(struct activity_stats), 1);

	pthread_mutex_init(&ret->mutex, NULL);

	return ret;
}

struct activity_stats*
new_activity_stats_s(size_t blocks) {

	struct activity_stats *ret;

	ret = malloc(sizeof(struct activity_stats));

	ret->block = calloc(sizeof(struct block_activity), blocks + 1);
	ret->len = blocks + 1;

	pthread_mutex_init(&ret->mutex, NULL);

	return ret;
}

void
destroy_activity_stats(struct activity_stats *activity) {

	if (!activity)
		return;

	if (activity->block)
		free(activity->block);

	pthread_mutex_destroy(&activity->mutex);
	free(activity);
}

float
score_decay(float curr_score, uint64_t time, double mean_lifetime) {
    assert(mean_lifetime != 0);

    double score = curr_score; // use double precision for calculation
    score = score * exp(-1.0 * time / mean_lifetime); // exponential decay
    return score;
}

static void
add_block_activity_read(struct block_activity *block, int64_t time,
    double mean_lifetime, double hit_score) {

    uint64_t time_diff = time - block->read_time;
    if (time_diff <= 0)
      block->read_score += hit_score;
    else {
        block->read_score = score_decay(block->read_score, time_diff,
            mean_lifetime) + hit_score;
        block->read_time = time;
    }
}

static void
add_block_activity_write(struct block_activity *block, int64_t time,
    double mean_lifetime, double hit_score) {

    uint64_t time_diff = time - block->write_time;
    if (time_diff <= 0)
      block->write_score += hit_score;
    else {
        block->write_score = score_decay(block->write_score, time_diff,
            mean_lifetime) + hit_score;
        block->write_time = time;
    }
}

int
add_block(struct activity_stats *activity, int64_t off, int64_t time,
		double mean_lifetime, double hit_score, int type) {

	int ret = 0;
	struct block_activity *tmp;

	pthread_mutex_lock(&activity->mutex);

	// dynamically extend activity->block as new blocks are added
	if (!activity->block) {

		activity->block = calloc(sizeof(struct block_activity), off + 1);
		if (!activity->block) {
			ret = ENOMEM;
			goto mutex_cleanup;
		}

		activity->len = off + 1;

	} else if (activity->len <= off) {

		tmp = realloc(activity->block,
				sizeof(struct block_activity) * (off + 1));
		if (!tmp) {
			ret = ENOMEM;
			goto mutex_cleanup;
		}
		activity->block = tmp;

		memset(activity->block + activity->len,
			0,
			(off + 1 - activity->len)*sizeof(struct block_activity));
		activity->len = off + 1;
	}

	if (type == T_READ)
		add_block_activity_read(&(activity->block[off]), time, mean_lifetime, hit_score);
	else
		add_block_activity_write(&(activity->block[off]), time, mean_lifetime, hit_score);
mutex_cleanup:
	pthread_mutex_unlock(&activity->mutex);

	return ret;
}

int
add_block_read(struct activity_stats *activity, int64_t off, int64_t time,
    double mean_lifetime, double hit_score) {

	return add_block(activity, off, time, mean_lifetime, hit_score, T_READ);
}

int
add_block_write(struct activity_stats *activity, int64_t off, int64_t time,
    double mean_lifetime, double hit_score) {

	return add_block(activity, off, time, mean_lifetime, hit_score, T_WRITE);
}

struct block_activity*
get_block_activity(struct activity_stats *activity, off_t off)
{
    return &activity->block[off];
}

// return last access time, either read or write
time_t
get_last_access_time(struct block_activity *ba, int type)
{
    time_t last_access = 0;

    if (type == T_READ)
        last_access = ba->read_time;
    else
        last_access = ba->write_time;

    return last_access;
}

time_t
get_last_read_time(struct block_activity *ba)
{
    return get_last_access_time(ba, T_READ);
}

time_t
get_last_write_time(struct block_activity *ba)
{
    return get_last_access_time(ba, T_WRITE);
}

float
get_block_activity_raw_score(struct block_activity *block, int type)
{
    assert(type == T_READ || type == T_WRITE);

    if (type == T_READ)
        return block->read_score;
    else
        return block->write_score;
}

float
calculate_score(float read_score,
                time_t read_time,
                float read_multiplier,
                float write_score,
                time_t write_time,
                float write_multiplier,
                time_t curr_time,
                float scale)
{
    float score;

    time_t read_diff = curr_time - read_time;
    time_t write_diff = curr_time - write_time;

    float curr_read_score;
    float curr_write_score;

    curr_read_score = exp(-1.0 * scale * read_diff) * read_score;
    curr_write_score = exp(-1.0 * scale * write_diff) * write_score;

    score = curr_read_score * read_multiplier
      + curr_write_score * write_multiplier;

    return score;
}

float
get_block_score(struct activity_stats *activity, int64_t off, int type,
    double mean_lifetime) {

	double score = 0.0;
	time_t time_diff;

    struct block_activity *ba = get_block_activity(activity, off);

    if (type == T_READ) {
        time_diff = time(NULL) - ba->read_time;
        score = ba->read_score;
    } else {
        time_diff = time(NULL) - ba->write_time;
        score = ba->write_score;
    }

    if (time_diff > 0)
	    score *= exp(-1.0 * time_diff / mean_lifetime);

	return score;
}

float
get_block_write_score(struct activity_stats *activity, int64_t off,
    double mean_lifetime) {

	return get_block_score(activity, off, T_WRITE, mean_lifetime);
}

float
get_block_read_score(struct activity_stats *activity, int64_t off,
    double mean_lifetime) {

	return get_block_score(activity, off, T_READ, mean_lifetime);
}

float
get_raw_block_read_score(struct block_activity *ba)
{
    return get_block_activity_raw_score(ba, T_READ);
}

float
get_raw_block_write_score(struct block_activity *ba)
{
    return get_block_activity_raw_score(ba, T_WRITE);
}

void
dump_activity_stats(struct activity_stats *activity) {

	for (size_t i=0; i<activity->len; i++) {
		printf("block %8lu, last read:  %lu, read score:  %e\n",
				i, activity->block[i].read_time,
                activity->block[i].read_score);
        printf("block %8lu, last write: %lu, write score: %e\n",
				i, activity->block[i].write_time,
                activity->block[i].write_score);
	}
}

#define FILE_MAGIC 0xefabb773746d766cULL
#define OLD_MAGIC 0xffabb773746d766cULL

static int
write_block(struct block_activity *block, FILE *f) {
	int n;

    n = fwrite(&block->read_time, sizeof(uint64_t), 1, f);
    if (n != 1)
        return EIO;
    n = fwrite(&block->read_score, sizeof(float), 1, f);
    if (n != 1)
        return EIO;

    n = fwrite(&block->write_time, sizeof(uint64_t), 1, f);
    if (n != 1)
        return EIO;
    n = fwrite(&block->write_score, sizeof(float), 1, f);
    if (n != 1)
        return EIO;

	return 0;
}

static int
read_block(struct block_activity *block, FILE *f) {
	int n;
	float score;
	uint64_t time;

	clearerr(f);

    n = fread(&time, sizeof(uint64_t), 1, f);
    if (n != 1) {
        if (feof(f))
            return 2;
        else
            return 1;
    }
    n = fread(&score, sizeof(float), 1, f);
    if (n != 1) {
        if (feof(f))
            return 2;
        else
            return 1;
    }
    block->read_score = score;
    block->read_time = time;

    n = fread(&time, sizeof(uint64_t), 1, f);
    if (n != 1) {
        if (feof(f))
            return 2;
        else
            return 1;
    }
    n = fread(&score, sizeof(float), 1, f);
    if (n != 1) {
        if (feof(f))
            return 2;
        else
            return 1;
    }
    block->write_score = score;
    block->write_time = time;

	return 0;
}

int
write_activity_stats(struct activity_stats *activity, char *file) {

	assert(activity);
	assert(file);

	FILE *f;
	int ret = 0;
	char *tmp = NULL;
	int n;

	f = fopen(file, "w");
	if (!f) {
		if (asprintf(&tmp, "Can't open file \"%s\"", file) == -1) {
			fprintf(stderr, "Out of memory\n");
			return 1;
		}
		perror(tmp);
		free(tmp);
		return 1;
	}

	uint64_t magic = FILE_MAGIC;

	n = fwrite(&magic, sizeof(uint64_t), 1, f);
	if (n != 1) {
		ret = 1;
		goto file_cleanup;
	}

	n = fwrite(&(activity->len), sizeof(int64_t), 1, f);
	if (n != 1) {
		ret = 1;
		goto file_cleanup;
	}

	fseek(f, sizeof(int32_t)*3, SEEK_CUR);

	pthread_mutex_lock(&activity->mutex);

	for(size_t i=0; i<activity->len; i++) {
		n = write_block(&(activity->block[i]), f);
		if (n) {
			ret = n;
			break;
		}
	}

	pthread_mutex_unlock(&activity->mutex);

file_cleanup:
	fsync(fileno(f));
	fclose(f);

	return ret;
}

// TODO optimise file format so that reading it under valgrind doesn't take
// half a minute
int
read_activity_stats(struct activity_stats **activity, char *file) {
	assert(activity);
	int ret = 0;
	int n;
	char *tmp = NULL;
	FILE *f;

	f = fopen(file, "r");
	if (!f) {
		if (asprintf(&tmp, "Can't open file \"%s\"", file) == -1) {
			fprintf(stderr, "Out of memory\n");
			return 1;
		}
		perror(tmp);
		free(tmp);
		return 1;
	}

	uint64_t magic;

	n = fread(&magic, sizeof(uint64_t), 1, f);
	if (n != 1) {
		fprintf(stderr, "File read error\n");
		ret = 1;
		goto file_cleanup;
	}

    if (magic == OLD_MAGIC) {
        fprintf(stderr, "Old file format detected. Remove the file and generate new data\n");
        ret = 1;
        goto file_cleanup;
    }

	if (magic != FILE_MAGIC) {
		fprintf(stderr, "File format error, magic value incorrect\n");
		ret = 1;
		goto file_cleanup;
	}

	uint64_t len;
	n = fread(&len, sizeof(uint64_t), 1, f);
	if (n != 1) {
		fprintf(stderr, "File read error\n");
		ret = 1;
		goto file_cleanup;
	}

	*activity = new_activity_stats_s(len-1);
	if (!*activity) {
		fprintf(stderr, "Out of memory\n");
		ret = 1;
		goto file_cleanup;
	}

	fseek(f, sizeof(int32_t)*3, SEEK_CUR);

	for(size_t i=0; i<(*activity)->len; i++) {
		n = read_block(&((*activity)->block[i]), f);
		if (n == 2)
			break;
		if (n) {
			fprintf(stderr, "File read error\n");
			ret = n;
			goto activity_cleanup;
		}
	}

	goto file_cleanup;

activity_cleanup:
	destroy_activity_stats(*activity);
	*activity = NULL;

file_cleanup:
	fclose(f);

	return ret;
}

// add data about a block, extending the bs structure (assumes that enough
// memory has already been allocated)
static void
add_score_to_block_scores(struct block_scores *bs, size_t size,
    struct block_scores *block)
{
  // empty set case
  if (size == 0) {
    memcpy(bs, block, sizeof(struct block_scores));
    return;
  }

  // when is better than one of the elements
  // TODO can be made n log n, instead of n^2, by using binary search instead
  // of linear
  for (size_t i=0; i<size; i++) {
    if (block->score > bs[i].score) {
      memmove(&(bs[i+1]), &(bs[i]), sizeof(struct block_scores) * (size-i));
      memcpy(&(bs[i]), block, sizeof(struct block_scores));
      return;
    }
  }

  // when new element is worse than all others
  memcpy(&(bs[size]), block, sizeof(struct block_scores));

  return;
}

// Add new data about a block, removing the worst one off the list
static void
insert_score_to_block_scores(struct block_scores *bs, size_t size,
    struct block_scores *block)
{
  assert(bs);
  assert(block);
  assert(size>0);

  if (block->score < bs[size-1].score)
    return;

  // TODO can be made n log n, instead of n^2, by using binary search instead
  // of linear
  for (size_t i=0; i<size; i++) {
    if (block->score > bs[i].score) {
      memmove(&(bs[i+1]), &(bs[i]), sizeof(struct block_scores) * (size-i-1));
      memcpy(&(bs[i]), block, sizeof(struct block_scores));
      return;
    }
  }

  return;
}

// dump collected block_scores
void
print_block_scores(struct block_scores *bs, size_t size)
{
  for(size_t i=0; i<size; i++)
    printf("block %10li score: %8f\n", bs[i].offset, bs[i].score);
}

/**
 * Return "size" best blocks from provided activity stats
 *
 * @val activity activity stats to read data from
 * @val bs[out] found activity stats, sorted from most to least active
 * @val size number of best blocks to return, also size of bs to allocate,
 * if *bs is non NULL
 * @val read_multiplier How much are reads more important than writes, provide
 * 0 to get only writes
 * @val write_multiplier How much are writes more important than reads, provide
 * 0 to get only reads
 * @val mean_lifetime Exponential time constant in exponential decay
 * @return 0 if everything is OK, non zero if it isn't
 */
int
get_best_blocks(struct activity_stats *activity, struct block_scores **bs,
        size_t size, int read_multiplier, int write_multiplier,
        double mean_lifetime)
{
  assert(read_multiplier || write_multiplier);
  int f_ret = 0;

  if (!*bs)
    *bs = malloc(sizeof(struct block_scores)*size);

  if (!*bs) {
    f_ret = 1;
    goto no_cleanup;
  }

  struct block_scores block;

  for (size_t i=0; i<size; i++) {
    block.offset = i;
    block.score = get_block_read_score(activity, i, mean_lifetime) * read_multiplier +
      get_block_write_score(activity, i, mean_lifetime) * write_multiplier;
    add_score_to_block_scores(*bs, i, &block);
  }

  for (size_t i=size; i<activity->len; i++) {
    block.score = get_block_read_score(activity, i, mean_lifetime) * read_multiplier +
      get_block_write_score(activity, i, mean_lifetime) * write_multiplier;
    if (block.score > (*bs)[size-1].score) {
      block.offset = i;
      insert_score_to_block_scores(*bs, size, &block);
    }
  }

no_cleanup:
  return f_ret;
}

/** get best n blocks with score equal and lower than provided
 * @val activity activity stats to read data from
 * @val bs[out] found activity stats, sorted from most to least active
 * @val size number of best blocks to return, also size of bs to allocate,
 * if *bs is non NULL
 * @val read_multiplier How much are reads more important than writes, provide
 * 0 to get only writes
 * @val write_multiplier How much are writes more important than reads, provide
 * 0 to get only reads
 * @val max_score Highest score a block can have
 * @return 0 if everything is OK, non zero if it isn't
 */
int
get_best_blocks_with_max_score(struct activity_stats *activity,
        struct block_scores **bs, size_t size, int read_multiplier,
        int write_multiplier, double mean_lifetime, float max_score)
{
  assert(read_multiplier || write_multiplier);
  int f_ret = 0;

  if (!*bs)
    *bs = malloc(sizeof(struct block_scores)*size);

  if (!*bs) {
    f_ret = 1;
    goto no_cleanup;
  }

  struct block_scores block;

  size_t count=0;
  size_t i=0;
  for (; count<size && count < activity->len; i++) {
    block.offset = i;
    block.score = get_block_read_score(activity, i, mean_lifetime) * read_multiplier +
      get_block_write_score(activity, i, mean_lifetime) * write_multiplier;
    if (block.score > max_score)
	continue;
    add_score_to_block_scores(*bs, count, &block);
    count++;
  }

  if (count != size)
    return f_ret; // there are less qualifying blocks in activity that places in block_scores

  for (; i<activity->len; i++) {
    block.score = get_block_read_score(activity, i, mean_lifetime) * read_multiplier +
      get_block_write_score(activity, i, mean_lifetime) * write_multiplier;
    if (block.score > max_score)
	  continue;
    if (block.score > (*bs)[size-1].score) {
      block.offset = i;
      insert_score_to_block_scores(*bs, size, &block);
    }
  }

no_cleanup:
  return f_ret;
}
