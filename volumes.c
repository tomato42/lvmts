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
#include <assert.h>
#include <string.h>
#include "config.h"
#include "volumes.h"
#include "activity_stats.h"
#include "lvmls.h"

off_t
get_extent_size(struct program_params *pp, char *lv_name)
{
    return 1;
}

char *
get_first_volume_name(struct program_params *pp)
{
    return NULL;
}

int extents_selector(struct extent_stats *es, struct extents **ret,
    struct program_params *pp, char *lv_name, int max_tier, int max_extents,
    int hot_cold)
{
    return 1;
}

int
get_volume_stats(struct program_params *pp, char *lv_name, struct extent_stats **es)
{
    assert(es);
    int f_ret = 0; // this function return code
    int ret;

    time_t now = time(NULL);

    *es = malloc(sizeof(struct extent_stats));
    if (*es)
        return -1;

    struct activity_stats *as;

    static char *hack = "dane-stacja.lvmts"; // XXX get from params

    ret = read_activity_stats(&as, hack);
    assert(ret);

    (*es)->extents = malloc(sizeof(struct extent) * as->len);
    assert((*es)->extents); // XXX better error checking
    (*es)->length = as->len;

    init_le_to_pe();

    float read_mult = get_read_multiplier(pp, lv_name);
    float write_mult = get_write_multiplier(pp, lv_name);
    float hit_score = get_hit_score(pp, lv_name);
    float scale = get_score_scaling_factor(pp, lv_name);

    for(size_t i=0; i < as->len; i++) {
        struct extent *e = &((*es)->extents[i]);

        struct pv_info *pv_i;
        pv_i = LE_to_PE("dane", "stacja", i);

        struct block_activity *ba = get_block_activity(as, i);

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

    le_to_pe_exit();

    destroy_activity_stats(as);

    return f_ret;
}
