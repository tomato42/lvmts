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
#include <lvm2cmd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvmls.h"

// information about continuous extent allocations
struct pv_allocations {
    char *pv_name;
    char *vg_name;
    char *vg_format; // not used
    char *vg_attr; // not used
    char *lv_name;
    char *pv_type; // type of allocation ("free", "linear", "striped")
    uint64_t pv_start; // starting extent in PV
    uint64_t pv_length;
    uint64_t lv_start; // starting extent of this segment in LV
};

struct pv_allocations *pv_segments=NULL;
size_t pv_segments_num=0;

// helper function to qsort, used to sort phisical extents
int _compare_segments(const void *a, const void *b)
{
    struct pv_allocations *alloc_a, *alloc_b;
    int r;

    alloc_a = (struct pv_allocations *)a;
    alloc_b = (struct pv_allocations *)b;

    r = strcmp(alloc_a->vg_name, alloc_b->vg_name);
    if(r != 0)
        return r;

    r = strcmp(alloc_a->lv_name, alloc_b->lv_name);
    if(r != 0)
        return r;

    if (alloc_a->lv_start == alloc_b->lv_start)
        return 0;
    else if (alloc_a->lv_start < alloc_b->lv_start)
        return -1;
    else
        return 1;
}

void sort_segments(struct pv_allocations *segments, size_t nmemb)
{
    qsort(segments, nmemb, sizeof(struct pv_allocations), _compare_segments);
}

// add information about physical segment to global variable pv_segments
void add_segment(char *pv_name, char *vg_name, char *vg_format, char *vg_attr,
    char *lv_name, char *pv_type, uint64_t pv_start, uint64_t pv_length,
    uint64_t lv_start)
{
    if(pv_segments_num==0)
        pv_segments = calloc(sizeof(struct pv_allocations), 1);

    if(!pv_segments)
        goto segment_failure;

#define str_copy_alloc(N, X) pv_segments[(N)].X = malloc(strlen(X)+1);  \
    if(!pv_segments[(N)].X)                                             \
        goto segment_failure;                                           \
    strcpy(pv_segments[(N)].X, X);

    if (pv_segments_num==0) {
        str_copy_alloc(0, pv_name);
        str_copy_alloc(0, vg_name);
        str_copy_alloc(0, vg_format);
        str_copy_alloc(0, vg_attr);
        str_copy_alloc(0, lv_name);
        str_copy_alloc(0, pv_type);

        pv_segments[0].pv_start = pv_start;
        pv_segments[0].pv_length = pv_length;
        pv_segments[0].lv_start = lv_start;
        pv_segments_num=1;
        return;
    }

    pv_segments = realloc(pv_segments,
        sizeof(struct pv_allocations)*(pv_segments_num+1));
    if (!pv_segments)
      goto segment_failure;

    str_copy_alloc(pv_segments_num, pv_name);
    str_copy_alloc(pv_segments_num, vg_name);
    str_copy_alloc(pv_segments_num, vg_format);
    str_copy_alloc(pv_segments_num, vg_attr);
    str_copy_alloc(pv_segments_num, lv_name);
    str_copy_alloc(pv_segments_num, pv_type);

    pv_segments[pv_segments_num].pv_start = pv_start;
    pv_segments[pv_segments_num].pv_length = pv_length;
    pv_segments[pv_segments_num].lv_start = lv_start;
    pv_segments_num+=1;

    return;

segment_failure:
    fprintf(stderr, "Out of memory\n");
    exit(1);

#undef str_copy_alloc
}

// helper funcion for lvm2cmd, used to parse mappings between
// logical extents and physical extents
void parse_pvs_segments(int level, const char *file, int line,
                 int dm_errno, const char *format)
{
    // disregard debug output
    if (level != 4)
      return;

    char pv_name[4096]={0}, vg_name[4096]={0}, vg_format[4096]={0},
         vg_attr[4096]={0}, pv_size[4096]={0}, pv_free[4096]={0},
         lv_name[4096]={0}, pv_type[4096]={0};
    int pv_start=0, pv_length=0, lv_start=0;
    int r;

    // try to match standard format (allocated extents)
    r = sscanf(format, " %4095s %4095s %4095s %4095s %4095s "
        "%4095s %40u %40u %4095s %40d %4095s ",
        pv_name, vg_name, vg_format, vg_attr, pv_size, pv_free,
        &pv_start, &pv_length, lv_name, &lv_start, pv_type);

    if (r==EOF) {
        fprintf(stderr, "Error matching line %i: %s\n", line, format);
        goto parse_error;
    } else if (r != 11) {
        // segments with state "free" require matching format without "lv_name"
        lv_name[0]='\000';

        r = sscanf(format, " %4095s %4095s %4095s %4095s %4095s %4095s "
            "%40u %40u %40d %4095s ",
            pv_name, vg_name, vg_format, vg_attr, pv_size, pv_free,
            &pv_start, &pv_length, &lv_start, pv_type);

        if (r==EOF || r != 10) {
            fprintf(stderr, "Error matching line %i: %s\n", line, format);
            goto parse_error;
        }
    } else { // r == 11
        // do nothing, correct parse
    }

    add_segment(pv_name, vg_name, vg_format, vg_attr, lv_name, pv_type,
        pv_start, pv_length, lv_start);

    // DEBUG
//    printf("matched %i fields:", r);

//    printf("%s,%s,%s,%s,%s,%s,%i,%i,%s,%i,%s\n",
//        pv_name, vg_name, vg_format, vg_attr, pv_size, pv_free,
//        pv_start, pv_length, lv_name, lv_start, pv_type);

    // DEBUG
    //printf("%s\n", format);
parse_error:
    return;
}

