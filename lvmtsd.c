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
#include "lvmls.h"

int work = 1;

int
main_loop(void)
{
  while (work) {
  // if move daemon is working, wait 5 minutes, start from beginning

  // get most used min(100,free_space) extents (read and write multipliers from
  // config file), check if they're on fast or slow storage
  // move hot extents from slow storage, queue the move in lvmtsmvd
  // continue if move queued

  // get next min(100, free_space) extents, move them from slow storage,
  // until no space left

  // when no space left, get stats for all blocks, add big number (10000) to
  // blocks in fast storage. If there are blocks in slow storage with higher
  // score than ones in fast storage, move 10 worst extents from fast to slow
  // if move queued, continue

  // wait 10 minutes
  }

  if (!work)
    return 0;
  else
    return 1;
}

int
main(int argc, char **argv)
{
    int f_ret = EXIT_SUCCESS;

    // read config file

    // make sure collector daemon works

    // check how fresh are collected stats

    // if stats are not fresh, wait 5 minutes, start from beginning

    // start main loop
    if(main_loop())
      f_ret = EXIT_FAILURE;

    return f_ret;
}
