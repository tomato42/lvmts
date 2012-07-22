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

static void
add_block_activity_read(struct block_activity *block, int64_t time, int granularity) {

	if (block->reads[0].time + granularity >= time
	    && block->reads[0].hits < HITS_MAX) {

		block->reads[0].hits++;
		return;
	}

	memmove(&(block->reads[1]), &(block->reads[0]),
		sizeof(struct rw_activity) * (HISTORY_LEN - 1));
	memset(block->reads, 0, sizeof(struct rw_activity));

	block->reads[0].time = time;
	block->reads[0].hits = 1;
}

static void
add_block_activity_write(struct block_activity *block, int64_t time, int granularity) {

	if (block->writes[0].time + granularity >= time
	    && block->writes[0].hits < HITS_MAX) {

		block->writes[0].hits++;
		return;
	}

	memmove(&(block->writes[1]), &(block->writes[0]),
		sizeof(struct rw_activity) * (HISTORY_LEN - 1));
	memset(block->writes, 0, sizeof(struct rw_activity));

	block->writes[0].time = time;
	block->writes[0].hits = 1;
}

#define T_READ 1
#define T_WRITE 2

int
add_block(struct activity_stats *activity, int64_t off, int64_t time,
		int granularity, int type) {

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
		add_block_activity_read(&(activity->block[off]), time, granularity);
	else
		add_block_activity_write(&(activity->block[off]), time, granularity);
mutex_cleanup:
	pthread_mutex_unlock(&activity->mutex);

	return ret;
}

int
add_block_read(struct activity_stats *activity, int64_t off, int64_t time, int granularity) {

	return add_block(activity, off, time, granularity, T_READ);
}

int
add_block_write(struct activity_stats *activity, int64_t off, int64_t time, int granularity) {

	return add_block(activity, off, time, granularity, T_WRITE);
}

#define HIT_SCORE 16.0L
#define SCALE (pow(2,-15))
float
get_block_score(struct activity_stats *activity, int64_t off, int type) {

	double score = 0.0;
	time_t last_access = 0;
	int len = 0;
	int i;
	for(i=0; i<HISTORY_LEN; i++)
		if ((type == T_READ && activity->block[off].reads[i].hits == 0)
		   || (type == T_WRITE && activity->block[off].writes[i].hits == 0))
			break;
	len = i;

	if (type == T_READ) {
		last_access = activity->block[off].reads[i].time;
		score = activity->block[off].reads[i].hits * HIT_SCORE;
	} else if (type == T_WRITE) {
		last_access = activity->block[off].writes[i].time;
		score = activity->block[off].writes[i].hits * HIT_SCORE;
	} else {
		assert(1);
	}

	time_t time_diff;
	for(int i=len-1; i>=0; i--) {
		if (type == T_READ) {
			time_diff = activity->block[off].reads[i].time - last_access;
			last_access = activity->block[off].reads[i].time;
		} else if (type == T_WRITE) {
			time_diff = activity->block[off].writes[i].time - last_access;
			last_access = activity->block[off].writes[i].time;
		} else
			assert(1);

		score *= exp(-1.0 * SCALE * time_diff);

		if (type == T_READ) {
			score += activity->block[off].reads[i].hits * HIT_SCORE;
		} else if (type == T_WRITE) {
			score += activity->block[off].writes[i].hits * HIT_SCORE;
		} else
			assert (1);
	}

	time_diff = time(NULL) - last_access;

	score *= exp(-1.0 * SCALE * time_diff);

	return score;
}

float
get_block_write_score(struct activity_stats *activity, int64_t off) {

	return get_block_score(activity, off, T_WRITE);
}

float
get_block_read_score(struct activity_stats *activity, int64_t off) {

	return get_block_score(activity, off, T_READ);
}

void
dump_activity_stats(struct activity_stats *activity) {

	for (size_t i=0; i<activity->len; i++) {
		if (activity->block[i].reads[0].hits) {
			printf("block %8lu, last access: %lu, read score:  %e\n",
				i, activity->block[i].reads[0].time,
				get_block_read_score(activity, i));
			for(int j=0; j<HISTORY_LEN; j++) {
				if (activity->block[i].reads[j].hits == 0)
					break;
			//	printf("block read: %lu, time: %lu, hits: %i\n",
			//		i, (uint64_t)activity->block[i].reads[j].time,
			//		activity->block[i].reads[j].hits);
			}
		}
		if (activity->block[i].writes[0].hits) {
			printf("block %8lu, last access: %lu, write score: %e\n",
				i, activity->block[i].writes[0].time,
				get_block_write_score(activity, i));
			for (int j=0; j<HISTORY_LEN; j++) {
				if (activity->block[i].writes[j].hits == 0)
					break;
			//	printf("block write: %lu, time: %lu, hits: %i\n",
			//		i, (uint64_t)activity->block[i].writes[j].time,
			//		activity->block[i].writes[j].hits);
			}
		}
	}
}

