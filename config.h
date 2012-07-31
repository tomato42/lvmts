#ifndef _CONFIG_H
#define _CONFIG_H

struct program_params;

float get_read_multiplier(struct program_params *pp, char *lv_name);

float get_write_multiplier(struct program_params *pp, char *lv_name);

float get_hit_score(struct program_params *pp, char *lv_name);

float get_score_scaling_factor(struct program_params *pp, char *lv_name);

char *get_tier_device(struct program_params *pp, char *lv_name, int tier);

/**
 * Returns pinning score for provided volume
 */
float get_tier_pinning_score(struct program_params *pp, char *lv_name,
    int tier);

#endif
