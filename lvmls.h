/*
 * Copyright (C) 2011 Hubert Kario <kario@wsisiz.edu.pl>
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
#ifndef _lvmls_h_
#define _lvmls_h_
#include <stdint.h>
#include "config.h"

void init_le_to_pe(struct program_params *pp);

void le_to_pe_exit(struct program_params *pp);

struct pv_info {
    char *pv_name;
    uint64_t start_seg;
};

struct le_info {
    uint64_t le;
    uint64_t pe;
    const char *dev;
    const char *lv_name;
};

void pv_info_free(struct pv_info *pv);

// convert logical extent from logical volume specified by lv_name,
// vg_name and logical extent number (le_num) to physical extent
// on specific device
struct pv_info *LE_to_PE(const char *vg_name, const char *lv_name, uint64_t le_num);

uint64_t get_pe_size(const char *vg_name);

/**
 * Returns amount of free extents on pv in vg
 */
uint64_t get_free_extent_number(const char *vg_name, const char *pv_name);


struct le_info get_first_LE_info(const char *vg_name, const char *lv_name,
    const char *pv_name);

/**
 * Return type of allocation for provided PE
 */
struct le_info get_PE_allocation(const char *vg_name, const char *pv_name,
    uint64_t pe_num);

/**
 * Returns number of extents used by lv on specified pv in vg
 */
uint64_t get_used_space_on_pv(const char *vg_name, const char *lv_name,
    const char *pv_name);

#endif /* ifndef _lvmls_h */

