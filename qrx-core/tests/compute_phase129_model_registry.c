#include "compute/qrx_compute.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void root(char out[65],char c){for(int i=0;i<64;i++)out[i]=c;out[64]=0;}
int main(void){
 QrxAiModelRecord m; memset(&m,0,sizeof(m)); m.registry_version=QRX_MODEL_REGISTRY_VERSION;
 strcpy(m.model_id,"kimi-k3"); strcpy(m.model_version,"target-v1"); strcpy(m.architecture,"mixture-of-experts"); strcpy(m.runtime_id,"qrx-kimi-k3-runtime-v1");
 root(m.manifest_root,'a'); root(m.tokenizer_root,'b'); root(m.expert_manifest_root,'c'); m.min_memory_bytes=16ULL<<30; m.min_storage_bytes=64ULL<<30; m.verification_profile=QRX_MODEL_VERIFY_HIGH; strcpy(m.license_id,"upstream-license-metadata-required"); m.is_moe=1;
 assert(qrx_ai_model_validate(&m)==0); char a[65],b[65]; assert(qrx_ai_model_commitment(&m,a)==0); assert(qrx_ai_model_commitment(&m,b)==0); assert(!strcmp(a,b));
 QrxAiModelRegistry r; memset(&r,0,sizeof(r)); assert(qrx_ai_model_registry_add(&r,&m)==0); assert(qrx_ai_model_registry_add(&r,&m)==-2); assert(qrx_ai_model_registry_find(&r,"kimi-k3","target-v1")!=NULL);
 QrxAiModelRecord bad=m; root(bad.expert_manifest_root,'0'); assert(qrx_ai_model_validate(&bad)!=0); bad=m; bad.is_moe=0; assert(qrx_ai_model_validate(&bad)!=0); bad=m; bad.manifest_root[0]='z'; assert(qrx_ai_model_validate(&bad)!=0);
 printf("compute_phase129_model_registry PASS commitment=%s\\n",a); return 0;
}