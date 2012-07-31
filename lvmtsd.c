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
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include "lvmls.h"
#include "extents.h"
#include "volumes.h"
#include "config.h"

/** maximum number of storage tiers */
#define TIER_MAX 65536

/** variable defining ordered shutdown */
int stop = 0;

struct program_params {
    char *conf_file_path;
    int pvmove_wait;
};

/** Create new program_params with default settings */
static struct program_params*
new_program_params()
{
  struct program_params *pp;

  pp = calloc(sizeof(struct program_params),1);
  if (!pp)
    return NULL;

  pp->conf_file_path = strdup("/etc/lvmts/lvmtsd.conf");
  pp->pvmove_wait = 5 * 60; // 5 minutes

  return pp;
}

/** destroy program_params */
void
free_program_params(struct program_params *pp)
{
  if(!pp)
    return;

  if(pp->conf_file_path)
    free(pp->conf_file_path);

  free(pp);
}

/** parse CLI arguments */
static int
parse_arguments(int argc, char **argv, struct program_params *pp)
{
  return 0;
}

/** read configuration file */
static int
read_config(struct program_params *pp)
{
  return 0;
}

/** starts collector deamon processes */
static int
run_collector_daemon(struct program_params *pp)
{
  return 0;
}

/** checks if there are any stats collected */
static int
stats_available(struct program_params *pp)
{
  return 0;
}

/** Possible move daemon statuses */
enum pvmove_status {
    PVMOVE_IDLE = 0,
    PVMOVE_WORKING = 1
};

/** returns status of pvmove daemon */
static int
move_daemon_status(struct program_params *pp, char *lv_name)
{
    return PVMOVE_IDLE;
}

// TODO
static int
queue_extents_move(struct extents *ext, struct program_params *pp,
    int dst_tier)
{
    for(size_t i = 0; i < ext->length; i++) {
        printf("pvmove %s:%li %s\n", ext->extents[i].dev,
            ext->extents[i].pe, get_tier_device(pp, dst_tier));
    }
    return 0;
}

static off_t
get_avaiable_space(struct program_params *pp, char *lv_name, int tier)
{
    if (tier <= 2)
        return 4 * 1024 * 1024 * 200;

    return 0;
}

static int
lower_tiers_exist(struct program_params *pp, char *lv_name, int tier)
{
    return 0;
}

static int
add_pinning_scores(struct extent_stats *es, struct program_params *pp,
    char *lv_name)
{
  return 0;
}

static int
higher_tiers_exist(struct program_params *pp, char *lv_name, int tier)
{
    if (tier < 2)
        return 1;
    else
        return 0;
}

