#include "resource/qrx_resource_live.h"
#include "qrxdb.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#include <unistd.h>
#define MKDIR(p) mkdir(p,0700)
#endif
static void put_provider(QrxDB*d,int i,const char*region,const char*asn){char k[256],v[1200];snprintf(k,sizeof(k),"storage/provider/p%d",i);snprintf(v,sizeof(v),"1|owner%d|1|1000|1000000000|100000000|0|9900|9800|10000|9000|op%d|claimasn|claimregion|%s|%s|1",i,i,asn,region);assert(qrxdb_put(d,k,v)==0);}
int main(void){char dir[256];snprintf(dir,sizeof(dir),"/tmp/qrx-live-dashboard-%ld",(long)getpid());MKDIR(dir);QrxDB db;assert(qrxdb_init(&db,dir)==0);
 put_provider(&db,1,"EU-WEST","AS1");put_provider(&db,2,"EU-WEST","AS2");put_provider(&db,3,"EU-WEST","AS3");put_provider(&db,4,"PRIVATE-CELL","AS4");
 assert(qrxdb_put(&db,"storage/contract/c1","1|alice|STANDARD|1000|100|14|1|1000|10000|50|200|9750|0|0|0|1|00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000|1")==0);
 assert(qrxdb_put(&db,"storage/assignment/c1/0000000000","1|p1|2|100|aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa|00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000|1|10|1|20|1")==0);
 assert(qrxdb_put(&db,"storage/assignment/c1/0000000001","1|p2|3|100|bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb|00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000|1|10|1|20|1")==0);
 assert(qrxdb_put(&db,"storage/assignment/c1/0000000002","1|p3|4|100|cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc|00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000|1|10|1|20|1")==0);
 assert(qrxdb_put(&db,"qrxnet/domain/hash1","dummy")==0);qrxdb_close(&db);
 QrxResourceDashboardSnapshot s;QrxStorageAtlasCell*c=NULL;size_t n=0;assert(qrx_resource_live_snapshot(dir,3,&s,&c,&n)==0);assert(s.storage.providers_total==4);assert(s.active_contracts==1);assert(s.storage.logical_user_bytes==1000);assert(s.storage.healthy_shards==1);assert(s.storage.degraded_shards==1);assert(s.storage.repairing_shards==1);assert(s.active_domains==1);assert(s.visible_regions==1);assert(s.hidden_regions==1);assert(s.serving_providers==4);assert(s.independent_asns==4);qrx_storage_atlas_free(c);printf("storage_phase101_live_dashboard: ok\n");return 0;}
