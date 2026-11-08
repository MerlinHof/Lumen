#ifndef LUMEN_SYSTEM_UPDATE_H
#define LUMEN_SYSTEM_UPDATE_H
int system_update_stage(const char *archive,const char *version);
int system_update_maintenance(void);
int system_update_rollback(void);
int system_update_trial(void);
int system_update_keep(void);
#endif
