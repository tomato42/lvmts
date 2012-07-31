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

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include "config.h"
#include "volumes.h"
#include "activity_stats.h"
#include "lvmls.h"

// TODO stub
off_t
get_extent_size(struct program_params *pp, char *lv_name)
{
    return 4 * 1024 * 1024;
}

// TODO stub
char *
get_first_volume_name(struct program_params *pp)
{
    return "stacja-dane";
}

// selects best or worst extents in collection not residing on specific devices
int extents_selector(struct extent_stats *es, struct extents **ret,
    struct program_params *pp, char *lv_name, int max_tier, int max_extents,
    int hot_cold)
{
    assert(ret);
    assert(hot_cold == ES_HOT || hot_cold == ES_COLD);

    *ret = malloc(sizeof(struct extents));

    (*ret)->length = 0;
    (*ret)->sort = hot_cold;

    // assume we will find `max_extent` extents
    (*ret)->extents = malloc(sizeof(struct extent*) * max_extents);
    assert((*ret)->extents); // TODO error handling

    // a bit complicated for(;;), basically it searches from 0 when we're
    // looking for hot extents and searches from last extent in `es` in case
    // we're looking for cold extents. In both cases it doesn't load more than
    // max_extents to ret
    for(ssize_t i=(hot_cold == ES_HOT)?0:es->length-1;
        (hot_cold == ES_HOT && i < es->length && (*ret)->length < max_extents)
            || (i >= 0 && (*ret)->length < max_extents);
        i = (hot_cold == ES_HOT)?i+1:i-1) {

        int tier = get_extent_tier(pp, lv_name, &es->extents[i]);

        if ((hot_cold == ES_HOT && max_tier > tier) || max_tier < tier) {
            (*ret)->extents[(*ret)->length] = &es->extents[i];
            (*ret)->length++;
        }
    }

    return 0;
}

// comparison function for sorting extents according to their score
static int
extent_compare(const void *v1, const void *v2)
{
    struct extent *e1 = (struct extent *)v1;
    struct extent *e2 = (struct extent *)v2;

    if (e1->score > e2->score)
      return -1;
    if (e1->score < e2->score)
      return 1;

    return 0;
}

int
get_volume_stats(struct program_params *pp, char *lv_name, struct extent_stats **es)
{
    assert(es);
    int f_ret = 0; // this function return code
    int ret;

    // use the same time base to calculate score of all extents
    time_t now = time(NULL);

    *es = malloc(sizeof(struct extent_stats));
    if (!*es) {
        fprintf(stderr, "Out of memory\n");
        return -1;
    }

    struct activity_stats *as;

    static char *hack = "dane-stacja.lvmts"; // XXX get from params

    // read activity stats from file generated by lvmtsmd
    ret = read_activity_stats(&as, hack);
    assert(!ret);

    (*es)->extents = malloc(sizeof(struct extent) * as->len);
    assert((*es)->extents); // XXX better error checking
    (*es)->length = as->len;

    // load LE to PE translation tables
    init_le_to_pe();

    // collect general volume parameters
    float read_mult = get_read_multiplier(pp, lv_name);
    float write_mult = get_write_multiplier(pp, lv_name);
    float hit_score = get_hit_score(pp, lv_name);
    float scale = get_score_scaling_factor(pp, lv_name);

    for(size_t i=0; i < as->len; i++) {
        // just a shorthand, so that we wouldn't have to write full (*es)->...
        struct extent *e = &((*es)->extents[i]);

        // get logical extent to physical extent mapping for extent
        struct pv_info *pv_i;
        pv_i = LE_to_PE("stacja", "dane", i); // XXX get from params/config
        if (!pv_i) {
            fprintf(stderr, "error when translating extent %li\n", i);
            fprintf(stderr, "Do you have permission to access lvm device?\n");
            abort();
        }

        // get activity stats for block
        struct block_activity *ba = get_block_activity(as, i);

        // save collected data
        e->dev = strdup(pv_i->pv_name);
        e->le = i;
        e->pe = pv_i->start_seg;
        e->read_score =
            get_block_activity_raw_score(ba, T_READ, hit_score, scale);
        e->last_read_access = get_last_read_time(ba);
        e->write_score =
            get_block_activity_raw_score(ba, T_WRITE, hit_score, scale);
        e->last_write_access = get_last_write_time(ba);

        e->score = calculate_score( e->read_score,
                                    e->last_read_access,
                                    read_mult,
                                    e->write_score,
                                    e->last_write_access,
                                    write_mult,
                                    now,
                                    scale);

        pv_info_free(pv_i);
    }

    // clean up
    le_to_pe_exit();
    destroy_activity_stats(as);

    // sort according to score
    qsort((*es)->extents, (*es)->length, sizeof(struct extent),
        extent_compare);

    return f_ret;
}