#define FILE_MAGIC 0xffabb773746d766cULL

static int
write_block(struct block_activity *block, FILE *f) {
	int n;
	int16_t hits;
	uint64_t time;

	for(int j=0; j<HISTORY_LEN; j++) {
		if (block->reads[j].hits == 0) {
			n = fseek(f, sizeof(uint64_t) + sizeof(int16_t), SEEK_CUR);
			if (n)
				return errno;
		} else {
			hits = block->reads[j].hits;
			time = block->reads[j].time;
			n = fwrite(&time, sizeof(uint64_t), 1, f);
			if (n != 1)
				return EIO;
			n = fwrite(&hits, sizeof(int16_t), 1, f);
			if (n != 1)
				return EIO;
		}
	}

	for(int j=0; j<HISTORY_LEN; j++) {
		if (block->writes[j].hits == 0) {
			n = fseek(f, sizeof(uint64_t) + sizeof(int16_t), SEEK_CUR);
			if (n)
				return errno;
		} else {
			hits = block->writes[j].hits;
			time = block->writes[j].time;
			n = fwrite(&time, sizeof(uint64_t), 1, f);
			if (n != 1)
				return EIO;
			n = fwrite(&hits, sizeof(int16_t), 1, f);
			if (n != 1)
				return EIO;
		}
	}

	return 0;
}

static int
read_block(struct block_activity *block, FILE *f) {
	int n;
	int16_t hits;
	uint64_t time;

	clearerr(f);

	for (int j=0; j<HISTORY_LEN; j++) {
		n = fread(&time, sizeof(uint64_t), 1, f);
		if (n != 1) {
			if (feof(f))
				return 2;
			else
				return 1;
		}
		n = fread(&hits, sizeof(int16_t), 1, f);
		if (n != 1) {
			if (feof(f))
				return 2;
			else
				return 1;
		}
		block->reads[j].hits = hits;
		block->reads[j].time = time;
	}
	for (int j=0; j<HISTORY_LEN; j++) {
		n = fread(&time, sizeof(uint64_t), 1, f);
		if (n != 1) {
			if (feof(f))
				return 2;
			else
				return 1;
		}
		n = fread(&hits, sizeof(int16_t), 1, f);
		if (n != 1) {
			if (feof(f))
				return 2;
			else
				return 1;
		}
		block->writes[j].hits = hits;
		block->writes[j].time = time;
	}

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

	int32_t hist_len = HISTORY_LEN;

	n = fwrite(&hist_len, sizeof(int32_t), 1, f);
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

	if (magic != FILE_MAGIC) {
		fprintf(stderr, "File format error, no magic value present\n");
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

	int32_t hist_len;

	n = fread(&hist_len, sizeof(int32_t), 1, f);
	if (n != 1) {
		fprintf(stderr, "File read error\n");
		ret = 1;
		goto activity_cleanup;
	}

	if (hist_len != HISTORY_LEN) {
		fprintf(stderr, "Incompatible file format\n");
		ret = 1;
		goto activity_cleanup;
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
 * @return 0 if everything is OK, non zero if it isn't
 */
int
get_best_blocks(struct activity_stats *activity, struct block_scores **bs,
        size_t size, int read_multiplier, int write_multiplier)
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
    block.score = get_block_read_score(activity, i) * read_multiplier +
      get_block_write_score(activity, i) * write_multiplier;
    add_score_to_block_scores(*bs, i, &block);
  }

  for (size_t i=size; i<activity->len; i++) {
    block.score = get_block_read_score(activity, i) * read_multiplier +
      get_block_write_score(activity, i) * write_multiplier;
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
get_best_blocks_with_max_score(struct activity_stats *activity, struct block_scores **bs,
        size_t size, int read_multiplier, int write_multiplier, float max_score)
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
    block.score = get_block_read_score(activity, i) * read_multiplier +
      get_block_write_score(activity, i) * write_multiplier;
    if (block.score > max_score)
	continue;
    add_score_to_block_scores(*bs, count, &block);
    count++;
  }

  if (count != size)
    return f_ret; // there are less qualifying blocks in activity that places in block_scores

  for (; i<activity->len; i++) {
    block.score = get_block_read_score(activity, i) * read_multiplier +
      get_block_write_score(activity, i) * write_multiplier;
    if (block.score > (*bs)[size-1].score) {
      block.offset = i;
      insert_score_to_block_scores(*bs, size, &block);
    }
  }

no_cleanup:
  return f_ret;
}
