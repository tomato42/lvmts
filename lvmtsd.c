/** blkweight - parse output of blkparse and convert to LVM extents 
 * 
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
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

/// number of extents in 2TiB LV
//#define EXTENTS 524288
#define EXTENTS extents

uint64_t extents=1024;

// granularity of samples (5 minutes)
#define GRANULARITY 300

#define READ 0x1
#define WRITE 0x2

#define HISTORY_LEN 20

struct extent_info_t {
    uint64_t reads[HISTORY_LEN];
    uint64_t writes[HISTORY_LEN];
};

struct extent_score_t{
    off_t offset;
    long long read_score;
    long long write_score;
};

int add_io(struct extent_info_t *ei, uint64_t time, int type)
{
  long long result;
  long long now;

  now = time;

  if(type == READ) {
      result = now - ei->reads[0];
        if(result > GRANULARITY) {
            memmove(&ei->reads[1], &ei->reads[0], sizeof(uint64_t)*(HISTORY_LEN-1));
            ei->reads[0] = now;
        }
    }

  if(type == WRITE) {
      result = now - ei->writes[0];
        if(result > GRANULARITY) {
            memmove(&ei->writes[1], &ei->writes[0], sizeof(uint64_t)*(HISTORY_LEN-1));
            ei->writes[0] = now;
        }
    }

  return 0;
}

void print_io(struct extent_info_t *ei) {
    for(size_t i=0; i<HISTORY_LEN; i++) {
        if(ei->reads[i]){
            printf("r: %lli, ", (long long)ei->reads[i]);
        }
        if(ei->writes[i]){
            printf("w: %lli, ", (long long)ei->writes[i]);
        }
    }
    printf("\n");
}

unsigned int get_read_score(struct extent_info_t *ei) {
  
  unsigned int score=1<<30;
  struct timeval now;
  gettimeofday(&now, NULL);
  long long extent_time;
  long long now_time = now.tv_sec;

  if(!ei->reads[0])
    return 0;
  
  for(int i=0; i<HISTORY_LEN; i++){
      if(!ei->reads[i]){
          score -= (1<<30)/HISTORY_LEN;
          continue;
      }
      extent_time = ei->reads[i];
      score -= (now_time - extent_time) / GRANULARITY;
  }

  return score;
}

unsigned int get_write_score(struct extent_info_t *ei) {
  
  unsigned int score=1<<30;
  struct timeval now;
  gettimeofday(&now, NULL);
  long long extent_time;
  long long now_time = now.tv_sec;

  if(!ei->writes[0])
    return 0;
  
  for(int i=0; i<HISTORY_LEN; i++){
      if(!ei->writes[i]) {
          score -= (1<<30)/HISTORY_LEN;
          continue;
      }
      extent_time = ei->writes[i];
      score -= (now_time - extent_time) / GRANULARITY;
  }

  return score;
}

int extent_cmp(const void *a, const void *b)
{
  struct extent_score_t *ext_a, *ext_b;
  long a_score=0, b_score=0;

  ext_a = (struct extent_score_t *)a;
  ext_b = (struct extent_score_t *)b;

  a_score = ext_a->read_score + ext_a->write_score;
  b_score = ext_b->read_score + ext_b->write_score;

  if(a_score < b_score)
      return -1;
  else if (a_score == b_score)
      return 0;
  else
      return 1;
}

int extent_read_cmp(const void *a, const void *b){
  struct extent_score_t *ext_a, *ext_b;
  long a_score=0, b_score=0;

  ext_a = (struct extent_score_t *)a;
  ext_b = (struct extent_score_t *)b;

  a_score = ext_a->read_score;
  b_score = ext_b->read_score;

  if(a_score < b_score)
      return -1;
  else if (a_score == b_score)
      return 0;
  else
      return 1;
}

int extent_write_cmp(const void *a, const void *b){
  struct extent_score_t *ext_a, *ext_b;
  long a_score=0, b_score=0;

  ext_a = (struct extent_score_t *)a;
  ext_b = (struct extent_score_t *)b;

  a_score = ext_a->write_score;
  b_score = ext_b->write_score;

  if(a_score < b_score)
      return -1;
  else if (a_score == b_score)
      return 0;
  else
      return 1;
}

struct extent_score_t* convert_extent_info_to_extent_score(struct extent_info_t *ei){
    struct extent_score_t *es;
    es = calloc(sizeof(struct extent_score_t), EXTENTS);
    if(!es){
        fprintf(stderr, "can't allocate memory\n");
        exit(1);
    }
    
    for(off_t i=0; i<EXTENTS; i++){
        es[i].offset = i;
        es[i].read_score = get_read_score(&ei[i]);
        es[i].write_score = get_write_score(&ei[i]);
    }
    return es;
}

void print_extents(struct extent_info_t *extent_info, int ext_to_print, uint64_t start_extent)
{
    struct extent_score_t *extent_score;
    
    extent_score = convert_extent_info_to_extent_score(extent_info);

    qsort(extent_score, EXTENTS, sizeof(struct extent_score_t), extent_cmp);
    

    time_t res;

    res = time(NULL);
    
    printf("\n\n");
    printf("%s", asctime(localtime(&res)));
    printf("%i most active physical extents: (from least to most)\n", ext_to_print);
    for(int i=EXTENTS-ext_to_print; i<EXTENTS; i++)
        if(i%10==9 || i==EXTENTS-1)
            printf("%lu\n", extent_score[i].offset + start_extent);
        else
            printf("%lu:", extent_score[i].offset + start_extent);
    printf("\n\n");

    qsort(extent_score, EXTENTS, sizeof(struct extent_score_t), extent_read_cmp);

    printf("%i most read extents (from least to most):\n", ext_to_print);
    for(int i=EXTENTS-ext_to_print; i<EXTENTS; i++)
        if(i%10==9 || i==EXTENTS-1)
            printf("%lu\n", extent_score[i].offset + start_extent);
        else
            printf("%lu:", extent_score[i].offset + start_extent);
    printf("\n\n");

    qsort(extent_score, EXTENTS, sizeof(struct extent_score_t), extent_write_cmp);
    printf("%i most write extents (from least to most):\n", ext_to_print);
    for(int i=EXTENTS-ext_to_print; i<EXTENTS; i++)
        if(i%10==9 || i==EXTENTS-1)
            printf("%lu\n", extent_score[i].offset + start_extent);
        else
            printf("%lu:", extent_score[i].offset + start_extent);

    free(extent_score);
}

struct extent_info_t* read_stdin(uint64_t start_time, struct extent_info_t *extent_info)
{
    char in_buf[8192]={0};
    char blk_dev_num[4096]={0};
    int cpu_id=0;
    uint64_t seq_num=0;
    double time_stamp=0;
    int proc_id=0;
    char action_id[8]={0};
    char rwbs[8]={0};
    uint64_t offset=0;
    char plus_sgn[8]={0};
    uint64_t len=0;
    char err_val[16];
    // number of sectors in extent
    uint64_t sec_in_ext = 4 * 1024 * 1024 / 512; 

    // offset in extents
    uint64_t extent_num=0;
    int r;

    uint64_t last_print = 0;

    while (fgets(in_buf, 8191, stdin)){
        r = sscanf(in_buf, 
            "%4095s %100i %" SCNu64 " %64lf %64i %7s %7s %" SCNu64 " %4s "
            "%" SCNu64 " %15s",
            blk_dev_num, &cpu_id, &seq_num, &time_stamp, &proc_id,
            action_id, rwbs, &offset, plus_sgn, &len, err_val);

        // ignore all non Complete events 
        if (strcmp(action_id,"C"))
            continue;

        // print current stats every 5 minutes
        if (last_print+60*5<time_stamp) {
            print_extents(extent_info, 100, 0);
            last_print = time_stamp;
        }

        // round up
        extent_num=(offset+(sec_in_ext-1))/sec_in_ext;

        if (extents<=extent_num) {
            extent_info = realloc(extent_info, 
                sizeof(struct extent_info_t)*(extent_num+100));
            if (!extent_info){
                fprintf(stderr, "out of memory\n");
                exit(1);
            }

            memset(&extent_info[extents], 0, 
                (extent_num+100-extents)*sizeof(struct extent_info_t));

            extents=extent_num+100;
        }

        if (rwbs[0] == 'R') 
            add_io(&extent_info[(size_t)extent_num],
                start_time + time_stamp, READ);

        if (rwbs[0] == 'W') 
            add_io(&extent_info[(size_t)extent_num],
                start_time + time_stamp, WRITE);
    }

    return extent_info;
}

int main(int argc, char **argv)
{
    struct extent_info_t *extent_info;

    uint64_t start_extent = 0;
    int ext_to_print = 200;
    
    if(argc>1)
        start_extent = atoi(argv[1]);

    if(argc>2)
        ext_to_print = atoi(argv[2]);

    extent_info = calloc(sizeof(struct extent_info_t), EXTENTS);
    if (!extent_info) {
        fprintf(stderr, "can't allocate memory!\n");
        exit(1);
    }

    printf("sizeof: %li\n", sizeof(struct extent_info_t));

    uint64_t now = time(NULL);

    extent_info = read_stdin(now, extent_info);

    printf("individual extent score:\n");
    for(size_t i=0; i<EXTENTS; i++)
      if(extent_info[i].reads[0] || extent_info[i].writes[0]) {
          printf("%lu: r: %u, w:%u\n", i, get_read_score(&extent_info[i]), get_write_score(&extent_info[i]));
//          print_io(&extent_info[i]);
      }

    print_extents(extent_info, ext_to_print, start_extent);

    free(extent_info);
    return 0;
}
