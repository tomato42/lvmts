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

  free(bs);
}
END_TEST

Suite *
block_scores_suite(void)
{
  Suite *s = suite_create("Block_scores");

  TCase *tc = tcase_create("generic");
  tcase_add_test(tc, add_block_test);

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
