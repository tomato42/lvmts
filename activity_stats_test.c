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
#include <check.h>
#include <stdio.h>
#include <stdlib.h>
#include "activity_stats.c"

START_TEST(add_block_test)
{
  size_t size = 10;
  size_t elems = 0;
  struct block_scores *bs;
  bs = malloc(sizeof(struct block_scores) * size);

  fail_unless(bs != NULL);

  struct block_scores block;
  block.offset = 1;
  block.score = 2.5;

  add_score_to_block_scores(bs, elems, &block);
  elems++;

  fail_unless(bs[0].offset == 1);
  fail_unless(bs[0].score == 2.5);

  block.offset = 4;
  block.score = 5;

  add_score_to_block_scores(bs, elems, &block);
  elems++;

  fail_unless(bs[0].offset == 4);
  fail_unless(bs[0].score == 5);

  block.offset = 2;
  block.score = 4;

  add_score_to_block_scores(bs, elems, &block);
  elems++;

  fail_unless(bs[0].offset == 4);
  fail_unless(bs[0].score == 5);
  fail_unless(bs[1].offset == 2);
  fail_unless(bs[1].score == 4);

  free(bs);
}
END_TEST

// add blocks up to the size of allocated array
// last added block is at the end of array
START_TEST(add_block_full_test)
{
  size_t size = 10;
  size_t elems = 0;
  struct block_scores *bs;
  bs = malloc(sizeof(struct block_scores) * size);

  fail_unless(bs != NULL);

  struct block_scores block;

  block.offset = 1;
  block.score = 2.5;
  add_score_to_block_scores(bs, elems, &block);
  elems++;

  block.offset = 4;
  block.score = 5;
  add_score_to_block_scores(bs, elems, &block);
  elems++;

  block.offset = 2;
  block.score = 4;
  add_score_to_block_scores(bs, elems, &block);
  elems++;

  block.offset = 3;
  block.score = 1.5;
  add_score_to_block_scores(bs, elems, &block);
  elems++;

  block.offset = 5;
  block.score = 1.3;
  add_score_to_block_scores(bs, elems, &block);
  elems++;

  block.offset = 6;
  block.score = 1.2;
  add_score_to_block_scores(bs, elems, &block);
  elems++;

  block.offset = 7;
  block.score = 1.1;
  add_score_to_block_scores(bs, elems, &block);
  elems++;

  block.offset = 8;
  block.score = 1;
  add_score_to_block_scores(bs, elems, &block);
  elems++;

  block.offset = 9;
  block.score = 0.75;
  add_score_to_block_scores(bs, elems, &block);
  elems++;

  block.offset = 10;
  block.score = 0.5;
  add_score_to_block_scores(bs, elems, &block);
  elems++;

  fail_unless(elems == 10);
  fail_unless(bs[0].offset == 4);
  fail_unless(bs[0].score == 5);
  fail_unless(bs[1].offset == 2);
  fail_unless(bs[1].score == 4);
  fail_unless(bs[2].offset == 1);
  fail_unless(bs[2].score == 2.5);
  fail_unless(bs[3].offset == 3);
  fail_unless(bs[3].score == 1.5);
  fail_unless(bs[8].offset == 9);
  fail_unless(bs[8].score == 0.75);
  fail_unless(bs[9].offset == 10);
  fail_unless(bs[9].score == 0.5);

  free(bs);
}
END_TEST
//
// add blocks to fill array
// last block is at the front of the array
START_TEST(add_final_block_front_test)
{
  size_t size = 10;
  size_t elems = 0;
  struct block_scores *bs;
  bs = malloc(sizeof(struct block_scores) * size);

  fail_unless(bs != NULL);

  struct block_scores block;

  block.offset = 1;
  block.score = 2.5;
  add_score_to_block_scores(bs, elems, &block);
  elems++;

  block.offset = 4;
  block.score = 5;
  add_score_to_block_scores(bs, elems, &block);
  elems++;

  block.offset = 2;
  block.score = 4;
  add_score_to_block_scores(bs, elems, &block);
  elems++;

  block.offset = 3;
  block.score = 1.5;
  add_score_to_block_scores(bs, elems, &block);
  elems++;

  block.offset = 5;
  block.score = 1.3;
  add_score_to_block_scores(bs, elems, &block);
  elems++;

  block.offset = 6;
  block.score = 1.2;
  add_score_to_block_scores(bs, elems, &block);
  elems++;

  block.offset = 7;
  block.score = 1.1;
  add_score_to_block_scores(bs, elems, &block);
  elems++;

  block.offset = 8;
  block.score = 1;
  add_score_to_block_scores(bs, elems, &block);
  elems++;

  block.offset = 9;
  block.score = 3;
  add_score_to_block_scores(bs, elems, &block);
  elems++;

  block.offset = 10;
  block.score = 10;
  add_score_to_block_scores(bs, elems, &block);
  elems++;

  fail_unless(elems == 10);
  fail_unless(bs[0].offset == 10);
  fail_unless(bs[0].score == 10);
  fail_unless(bs[1].offset == 4);
  fail_unless(bs[1].score == 5);
  fail_unless(bs[2].offset == 2);
  fail_unless(bs[2].score == 4);
  fail_unless(bs[3].offset == 9);
  fail_unless(bs[3].score == 3);
  fail_unless(bs[4].offset == 1);
  fail_unless(bs[4].score == 2.5);
  fail_unless(bs[9].offset == 8);
  fail_unless(bs[9].score == 1);

  free(bs);
}
END_TEST

