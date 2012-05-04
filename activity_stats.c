#include <stdio.h>
#include <string.h>
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
