#include "compute/qrx_aura_provider_supervisor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif
#define CHECK(x) do{if(!(x)){fprintf(stderr,"CHECK failed %s:%d: %s\n",__FILE__,__LINE__,#x);return 1;}}while(0)
static void h64(char out[65],char c){for(int i=0;i<64;i++)out[i]=c;out[64]=0;}
int main(void){
    QrxAuraProviderHostConfig h;qrx_aura_provider_host_config_defaults(&h);h.enabled=1;snprintf(h.provider_id,sizeof(h.provider_id),"provider-supervisor");snprintf(h.pod_id,sizeof(h.pod_id),"pod-supervisor");snprintf(h.network,sizeof(h.network),"alpha");snprintf(h.region,sizeof(h.region),"DE");snprintf(h.lease_journal_path,sizeof(h.lease_journal_path),"supervisor.leases");snprintf(h.job_journal_path,sizeof(h.job_journal_path),"supervisor.jobs");h.relay_required=1;h.relay_count=3;snprintf(h.relay_endpoints[0],sizeof(h.relay_endpoints[0]),"qrxrelay://relay-a:19001/a");snprintf(h.relay_endpoints[1],sizeof(h.relay_endpoints[1]),"qrxrelay://relay-b:19001/b");snprintf(h.relay_endpoints[2],sizeof(h.relay_endpoints[2]),"qrxrelay://relay-c:19001/c");
    QrxAuraProviderSupervisor s;CHECK(qrx_aura_provider_supervisor_init(&s,&h)==0);CHECK(qrx_aura_provider_supervisor_select_relay(&s,100)==0);CHECK(qrx_aura_provider_supervisor_record_relay(&s,0,0,1000,100)==0);CHECK(qrx_aura_provider_supervisor_record_relay(&s,0,0,1000,101)==0);CHECK(qrx_aura_provider_supervisor_record_relay(&s,0,0,1000,102)==0);CHECK(s.relays[0].cooldown_until_height>102);CHECK(qrx_aura_provider_supervisor_select_relay(&s,102)==1);
    CHECK(qrx_aura_provider_supervisor_record_relay(&s,1,1,20,103)==0);CHECK(qrx_aura_provider_supervisor_record_relay(&s,2,1,5,103)==0);CHECK(qrx_aura_provider_supervisor_select_relay(&s,103)==2);
    CHECK(qrx_aura_provider_supervisor_record_runtime(&s,0,103)==0);CHECK(qrx_aura_provider_supervisor_record_runtime(&s,0,104)==0);CHECK(qrx_aura_provider_supervisor_restart_recommended(&s,104)==0);CHECK(qrx_aura_provider_supervisor_record_runtime(&s,0,105)==0);CHECK(qrx_aura_provider_supervisor_restart_recommended(&s,105)==1);CHECK(qrx_aura_provider_supervisor_record_runtime(&s,1,106)==0);CHECK(qrx_aura_provider_supervisor_restart_recommended(&s,106)==0);
    QrxAuraJobJournal j;qrx_aura_job_journal_init(&j);char lease[65],req[65];h64(lease,'a');h64(req,'b');for(uint64_t seq=1;seq<=10;seq++){CHECK(qrx_aura_job_journal_prepare(&j,"requester",lease,seq,req)==0);if(seq<=6){char result[32];snprintf(result,sizeof(result),"result-%llu",(unsigned long long)seq);CHECK(qrx_aura_job_journal_commit(&j,"requester",lease,seq,req,0,result,strlen(result)+1)==0);}}
    s.keep_committed_jobs=2;s.keep_prepared_jobs=1;char path[256];snprintf(path,sizeof(path),"phase169-journal-%ld.dat",(long)getpid());remove(path);size_t removed=0;CHECK(qrx_aura_provider_supervisor_maintenance(&s,&j,path,&removed)==0);CHECK(removed==7&&j.count==3);QrxAuraJobJournal j2;qrx_aura_job_journal_init(&j2);CHECK(qrx_aura_job_journal_load(path,&j2)==0&&j2.count==3);remove(path);qrx_aura_job_journal_free(&j2);qrx_aura_job_journal_free(&j);
    QrxAuraProviderSupervisorStatus st;CHECK(qrx_aura_provider_supervisor_status(&s,106,&st)==0);CHECK(st.active_relay==2&&st.available_relays==3&&st.runtime_healthy&&st.restart_recommended==0);printf("phase169 AURA provider supervisor/failover/health: PASS\n");return 0;
}