// convert logical extent from logical volume specified by lv_name,
// vg_name and logical extent number (le_num) to physical extent
// on specific device
struct pv_info *LE_to_PE(char *vg_name, char *lv_name, uint64_t le_num)
{
    for(size_t i=0; i < pv_segments_num; i++) { // TODO use binary search
        if(!strcmp(pv_segments[i].vg_name, vg_name) &&
            !strcmp(pv_segments[i].lv_name, lv_name)) {

            if (le_num >= pv_segments[i].lv_start &&
                le_num < pv_segments[i].lv_start+pv_segments[i].pv_length) {

                struct pv_info *pv_info;

                pv_info = malloc(sizeof(struct pv_info));
                if (!pv_info) {
                    fprintf(stderr, "Out of memory\n");
                    exit(1);
                }

                pv_info->pv_name = malloc(sizeof(char)
                    *(strlen(pv_segments[i].pv_name)+1));
                if (!pv_info->pv_name) {
                    fprintf(stderr, "Out of memory\n");
                    exit(1);
                }

                strcpy(pv_info->pv_name,pv_segments[i].pv_name);

                pv_info->start_seg = pv_segments[i].pv_start +
                  (le_num - pv_segments[i].lv_start);

                return pv_info;
            }
        }
    }
    return NULL;
}

struct vg_pe_sizes {
    char *vg_name;
    uint64_t pe_size;
};

struct vg_pe_sizes *vg_pe_sizes;
size_t vg_pe_sizes_len;

// parse output from lvm2cmd about extent sizes
void parse_vgs_pe_size(int level, const char *file, int line,
                 int dm_errno, const char *format)
{
    // disregard debug output
    if (level != 4)
      return;

    char vg_name[4096], pe_size[4096];
    uint64_t pe_size_bytes=0;
    int r;

    r = sscanf(format, " %4095s %4095s ", vg_name, pe_size);
    if (r == EOF || r != 2) {
        fprintf(stderr, "%s:%i Error parsing line %i: %s\n",
            __FILE__, __LINE__, line, format);
        return;
    }

    double temp;
    char *tail;

    temp = strtod(pe_size, &tail);
    if (temp == 0.0) {
        fprintf(stderr, "%s:%i Error parsing line %i: %s\n",
            __FILE__, __LINE__, line, format);
        return;
    }

    switch(tail[0]){
	    case 'b':
	    case 'B':
	        pe_size_bytes = temp;
	        break;
	    case 'S':
	        pe_size_bytes = temp * 512;
            break;
        case 'k':
	        pe_size_bytes = temp * 1024;
	        break;
        case 'K':
            pe_size_bytes = temp * 1000;
            break;
        case 'm':
            pe_size_bytes = temp * 1024 * 1024;
	        break;
        case 'M':
	        pe_size_bytes = temp * 1000 * 1000;
            break;
        case 'g':
            pe_size_bytes = temp * 1024 * 1024 * 1024;
            break;
        case 'G':
            pe_size_bytes = temp * 1000 * 1000 * 1000;
            break;
        case 't':
            pe_size_bytes = temp * 1024 * 1024 * 1024 * 1024;
            break;
        case 'T':
            pe_size_bytes = temp * 1000 * 1000 * 1000 * 1000;
            break;
        case 'p':
            pe_size_bytes = temp * 1024 * 1024 * 1024 * 1024 * 1024;
            break;
        case 'P':
            pe_size_bytes = temp * 1000 * 1000 * 1000 * 1000 * 1000;
            break;
        case 'e':
            pe_size_bytes = temp * 1024 * 1024 * 1024 * 1024 * 1024 * 1024;
            break;
        case 'E':
            pe_size_bytes = temp * 1000 * 1000 * 1000 * 1000 * 1000 * 1000;
            break;
        default:
            pe_size_bytes = temp;
            /* break; */
    }

    // save info about first volume group
    if (vg_pe_sizes_len == 0) {
        vg_pe_sizes = malloc(sizeof(struct vg_pe_sizes));
        if (!vg_pe_sizes)
            goto vgs_failure;

        vg_pe_sizes[0].vg_name = malloc(strlen(vg_name)+1);
        if (!vg_pe_sizes[0].vg_name)
            goto vgs_failure;

        strcpy(vg_pe_sizes[0].vg_name, vg_name);
        vg_pe_sizes[0].pe_size = pe_size_bytes;

        vg_pe_sizes_len=1;

        return;
    }

    // save info about subsequent groups
    vg_pe_sizes = realloc(vg_pe_sizes, sizeof(struct vg_pe_sizes)*
        (vg_pe_sizes_len+1));
    if (!vg_pe_sizes)
        goto vgs_failure;

    vg_pe_sizes[vg_pe_sizes_len].vg_name = malloc(strlen(vg_name)+1);
    if(!vg_pe_sizes[vg_pe_sizes_len].vg_name)
        goto vgs_failure;
    strcpy(vg_pe_sizes[vg_pe_sizes_len].vg_name, vg_name);
    vg_pe_sizes[vg_pe_sizes_len].pe_size = pe_size_bytes;

    vg_pe_sizes_len+=1;

    return;

vgs_failure:
    fprintf(stderr, "Out of memory\n");
    exit(1);
}

