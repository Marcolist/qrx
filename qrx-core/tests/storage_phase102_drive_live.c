#include "resource/qrx_drive_live.h"
#include "qrxdb.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#define getpid _getpid
#else
#include <sys/stat.h>
#include <unistd.h>
#define MKDIR(p) mkdir(p,0700)
#endif
static void put_provider(QrxDB*d,int i,const char*region,const char*asn){char k[256],v[1200];snprintf(k,sizeof(k),"storage/provider/p%d",i);snprintf(v,sizeof(v),"1|owner%d|1|1000|1000000000|100000000|0|9900|9800|10000|9000|op%d|claimasn|claimregion|%s|%s|1",i,i,asn,region);assert(qrxdb_put(d,k,v)==0);}
static void put_assignment(QrxDB*d,int shard,const char*pid,unsigned state,char ch){char k[256],v[1200],oid[65];memset(oid,ch,64);oid[64]=0;snprintf(k,sizeof(k),"storage/assignment/c1/%010d",shard);snprintf(v,sizeof(v),"1|%s|%u|100|%s|00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000|1|10|1|20|1",pid,state,oid);assert(qrxdb_put(d,k,v)==0);}
int main(void){char dir[256];snprintf(dir,sizeof(dir),"/tmp/qrx-drive-live-%ld",(long)getpid());MKDIR(dir);QrxDB db;assert(qrxdb_init(&db,dir)==0);
 put_provider(&db,1,"EU-WEST","AS1");put_provider(&db,2,"EU-WEST","AS2");put_provider(&db,3,"EU-WEST","AS3");put_provider(&db,4,"PRIVATE-CELL","AS4");
 assert(qrxdb_put(&db,"storage/contract/c1","1|alice|STANDARD|1000|100|14|1|1000|10000|50|200|9750|0|0|0|1|00000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000|1")==0);
 for(int i=0;i<10;i++)put_assignment(&db,i,i==9?"p4":((i%3)==0?"p1":(i%3)==1?"p2":"p3"),2,(char)('a'+i));
 put_assignment(&db,10,"p1",3,'k');put_assignment(&db,11,"p2",4,'l');qrxdb_close(&db);
 QrxDriveFileSnapshot *files=NULL;size_t fn=0;assert(qrx_drive_live_list(dir,"alice",&files,&fn)==0);assert(fn==1);assert(files[0].healthy_shards==10);assert(files[0].required_shards==10);assert(files[0].degraded_shards==1);assert(files[0].repairing_shards==1);assert(files[0].missing_shards==2);qrx_drive_live_files_free(files);
 assert(qrx_drive_live_list(dir,"bob",&files,&fn)==0);assert(fn==0);qrx_drive_live_files_free(files);
 QrxDriveShardRoute *routes=NULL;size_t rn=0;QrxDriveFileSnapshot f;assert(qrx_drive_live_routes(dir,"c1",3,&routes,&rn,&f)==0);assert(rn==12);assert(f.healthy_shards==10);int public_seen=0,private_seen=0;for(size_t i=0;i<rn;i++){if(routes[i].region_public){assert(strcmp(routes[i].region,"EU-WEST")==0);public_seen++;}else if(!strcmp(routes[i].region,"privacy-protected"))private_seen++;}assert(public_seen>0);assert(private_seen==1);qrx_drive_live_routes_free(routes);puts("storage_phase102_drive_live: ok");return 0;}
