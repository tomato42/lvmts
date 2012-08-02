#ifndef _CONFIG_H
#define _CONFIG_H

#include <confuse.h>

struct program_params {
    char *conf_file_path;
    void *lvm2_handle;
    cfg_t *cfg;
};

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
 * read configuration file
 */
int read_config(struct program_params *pp);
#endif