// return size of extents in provided volume group
uint64_t get_pe_size(char *vg_name)
{
    for(size_t i=0; i<vg_pe_sizes_len; i++)
        if (!strcmp(vg_pe_sizes[i].vg_name, vg_name))
            return vg_pe_sizes[i].pe_size;

    return 0;
}

// free allocated memory and objects
void le_to_pe_exit()
{
    for(size_t i=0; i<pv_segments_num; i++){
        free(pv_segments[i].pv_name);
        free(pv_segments[i].vg_name);
        free(pv_segments[i].vg_format);
        free(pv_segments[i].vg_attr);
        free(pv_segments[i].lv_name);
        free(pv_segments[i].pv_type);
    }
    free(pv_segments);
    pv_segments = NULL;
    pv_segments_num = 0;

    for(size_t i=0; i<vg_pe_sizes_len; i++)
        free(vg_pe_sizes[i].vg_name);

    free(vg_pe_sizes);
    vg_pe_sizes = NULL;
    vg_pe_sizes_len = 0;
}

// initialize or reload cache variables
void init_le_to_pe()
{
    void *handle;
    int r;

    if(pv_segments)
        le_to_pe_exit();

    vg_pe_sizes = NULL;
    vg_pe_sizes_len = 0;

    lvm2_log_fn(parse_pvs_segments);

    handle = lvm2_init();

    lvm2_log_level(handle, 1);
    r = lvm2_run(handle, "pvs --noheadings --segments -o+lv_name,"
        "seg_start_pe,segtype --units=b");

//    if (r)
//      fprintf(stderr, "command failed\n");

    sort_segments(pv_segments, pv_segments_num);

    lvm2_log_fn(parse_vgs_pe_size);

    r = lvm2_run(handle, "vgs -o vg_name,vg_extent_size --noheadings --units=b");

    lvm2_exit(handle);

    return;
}

// return number of free extents in PV in specified volume group
// or in whole volume group if pv_name is NULL
uint64_t get_free_extent_number(char *vg_name, char *pv_name)
{
    if (!vg_name)
        return 0;

    uint64_t sum=0;

    if(pv_name)
        for(size_t i=0; i < pv_segments_num; i++) {
            if (!strcmp(pv_segments[i].vg_name, vg_name) &&
                !strcmp(pv_segments[i].pv_name, pv_name) &&
                !strcmp(pv_segments[i].pv_type, "free"))
              sum+=pv_segments[i].pv_length;
        }
    else
        for(size_t i=0; i < pv_segments_num; i++)
            if (!strcmp(pv_segments[i].vg_name, vg_name) &&
                !strcmp(pv_segments[i].pv_type, "free"))
              sum+=pv_segments[i].pv_length;

    return sum;
}

#ifdef STANDALONE
int main(int argc, char **argv)
{
    init_le_to_pe();

    if (argc <= 1)
      return 1;

    for(int i=0; i < pv_segments_num; i++)
        if(!strcmp(pv_segments[i].lv_name, argv[1]))
            printf("%s %li-%li (%li-%li)\n", pv_segments[i].pv_name,
                pv_segments[i].pv_start,
                pv_segments[i].pv_start+pv_segments[i].pv_length,
                pv_segments[i].lv_start,
                pv_segments[i].lv_start+pv_segments[i].pv_length);

    if (argc <= 2)
        return 0;
    struct pv_info *pv_info;
    pv_info = LE_to_PE("laptom", argv[1], atoi(argv[2]));
    if (pv_info)
        printf("LE no %i of laptom-%s is at: %s:%li\n", atoi(argv[2]), argv[1],
            pv_info->pv_name, pv_info->start_seg);
    else
        printf("no LE found\n");

    printf("vg: laptom, extent size: %lu bytes\n", get_pe_size("laptom"));

    le_to_pe_exit();

    return 0;
}
#endif
