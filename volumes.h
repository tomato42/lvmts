#ifndef _VOLUMES_H
#define _VOLUMES_H

#include "extents.h"

// stub
struct program_params;

/**
 * Return size of extents (in bytes) for provided volume name
 */
off_t get_extent_size(struct program_params *pp, char *lv_name);

/**
 * Return volume name of first defined logical volume in config file
 */
char *get_first_volume_name(struct program_params *pp);

/**
 * Select best extents that conform to provided criteria
 *
 * @var es statistics of extent to get data from
 * @var ret[out] returned list of extents
 * @var pp general program parameters
 * @var lv_name volume name of which extents are to be selected
 * @var max_tier don't select extents from tier higher than this (or lower
 *      in case of hot_cold == ES_COLD)
 * @var max_extents don't return more than this much extents
 * @var hot_cold return hottest (ES_HOT) or coldest extents (ES_COLD)
 */
int extents_selector(   struct extent_stats *es,
                        struct extents **ret,
                        struct program_params *pp,
                        char *lv_name,
                        int max_tier,
                        int max_extents,
                        int hot_cold);

/**
 * Pull statistics from lvmtsm file
 */
int get_volume_stats(struct program_params *pp, char *lv_name,
    struct extent_stats **es);

#endif
