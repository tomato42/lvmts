#ifndef _EXTENTS_H
#define _EXTENTS_H

#include <time.h>
#include <unistd.h>
#include <stdint.h>

struct program_params;

/** extent statistics */
struct extent_stats {
    struct extent *extents;
    size_t length;
    char *lv_name;
};

/** single extent data */
struct extent {
    char *dev; // reference, not allocated string
    off_t pe;
    off_t le;
    float score; // calculated score at time of stats reading
    float read_score; // read score at time last_read_access
    time_t last_read_access;
    float write_score; // write score at time last_write_access
    time_t last_write_access;
};

/**
 * List of sorted extent references according to score
 */
struct extents {
    struct extent **extents;
    size_t length;
    int sort;
};

void free_extents(struct extents *e);

void free_extent_stats(struct extent_stats *es);

void free_extent(struct extent *e); // do nothing

/** strcmp for extents
 * @return -1 if e1 is "smaller" than e2, 0 if they're equal and 1 if e1 is
 * "bigger" than e2
 */
int compare_extents(struct extents *e1, struct extents *e2);

// sort order selection
enum hot_cold {
    ES_HOT = 1, // for getting hottest extents
    ES_COLD = 2 // for getting coldest extents
};

/**
 * Return embedded score from provided reference
 */
float get_extent_score(struct extent *e);

/** Returns a reference for nmemb'th extent in *e
 */
struct extent * get_extent(struct extents *e, size_t nmemb);

/**
 * Count extents that are colder/hotter than provided score
 */
int count_extents(struct extents *e, float score, int hot_cold);

/**
 * truncate (remove from end) extents to leave len positions
 */
void truncate_extents(struct extents *e, size_t len);

/**
 * Returns tier of provided extent in logical volume
 */
int get_extent_tier(struct program_params *pp, const char *lv_name,
    struct extent *e);

#endif
