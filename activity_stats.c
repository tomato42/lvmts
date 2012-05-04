#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include "activity_stats.h"

struct activity_stats*
new_activity_stats() {

	struct activity_stats *ret;

	ret = calloc(sizeof(struct activity_stats), 1);

	return ret;
}

struct activity_stats*
new_activity_stats_s(size_t blocks) {

	struct activity_stats *ret;

	ret = malloc(sizeof(struct activity_stats));

	ret->block = calloc(sizeof(struct block_activity), blocks + 1);
	ret->len = blocks + 1;

	return ret;
}

void
destroy_activity_stats(struct activity_stats *activity) {

	if (!activity)
		return;

	if (activity->block)
		free(activity->block);
	free(activity);
}

void
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

void
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

	struct block_activity *tmp;

	// dynamically extend activity->block as new blocks are added
	if (!activity->block) {

		activity->block = calloc(sizeof(struct block_activity), off + 1);
		if (!activity->block)
			return ENOMEM;

		activity->len = off + 1;

	} else if (activity->len <= off) {

		tmp = realloc(activity->block,
				sizeof(struct block_activity) * (off + 1));
		if (!tmp)
			return ENOMEM;
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

	return 0;
}

int
add_block_read(struct activity_stats *activity, int64_t off, int64_t time, int granularity) {

	return add_block(activity, off, time, granularity, T_READ);
}

int
add_block_write(struct activity_stats *activity, int64_t off, int64_t time, int granularity) {

	return add_block(activity, off, time, granularity, T_WRITE);
}

void
dump_activity_stats(struct activity_stats *activity) {

	for (size_t i=0; i<activity->len; i++) {
		if (activity->block[i].reads[0].hits)
			for(int j=0; j<HISTORY_LEN; j++) {
				if (activity->block[i].reads[j].hits == 0)
					break;
				printf("block read: %lu, time: %lu, hits: %i\n",
					i, activity->block[i].reads[j].time,
					activity->block[i].reads[j].hits);
			}
		if (activity->block[i].writes[0].hits)
			for (int j=0; j<HISTORY_LEN; j++) {
				if (activity->block[i].writes[j].hits == 0)
					break;
				printf("block write: %lu, time: %lu, hits: %i\n",
					i, activity->block[i].writes[j].time,
					activity->block[i].writes[j].hits);
			}
	}
}

#define FILE_MAGIC 0xffabb773746d766cULL

int
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

int
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

	for(size_t i=0; i<activity->len; i++) {
		n = write_block(&(activity->block[i]), f);
		if (n) {
			ret = n;
			goto file_cleanup;
		}
	}

file_cleanup:
	fclose(f);

	return ret;
}

int read_activity_stats(struct activity_stats **activity, char *file) {
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
