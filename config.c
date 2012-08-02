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

#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <confuse.h>
#include <assert.h>
#include "config.h"

// TODO stub
float
get_read_multiplier(struct program_params *pp, char *lv_name)
{
    return 1;
}

// TODO stub
float
get_write_multiplier(struct program_params *pp, char *lv_name)
{
    return 10;
}

// TODO stub
float
get_hit_score(struct program_params *pp, char *lv_name)
{
    return 16;
}

// TODO stub
float
get_score_scaling_factor(struct program_params *pp, char *lv_name)
{
    return pow(2,-15);
}

// TODO stub
char *
get_tier_device(struct program_params *pp, char *lv_name, int tier)
{
    static char *tiers[] = { "/dev/0",
                       "/dev/1",
                       "/dev/2",
                       "/dev/3",
                       "/dev/4"};

    return tiers[tier];
}

// TODO stub
float get_tier_pinning_score(struct program_params *pp, char *lv_name,
    int tier)
{
    return 20-tier*10;
}

// parse time from string, such as "5m", "20s", "3h", "3:10" or "1:15:34"
// as, respectively: 300, 20, 10800, 11400 and 4534
// by default assume minutes
static int
parse_time_value(cfg_t *cfg, cfg_opt_t *opt, const char *value, void *result)
{
    assert(cfg);
    assert(result);
    assert(opt);
    assert(value);

    char *endptr;
    long int *res = (long int *)result;

    long int sum;
    long int partial;

    sum = strtol(value, &endptr, 10);

    // check simple errors while parsing number
    if (endptr == value) {
        cfg_error(cfg, "Invalid value for option %s: value can't be parsed "
            "as a number", opt->name);
        return -1;
    }
    if (sum == LONG_MAX) {
        cfg_error(cfg, "Value too large for option %s.", opt->name);
        return -1;
    }
    if (sum < 0) {
        cfg_error(cfg, "Value can't be negative for option %s.", opt->name);
        return -1;
    }

    // check if a unit has been specified
    switch(*endptr){
      case '\0': // no unit, assume "minutes"
        cfg_error(cfg, "Warning, no unit specified for option %s, assuming "
            "minutes.", opt->name);
        /* fall through */
      case 'm': // m is for "minutes"
      case ':': // so is colon (until the second one, then it's hours)
        sum *= 60;
        break;
      case 'h': // h is for "hours"
        sum *= 3600;
        break;
      case 'd': // d is for "days"
        sum *= 24 * 60 * 60;
        break;
      case 's':
        // do nothing
        break;
      case '\t':
      case ' ':
        cfg_error(cfg, "Whitespace in option %s.", opt->name);
        return -1;
      default:
        cfg_error(cfg, "Unrecognized trailing characters in option %s: %s",
            opt->name, endptr);
        return -1;
    }
    // don't advance past the end of the string
    if (*endptr)
        value = endptr + 1;
    else
        value = endptr;

    partial = strtol(value, &endptr, 10);
    if (endptr == value) {
        // it's OK
    }
    if (partial == LONG_MAX) {
        cfg_error(cfg, "Value too large for option %s.", opt->name);
        return -1;
    }
    if (partial < 0) {
        cfg_error(cfg, "Value can't be negative in option %s.", opt->name);
        return -1;
    }

    switch(*endptr) {
      case '\0': // no unit, assume seconds
        sum += partial;
        break;
      case ':': // we have a "hh:mm:ss" construct...
        sum += partial;
        sum *= 60;
        break;
      case ' ':
      case '\t':
        cfg_error(cfg, "Whitespace in option %s.", opt->name);
        return -1;
      default:
        cfg_error(cfg, "Unrecognized trailing characters in option %s: %s",
            opt->name, endptr);
        return -1;
    }
    // don't advance past the end of the string
    if (*endptr)
        value = endptr + 1;
    else
        value = endptr;

    // parse the last part in "hh:mm:ss" construct
    partial = strtol(value, &endptr, 10);

    if (endptr == value) {
        // OK
    }
    if (partial == LONG_MAX) {
        cfg_error(cfg, "Value too large for option %s.", opt->name);
        return -1;
    }
    if (partial < 0) {
        cfg_error(cfg, "Value can't be negative for option %s.", opt->name);
        return -1;
    }
    if (*endptr != '\0') {
        cfg_error(cfg, "Trailing character(s) in option %s: %s", opt->name,
            endptr);
        return -1;
    }

    sum += partial;

    *res = sum;

    return 0;
}

