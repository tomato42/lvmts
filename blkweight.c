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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

/// number of extents in 2TiB LV
#define EXTENTS 524288

#define READ 0x1
#define WRITE 0x2

#define HISTORY_LEN 20

struct extent_info_t {
    long long reads[HISTORY_LEN];
    long long writes[HISTORY_LEN];
};

struct extent_score_t{
    off_t offset;
    long long read_score;
    long long write_score;
};

int add_io(struct extent_info_t *ei, int type)
{
  long long result;
  long long now;
  long long one_hour;
  
  one_hour=3600;

  now = time(NULL);

  if(type == READ) {
      result = now - ei->reads[0];
        {//if(timercmp(&result, &one_hour, >)) {
            memmove(&ei->reads[1], &ei->reads[0], sizeof(struct timeval)*(HISTORY_LEN-1));
            ei->reads[0] = now;
        }
    }

  if(type == WRITE) {
      result = now - ei->writes[0];
        {//if(timercmp(&result, &one_hour, >)) {
            memmove(&ei->writes[1], &ei->writes[0], sizeof(struct timeval)*(HISTORY_LEN-1));
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
      score -= (now_time - extent_time) / 3600;
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
      score -= (now_time - extent_time) / 3600;
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

int main(int argc, char **argv)
{
    struct extent_info_t *extent_info;

    extent_info = calloc(sizeof(struct extent_info_t), EXTENTS);
    if (!extent_info) {
        fprintf(stderr, "can't allocate memory!\n");
        exit(1);
    }

    printf("sizeof: %li\n", sizeof(struct extent_info_t));

    char type[10];
    float begin, end;
    int ret;
    while (1){
        ret = fscanf(stdin, "%s %f %f\n", type, &begin, &end);
        if (ret == EOF)
            goto clean_up;
        
        if (type[0] == 'R') {
//            printf("read: %lu\n", (size_t)begin);
            add_io(&extent_info[(size_t)begin], READ);
        }

        if (type[0] == 'W') {
//            printf("write: %lu\n", (size_t)begin);
            add_io(&extent_info[(size_t)begin], WRITE);
        }
    }

clean_up:
    printf("individual extent score:\n");
    for(size_t i=0; i<EXTENTS; i++)
      if(extent_info[i].reads[0] || extent_info[i].writes[0]) {
          printf("%lu: r: %u, w:%u\n", i, get_read_score(&extent_info[i]), get_write_score(&extent_info[i]));
//          print_io(&extent_info[i]);
      }

    struct extent_score_t *extent_score;

    extent_score = convert_extent_info_to_extent_score(extent_info);

    qsort(extent_score, EXTENTS, sizeof(struct extent_score_t), extent_cmp);
    
    printf("\n\n");
    printf("200 most active extents: (from least to most)\n");
    for(int i=EXTENTS-200; i<EXTENTS; i++)
        printf("%lu:", extent_score[i].offset);
    printf("\n\n");

    qsort(extent_score, EXTENTS, sizeof(struct extent_score_t), extent_read_cmp);

    printf("200 most read extents (from least to most):\n");
    for(int i=EXTENTS-200; i<EXTENTS; i++)
        printf("%lu:", extent_score[i].offset);
    printf("\n\n");

    qsort(extent_score, EXTENTS, sizeof(struct extent_score_t), extent_write_cmp);
    printf("200 most write extents (from least to most):\n");
    for(int i=EXTENTS-200; i<EXTENTS; i++)
        printf("%lu:", extent_score[i].offset);

    free(extent_score);
    free(extent_info);
    return 0;
}
