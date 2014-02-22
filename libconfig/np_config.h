#ifndef NPCONFIG_H
#define NPCONFIG_H

struct _NetPhoneConfig;
typedef struct _NetPhoneConfig NetPhoneConfig;

#ifdef __cplusplus
extern "C" {
#endif

NetPhoneConfig * np_config_new(const char *filename);
void np_config_destroy(NetPhoneConfig *cfg);

const char *np_config_get_string(NetPhoneConfig *npconfig, const char *section, const char *key, const char *default_string);
int np_config_get_int(NetPhoneConfig *npconfig,const char *section, const char *key, int default_value);
float np_config_get_float(NetPhoneConfig *npconfig,const char *section, const char *key, float default_value);
void np_config_set_string(NetPhoneConfig *npconfig,const char *section, const char *key, const char *value);
void np_config_set_int(NetPhoneConfig *npconfig,const char *section, const char *key, int value);
int np_config_sync(NetPhoneConfig *npconfig);
int np_config_has_section(NetPhoneConfig *npconfig, const char *section);
void np_config_clean_section(NetPhoneConfig *npconfig, const char *section);
	
//检测是否需要更新配置
int np_config_needs_commit(const NetPhoneConfig *npconfig);
	
#ifdef __cplusplus
}
#endif

#endif
