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
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <lvm2cmd.h>
#include "lvmls.h"

int
main(int argc, char **argv)
{
    if (argc != 4) {
        fprintf(stderr, "Tool to defragment LogicalVolume on selected PhysicalVolume\n");
        fprintf(stderr, "Usage: lvmdefrag VolumeGroup LogicalVolume PhysicalVolume\n");
        return EXIT_FAILURE;
    }

    char *vg_name = argv[1];
    char *lv_name = argv[2];
    char *pv_name = argv[3];

    struct program_params pp = { .lvm2_handle = NULL };
    init_le_to_pe(&pp);

    struct le_info first_le;

    first_le = get_first_LE_info(vg_name, lv_name, pv_name);

    for(size_t i=0; i < pv_segments_num; i ++)
        if( !strcmp(pv_segments[i].lv_name, lv_name) &&
            !strcmp(pv_segments[i].vg_name, vg_name) &&
            !strcmp(pv_segments[i].pv_name, pv_name)) {

            long int optimal_pos;
            optimal_pos = first_le.pe + pv_segments[i].lv_start - first_le.le;

            if (pv_segments[i].pv_start == optimal_pos)
                  continue;

            struct le_info optimal;
            long int move_extent = 0;

            for (long int j=optimal_pos;
                 j < optimal_pos + pv_segments[i].pv_length;
                 j++) {

                optimal = get_PE_allocation(vg_name, pv_name, j);

                if (optimal.dev == NULL) {
                    printf("# Optimal position for LE %li-%li on %s is after end of "
                        "the device\n",
                        pv_segments[i].lv_start,
                        pv_segments[i].lv_start + pv_segments[i].pv_length,
                        pv_name);
                    break;
                } else if (strcmp(optimal.lv_name, "free")) {
                    printf("# Optimal position for LE %li-%li is used by %s LE %li\n",
                        pv_segments[i].lv_start,
                        pv_segments[i].lv_start + pv_segments[i].pv_length,
                        optimal.lv_name, optimal.le);
                    break;
                }

                move_extent++;
            }

            if (move_extent) {
                  printf("pvmove -i1 --alloc anywhere %s:%li-%li %s:%li-%li # LE %li (size: %li)\n",
                      pv_name,
                      pv_segments[i].pv_start,
                      pv_segments[i].pv_start + move_extent - 1,
                      pv_name,
                      optimal_pos,
                      optimal_pos + move_extent - 1,
                      pv_segments[i].lv_start,
                      move_extent);
            }
        }

    le_to_pe_exit(&pp);
    lvm2_exit(pp.lvm2_handle);

    return EXIT_SUCCESS;
}