/** controlling daemon main loop */
static int
main_loop(struct program_params *pp)
{
    int ret;

    char *lv_name = get_first_volume_name(pp);

    while (stop) {

        // if move daemon is working, wait 5 minutes, start from beginning
        switch(move_daemon_status(pp, lv_name)) {
          case PVMOVE_WORKING:
            sleep(pp->pvmove_wait);
            continue;
            break;
          case PVMOVE_IDLE:
            break; /* do nothing */
          default:
            fprintf(stderr, "Unknown response from move_daemon_status()\n");
            break;
        }

        // refresh stats, read new stats from file
        struct extent_stats *es = NULL;
        ret = get_volume_stats(pp, lv_name, &es);
        if (ret) {
            fprintf(stderr, "get_extent_stats error\n");
            break;
        }

        // get most used min(100,free_space) extents (read and write multipliers from
        // config file), check if they're on fast or slow storage
        // move hot extents from slow storage, queue the move in move daemon
        // continue if move queued
        for (int tier=0; tier<TIER_MAX; tier++) {
            off_t free_space = get_avaiable_space(pp, lv_name, tier);
            if (free_space < 0) {
                fprintf(stderr, "get_free_space error\n");
                goto no_cleanup;
            }

            if (free_space == 0) {
                if (lower_tiers_exist(pp, lv_name, tier))
                    continue;
                else
                    break;
            }

            // always leave 5 extents worth of free space so that we always
            // can move cold extents from higher tier
            off_t available_extents = free_space / get_extent_size(pp, lv_name) - 5;
            if (available_extents < 0)
              available_extents = 0;

            struct extents *ext = NULL;

            // get next hottest min(100, free_space) extents
            size_t max_extents = 100;
            if (max_extents > available_extents)
                max_extents = available_extents;

            ret = extents_selector(es, &ext, pp, lv_name, tier, max_extents, ES_HOT);
            if (ret) {
                fprintf(stderr, "extents_selector error\n");
                goto no_cleanup;
            }

            if (!ext->length) {
                free_extents(ext);
                continue;
            }

            // move them from slow storage,
            // until no space left
            ret = queue_extents_move(ext, pp, tier);
            if (ret) {
                fprintf(stderr, "Can't queue extents move\n");
                goto no_cleanup;
            }

            free_extents(ext);

            sleep(pp->pvmove_wait);

            // continue main loop, free memory before
            goto cont;
        }

        // pin extents to their current physical volumes, this will cause them
        // to be moved only when the temperature difference is large
        ret = add_pinning_scores(es, pp, lv_name);
        if (ret) {
            fprintf(stderr, "can't add pining scores\n");
            goto no_cleanup;
        }

        // when no space left, get stats for all blocks, add big number (10000) to
        // blocks in fast storage. If there are blocks in slow storage with higher
        // score than ones in fast storage, move 10 worst extents from fast to slow
        // if move queued, continue
        struct extents *prev_tier_max = NULL;
        int prev_tier = -1;
        for (int tier = TIER_MAX; tier >= 0; tier--) {
            off_t free_space = get_avaiable_space(pp, lv_name, tier);
            if (free_space < 0) {
                fprintf(stderr, "get_avaiable_space error\n");
                goto no_cleanup;
            }

            if (!free_space) {
                if (higher_tiers_exist(pp, lv_name, tier))
                  continue;
                else
                  break;
            }

            struct extents *curr_tier_min = NULL;

            if (!prev_tier_max) { // get base line extents
                ret = extents_selector(es, &prev_tier_max, pp, lv_name, tier,
                    5, ES_HOT);
                if (ret) {
                    fprintf(stderr, "extent_selector error\n");
                    goto no_cleanup;
                }
                prev_tier = tier;
                continue;
            }

            ret = extents_selector(es, &curr_tier_min, pp, lv_name, tier, 5, ES_COLD);
            if (ret) {
                fprintf(stderr, "%s:%i: extent_selector error\n", __FILE__, __LINE__);
                goto no_cleanup;
            }

            // check if extents in lower tier aren't hotter
            if (compare_extents(prev_tier_max, curr_tier_min) < 0) {
                float prev_score = get_extent_score(get_extent(prev_tier_max, 0));
                float curr_score = get_extent_score(get_extent(curr_tier_min, 0));
                // don't move more extents that would push very hot extents
                // to low tier or cold extents to higher tier when there is
                // more cold extents to swap than hotter and vice-versa
                int prev_count = count_extents(prev_tier_max, curr_score, ES_COLD);
                int curr_count = count_extents(curr_tier_min, prev_score, ES_HOT);

                int move_extents =
                  (prev_count > curr_count)?curr_count:prev_count;

                truncate_extents(prev_tier_max, move_extents);
                truncate_extents(curr_tier_min, move_extents);

                // queue move of extents that remain
                ret = queue_extents_move(prev_tier_max, pp, tier);
                if (ret) {
                    fprintf(stderr, "%s:%i: queue extents failed\n", __FILE__, __LINE__);
                    goto no_cleanup;
                }
                ret = queue_extents_move(curr_tier_min, pp, prev_tier);
                if (ret) {
                    fprintf(stderr, "%s:%i: queue extents failed\n", __FILE__, __LINE__);
                    goto no_cleanup;
                }
            }

            free_extents(curr_tier_min);
            free_extents(prev_tier_max);
            prev_tier_max = NULL;

            // remember previous tier extents
            ret = extents_selector(es, &prev_tier_max, pp, lv_name, tier, 5, ES_HOT);
            if (ret) {
                fprintf(stderr, "%s:%i: Extent_selector error\n", __FILE__, __LINE__);
                goto no_cleanup;
            }
            prev_tier = tier;
        }

        // wait 10 minutes
        sleep(10*60);

cont:
        free_extent_stats(es);
    }

no_cleanup:

    if (stop)
        return 0;
    else
        return 1;
}

int
main(int argc, char **argv)
{
    int f_ret = EXIT_SUCCESS;
    int ret;

    struct program_params *pp;

    pp = new_program_params();
    if (!pp) {
        fprintf(stderr, "Out of memory\n");
        f_ret = EXIT_FAILURE;
        goto no_cleanup;
    }

    ret = parse_arguments(argc, argv, pp);
    if (ret) {
        f_ret = EXIT_FAILURE;
        goto program_params_cleanup;
    }

    // read config file
    ret = read_config(pp);
    if (ret) {
        f_ret = EXIT_FAILURE;
        goto program_params_cleanup;
    }

    // TODO set signal handlers

    // make sure collector daemon works
    ret = run_collector_daemon(pp);
    if (ret) {
        f_ret = EXIT_FAILURE;
        goto program_params_cleanup;
    }

    // TODO daemonize

    // check how fresh are collected stats
    while(stats_available(pp)) {
        sleep(5*60); // 5 minutes
    }

    // start main loop
    if(main_loop(pp))
      f_ret = EXIT_FAILURE;

program_params_cleanup:
    free_program_params(pp);

no_cleanup:
    return f_ret;
}