// add blocks to fill array
// last block is at the middle of the array
START_TEST(add_final_block_middle_test)
{
  size_t size = 10;
  size_t elems = 0;
  struct block_scores *bs;
  bs = malloc(sizeof(struct block_scores) * size);

  fail_unless(bs != NULL);

  struct block_scores block;

  block.offset = 1;
  block.score = 2.5;
  add_score_to_block_scores(bs, elems, &block);
  elems++;

  block.offset = 4;
  block.score = 5;
  add_score_to_block_scores(bs, elems, &block);
  elems++;

  block.offset = 2;
  block.score = 4;
  add_score_to_block_scores(bs, elems, &block);
  elems++;

  block.offset = 3;
  block.score = 1.5;
  add_score_to_block_scores(bs, elems, &block);
  elems++;

  block.offset = 5;
  block.score = 1.3;
  add_score_to_block_scores(bs, elems, &block);
  elems++;

  block.offset = 6;
  block.score = 1.2;
  add_score_to_block_scores(bs, elems, &block);
  elems++;

  block.offset = 7;
  block.score = 1.1;
  add_score_to_block_scores(bs, elems, &block);
  elems++;

  block.offset = 8;
  block.score = 1;
  add_score_to_block_scores(bs, elems, &block);
  elems++;

  block.offset = 9;
  block.score = 3;
  add_score_to_block_scores(bs, elems, &block);
  elems++;

  block.offset = 10;
  block.score = 2;
  add_score_to_block_scores(bs, elems, &block);
  elems++;

  fail_unless(elems == 10);
  fail_unless(bs[0].offset == 4);
  fail_unless(bs[0].score == 5);
  fail_unless(bs[1].offset == 2);
  fail_unless(bs[1].score == 4);
  fail_unless(bs[2].offset == 9);
  fail_unless(bs[2].score == 3);
  fail_unless(bs[3].offset == 1);
  fail_unless(bs[3].score == 2.5);
  fail_unless(bs[4].offset == 10);
  fail_unless(bs[4].score == 2);
  fail_unless(bs[9].offset == 8);
  fail_unless(bs[9].score == 1);

  free(bs);
}
END_TEST

Suite *
block_scores_suite(void)
{
  Suite *s = suite_create("Block_scores");

  TCase *tc = tcase_create("filling block_scores");
  tcase_add_test(tc, add_block_test);
  tcase_add_test(tc, add_block_full_test);
  tcase_add_test(tc, add_final_block_front_test);
  tcase_add_test(tc, add_final_block_middle_test);

  suite_add_tcase(s, tc);

  return s;
}

int
main(int argc, char **argv)
{
  int number_failed;

  Suite *s = block_scores_suite();
  SRunner *sr = srunner_create(s);
  srunner_run_all(sr, CK_NORMAL);
  number_failed = srunner_ntests_failed(sr);
  srunner_free(sr);
  return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
