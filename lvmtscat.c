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
#include <getopt.h>
#include "activity_stats.h"
#include "lvmls.h"

int blocks = 100;
int read_mult = 1;
int write_mult = 10;
float max_score;
int get_max = 0;
char *file = NULL;
int pvmove_output = 0;
int print_le = 0;
char *lv_name = NULL;
char *vg_name = NULL;

void
usage(void)
{
  printf("Usage: lvmtscat [options] [--LE|--VG VolumeGroupName --LV LogicalVolumeName] StatsFile\n");
  printf("\n");
  printf(" -b,--blocks            Number of blocks to print\n");
  printf(" -r,--read-multiplier   Read score multiplier\n");
  printf(" -w,--write-multiplier  Write score multiplier\n");
  printf(" -m,--max-score         Don't print blocks with score higher than that\n");
  printf(" --pvmove               Use pvmove-compatible output\n");
  printf(" --LE                   Print logical extents, not physical extents\n");
  printf(" --LV                   Name of logical volume\n");
  printf(" --VG                   Name of volume group\n");
  printf(" -?,--help              This message\n");
}

int
parse_arguments(int argc, char **argv)
{
  int c;
  int f_ret = 0;

  struct option long_options[] = {
			  {"blocks",           required_argument, 0, 'b' }, // 0
			  {"read-multiplier",  required_argument, 0, 'r' }, // 1
			  {"write-multiplier", required_argument, 0, 'w' }, // 2
			  {"max-score",        required_argument, 0, 'm' }, // 3
			  {"help",             no_argument,       0, '?' }, // 4
              {"pvmove",           no_argument,       0, 0 }, // 5
              {"LE",               no_argument,       0, 0 }, // 6
              {"LV",               required_argument, 0, 0 }, // 7
              {"VG",               required_argument, 0, 0 }, // 8
			  {0, 0, 0, 0}
  };

  int64_t tmp_lint;
  float a;

  while(1) {
    int option_index = 0;

    c = getopt_long(argc, argv, "b:r:w:m:?", long_options, &option_index);

    if (c == -1)
      break;

    switch(c) {
      case 0: /* long options */
        switch(option_index) {
          case 5:
            pvmove_output = 1;
            break;
          case 6:
            print_le = 1;
            break;
          case 7:
            lv_name = optarg;
            break;
          case 8:
            vg_name = optarg;
            break;
        }
	break;
      case 'b':
	tmp_lint = atoll(optarg);
	if (tmp_lint <= 0) {
          fprintf(stderr, "Number of blocks must be larger than zero!\n");
	  f_ret = 1;
	  break;
	}
	blocks = tmp_lint;
	break;
      case 'r':
	tmp_lint = atoll(optarg);
	if (tmp_lint < 0) {
	  fprintf(stderr, "Read multiplier can't be negative!\n");
	  f_ret = 1;
	  break;
	}
	read_mult = tmp_lint;
	break;
      case 'w':
	tmp_lint = atoll(optarg);
	if (tmp_lint < 0) {
		fprintf(stderr, "Write multiplier can't be negative!\n");
		f_ret = 1;
		break;
	}
	write_mult = tmp_lint;
	break;
      case 'm':
	a = atof(optarg);
	if (a <= 0) {
		fprintf(stderr, "Max score must be larger than zero!\n");
		f_ret = 1;
		break;
	}
	get_max = 1;
	max_score = a;
	break;
      case '?':
	// TODO help
	break;
      default:
	fprintf(stderr, "Unknown option: %c\n", c);
	break;
    }
  }

  if (f_ret == 0)
    file = argv[optind];

  return f_ret;
}

int
main(int argc, char **argv)
{
	int ret = 0;
	int n;
    double mean_lifetime = 3 * 24 * 60 * 60;

	if (parse_arguments(argc, argv))
		return 1;

	struct activity_stats *as = NULL;

	if (file == NULL) {
      fprintf(stderr, "No file name provided\n");
	  usage();
	  return 1;
	}

    if (!print_le && (!lv_name || !vg_name)) {
        fprintf(stderr, "You must ask for logical extents or provide volume"
            " group and logical volume name.\n");
        usage();
        return 1;
    }

    struct program_params pp = { .lvm2_handle = NULL };

    if (!print_le) {
        init_le_to_pe(&pp);
    }

	n = read_activity_stats(&as, file);
	assert(!n);

	struct block_scores *bs = NULL;

	if(get_max)
	   get_best_blocks_with_max_score(as, &bs, blocks, read_mult,
			   write_mult, mean_lifetime, max_score);
	else
	   get_best_blocks(as, &bs, blocks, read_mult, write_mult, mean_lifetime);

    if (pvmove_output) {
        if (print_le) {
            printf("%li", bs[0].offset);
            for (int i=1; i< blocks; i++) {
                printf(":%li", bs[i].offset);
            }
            printf("\n");
        } else {
            struct pv_info *pvi;
            struct pv_info *pvn;
            pvi = LE_to_PE(vg_name, lv_name, bs[0].offset);
            printf("%s:%li", pvi->pv_name, pvi->start_seg);
            for (int i=1; i<blocks; i++) {
                pvn = LE_to_PE(vg_name, lv_name, bs[i].offset);
                if (!pvn)
                  continue;
                if (!strcmp(pvi->pv_name, pvn->pv_name)) {
                    printf(":%li", pvn->start_seg);
                    pv_info_free(pvn);
                    continue;
                } else {
                    printf("\n%s:%li", pvn->pv_name, pvn->start_seg);
                    pv_info_free(pvi);
                    pvi = pvn;
                    continue;
                }
            }
            printf("\n");
            pv_info_free(pvi);
        }
    } else {
        if (!print_le) {
            fprintf(stderr, "Unsupported combination of parameters"
                " (add --pvmove or --LE)\n");
        } else
	        print_block_scores(bs, blocks);
    }

	free(bs);
	bs = NULL;

	//dump_activity_stats(as);

	destroy_activity_stats(as);
	as = NULL;

    if (!print_le) {
        le_to_pe_exit(&pp);
    }

	return ret;
}
