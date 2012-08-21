#ifndef _CONFIG_H
#define _CONFIG_H

#include <confuse.h>

struct program_params {
    char *conf_file_path;
    void *lvm2_handle;
    cfg_t *cfg;
};

struct program_params* new_program_params();

void free_program_params(struct program_params *pp);

float get_read_multiplier(struct program_params *pp, const char *lv_name);

float get_write_multiplier(struct program_params *pp, const char *lv_name);

float get_hit_score(struct program_params *pp, const char *lv_name);

float get_score_scaling_factor(struct program_params *pp, const char *lv_name);

/**
 * Return name of device for provided volume at tier
 */
const char *get_tier_device(struct program_params *pp, const char *lv_name, int tier);

/**
 * Return tier of device named dev in volume lv_name
 *
 * -1 if not found
 */
int get_device_tier(struct program_params *pp, const char *lv_name, const char *dev);

/**
 * Returns pinning score for provided volume
 */
float get_tier_pinning_score(struct program_params *pp, const char *lv_name,
    int tier);

/**
 * checks if tiers with larger tier value exist for provided volume
 */
int lower_tiers_exist(struct program_params *pp, const char *lv_name, int tier);

/**
 * checks if tiers with smaller tier value exist for provided volume
 */
int higher_tiers_exist(struct program_params *pp, const char *lv_name, int tier);

/**
 * Return max used space on tier by volume
 */
long int get_max_space_tier(struct program_params *pp, const char *lv_name,
    int tier);

/**
 * read configuration file
 */
int read_config(struct program_params *pp);

const char *get_volume_lv(struct program_params *pp, const char *lv_name);

const char *get_volume_vg(struct program_params *pp, const char *lv_name);

#endif
