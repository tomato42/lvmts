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
#include "activity_stats.h"

int
main(int argc, char **argv)
{
	int ret = 0;
	int n;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s stats-file.lvmts\n", argv[0]);
		return 1;
	}

	struct activity_stats *as = NULL;

	n = read_activity_stats(&as, argv[1]);
	assert(!n);

	dump_activity_stats(as);

	destroy_activity_stats(as);
	as = NULL;

	return ret;
}
