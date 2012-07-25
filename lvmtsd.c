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
#include "lvmls.h"

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

struct extent_stats {
};

struct extents {
};

static char *
get_first_volume_name(struct program_params *pp)
{
    return NULL;
}

static int
get_volume_stats(struct program_params *pp, char *lv_name, struct extent_stats **es)
{
    return 0;
}

static off_t
get_avaiable_space(struct program_params *pp, char *lv_name, int tier)
{
    return 0;
}

static int
lower_tiers_exist(struct program_params *pp, char *lv_name, int tier)
{
    return 0;
}

/**
 * Select best extents that conform to provided criteria
 *
 * @var es statistics of extent to gather data from
 * @var ret[out] returned list of extents
 * @var pp general program parameters
 * @var lv_name volume name of which extents are to be selected
 * @var max_tier don't select extents from tier higher than this
 * @var max_extents don't return more than this much extents
 */
static int
extents_selector(struct extent_stats *es, struct extents **ret,
    struct program_params *pp, char *lv_name, int max_tier, int max_extents)
{
    return 1;
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
                break;
            }

            if (free_space == 0) {
                if (lower_tiers_exist(pp, lv_name, tier))
                    continue;
                else
                    break;
            }

            struct extents *ext = NULL;
            // 100 - do not select more than 100
            ret = extents_selector(es, &ext, pp, lv_name, tier, 100);
        }


        // get next min(100, free_space) extents, move them from slow storage,
        // until no space left

        // when no space left, get stats for all blocks, add big number (10000) to
        // blocks in fast storage. If there are blocks in slow storage with higher
        // score than ones in fast storage, move 10 worst extents from fast to slow
        // if move queued, continue

        // wait 10 minutes
    }

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
