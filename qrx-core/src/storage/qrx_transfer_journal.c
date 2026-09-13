#include "storage/qrx_transfer_journal.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <unistd.h>
#define MKDIR(p) mkdir((p),0700)
#endif
static int safe(const char*s){if(!s||!*s)return 0;for(;*s;s++)if(*s=='/'||*s=='\\'||*s=='\n'||*s=='\r'||*s=='=')return 0;return 1;}
int qrx_transfer_journal_save(const char*d,const QrxTransferJournal*j){if(!d||!j||!safe(j->transfer_id))return -1;MKDIR(d);char p[1400],t[1400];if(snprintf(p,sizeof(p),"%s/%s.journal",d,j->transfer_id)>=(int)sizeof(p)||snprintf(t,sizeof(t),"%s.tmp",p)>=(int)sizeof(t))return -1;FILE*f=fopen(t,"wb");if(!f)return -1;int ok=fprintf(f,"version=2\ntransfer_id=%s\ncontract_id=%s\ndirection=%s\npath=%s\ntotal=%llu\ncompleted=%llu\ncompleted_shards=%u\nrequired_shards=%u\ndata_shards=%u\nparity_shards=%u\nshard_size=%llu\nstate=%s\n",j->transfer_id,j->contract_id,j->direction,j->path,(unsigned long long)j->total_bytes,(unsigned long long)j->completed_bytes,j->completed_shards,j->required_shards,j->data_shards,j->parity_shards,(unsigned long long)j->shard_size,j->state)>0;if(fflush(f)||fclose(f)||!ok){remove(t);return -1;}if(rename(t,p)){remove(t);return -1;}return 0;}
int qrx_transfer_journal_load(const char*d,const char*id,QrxTransferJournal*j){if(!d||!safe(id)||!j)return -1;char p[1400];if(snprintf(p,sizeof(p),"%s/%s.journal",d,id)>=(int)sizeof(p))return -1;FILE*f=fopen(p,"rb");if(!f)return -1;memset(j,0,sizeof(*j));char line[1400];while(fgets(line,sizeof(line),f)){char*k=strtok(line,"=\n"),*v=strtok(NULL,"\n");if(!k||!v)continue;if(!strcmp(k,"transfer_id"))snprintf(j->transfer_id,sizeof(j->transfer_id),"%s",v);else if(!strcmp(k,"contract_id"))snprintf(j->contract_id,sizeof(j->contract_id),"%s",v);else if(!strcmp(k,"direction"))snprintf(j->direction,sizeof(j->direction),"%s",v);else if(!strcmp(k,"path"))snprintf(j->path,sizeof(j->path),"%s",v);else if(!strcmp(k,"total"))j->total_bytes=strtoull(v,NULL,10);else if(!strcmp(k,"completed"))j->completed_bytes=strtoull(v,NULL,10);else if(!strcmp(k,"completed_shards"))j->completed_shards=(uint32_t)strtoul(v,NULL,10);else if(!strcmp(k,"required_shards"))j->required_shards=(uint32_t)strtoul(v,NULL,10);else if(!strcmp(k,"data_shards"))j->data_shards=(uint32_t)strtoul(v,NULL,10);else if(!strcmp(k,"parity_shards"))j->parity_shards=(uint32_t)strtoul(v,NULL,10);else if(!strcmp(k,"shard_size"))j->shard_size=strtoull(v,NULL,10);else if(!strcmp(k,"state"))snprintf(j->state,sizeof(j->state),"%s",v);}fclose(f);return strcmp(j->transfer_id,id)?-1:0;}