// parse volume sizes from string, such as "4b", "1k", "4M", "11G"
// as, respectively: 4, 1024, 4194304 and 11811160064
static int
parse_size_value(cfg_t *cfg, cfg_opt_t *opt, const char *value, void *result)
{
    long int res;
    char *endptr;

    res = strtol(value, &endptr, 10);

    // check for simple errors while parsing number
    if (endptr == value) {
        cfg_error(cfg, "Invalid value for option %s: value can't be parsed "
            "as a number.", opt->name);
        return -1;
    }
    if (res == LONG_MAX) {
        cfg_error(cfg, "Value too large for option %s.", opt->name);
        return -1;
    }
    if (res < 0) {
        cfg_error(cfg, "Value can't be negative for option %s.", opt->name);
        return -1;
    }
    if (res == 0) {
        cfg_error(cfg, "Value can't be zero for option %s.", opt->name);
        return -1;
    }

    switch(*endptr) {
        case '\0':
        case 'b':
        case 'B':
          /* do nothing */
          break;
        case 's':
        case 'S':
          res *= 512;
          break;
        case 'k':
        case 'K':
          res *= 1024L;
          break;
        case 'm':
        case 'M':
          res *= 1024L * 1024L;
          break;
        case 'g':
        case 'G':
          res *= 1024L * 1024L * 1024L;
          break;
        case 't':
        case 'T':
          res *= 1024L * 1024L * 1024L * 1024L;
          break;
        default:
          cfg_error(cfg, "Unrecognized trailing characters for option %s: %s",
              opt->name, endptr);
          return -1;
    }
    if (*endptr && *(endptr+1)) {
        cfg_error(cfg, "Trailing characters in option %s: %s", opt->name, endptr);
        return -1;
    }

    *(long int*)result = res;

    return 0;
}

static int
validate_require_nonnegative(cfg_t *cfg, cfg_opt_t *opt)
{
    if (opt->type == CFGT_INT) {
        long int value = cfg_opt_getnint(opt, cfg_opt_size(opt) - 1);
        if (value < 0) {
            cfg_error(cfg, "Value for option %s can't be negative in %s section \"%s\"",
                opt->name, cfg->name, cfg_title(cfg));
            return -1;
        }
    } else if (opt->type == CFGT_FLOAT) {
        double value = cfg_opt_getnfloat(opt, cfg_opt_size(opt) - 1);
        if (value < 0.0) {
            cfg_error(cfg, "Value for option %s can't be negative in %s section \"%s\"",
                opt->name, cfg->name, cfg_title(cfg));
            return -1;
        }
    }

    return 0;
}

static int
validate_require_positive(cfg_t *cfg, cfg_opt_t *opt)
{
    assert(opt->type == CFGT_INT || opt->type == CFGT_FLOAT);

    if (opt->type == CFGT_INT) {
        long int value = cfg_opt_getnint(opt, cfg_opt_size(opt) - 1);
        if (value <= 0) {
            cfg_error(cfg, "Value for option %s must be positive in %s section \"%s\"",
                opt->name, cfg->name, cfg_title(cfg));
            return -1;
        }
    } else if (opt->type == CFGT_FLOAT) {
        double value = cfg_opt_getnfloat(opt, cfg_opt_size(opt) - 1);
        if (value <= 0.0) {
            cfg_error(cfg, "Value for option %s must be positive in %s section \"%s\"",
                opt->name, cfg->name, cfg_title(cfg));
            return -1;
        }
    }
    return 0;
}

/* read configuration file
 * TODO
 */
int
read_config(struct program_params *pp)
{
    static cfg_opt_t pv_opts[] = {
        CFG_INT("tier", 0, CFGF_NONE),
        CFG_FLOAT("pinningScore", 0, CFGF_NONE),
        CFG_STR("path", NULL, CFGF_NONE),
        CFG_INT_CB("maxUsedSpace", 0, CFGF_NONE, parse_size_value),
        CFG_END()
    };

    static cfg_opt_t volume_opts[] = {
        CFG_STR("LogicalVolume", NULL, CFGF_NONE),
        CFG_STR("VolumeGroup",   NULL, CFGF_NONE),
        CFG_FLOAT("timeExponent",  1.0/(2<<14), CFGF_NONE),
        CFG_FLOAT("hitScore",      16, CFGF_NONE),
        CFG_FLOAT("readMultiplier", 1, CFGF_NONE),
        CFG_FLOAT("writeMultiplier", 4, CFGF_NONE),
        CFG_INT_CB("pvmoveWait",     5*60, CFGF_NONE, parse_time_value),
        CFG_INT_CB("checkWait",      15*60, CFGF_NONE, parse_time_value),
        CFG_SEC("pv", pv_opts, CFGF_TITLE | CFGF_MULTI),
        CFG_END()
    };

    cfg_opt_t opts[] = {
        CFG_SEC("volume", volume_opts, CFGF_TITLE | CFGF_MULTI),
        CFG_END()
    };

    cfg_t *cfg;

    cfg = cfg_init(opts, CFGF_NONE);
    assert(cfg);

    // add verification functions
    cfg_set_validate_func(cfg, "volume|timeExponent",
        validate_require_positive);
    cfg_set_validate_func(cfg, "volume|hitScore",
        validate_require_positive);
    cfg_set_validate_func(cfg, "volume|readMultiplier",
        validate_require_nonnegative);
    cfg_set_validate_func(cfg, "volume|writeMultiplier",
        validate_require_nonnegative);
    cfg_set_validate_func(cfg, "volume|pv|pinningScore",
        validate_require_nonnegative);
    cfg_set_validate_func(cfg, "volume|pv|tier",
        validate_require_nonnegative);
    cfg_set_validate_func(cfg, "volume|pv|maxUsedSpace",
        validate_require_nonnegative);
    // TODO cfg_set_validate_func(cfg, "volume", validate_pv);

    cfg_parse(cfg, "doc/sample.conf");

    cfg_free(cfg);

    return 0;
}
