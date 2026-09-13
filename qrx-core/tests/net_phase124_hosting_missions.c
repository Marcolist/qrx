#include "resource/qrx_storage_atlas.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void){
    QrxStorageAtlasCell cells[3]; memset(cells,0,sizeof(cells));
    strcpy(cells[0].region,"EU-CELL-A"); cells[0].publicly_visible=1; cells[0].provider_count=6; cells[0].free_bytes=256ULL<<20; cells[0].opportunity_score=80;
    strcpy(cells[1].region,"HIDDEN-SPARSE"); cells[1].publicly_visible=0; cells[1].provider_count=1; cells[1].free_bytes=0; cells[1].opportunity_score=100;
    strcpy(cells[2].region,"EU-CELL-B"); cells[2].publicly_visible=1; cells[2].provider_count=12; cells[2].free_bytes=2ULL<<30; cells[2].opportunity_score=10;
    QrxStorageMission *m=NULL; size_t n=0;
    assert(qrx_storage_public_missions_from_atlas(cells,3,1ULL<<30,10,&m,&n)==0);
    assert(n==1); assert(!strcmp(m[0].region,"EU-CELL-A"));
    assert(m[0].wanted_additional_providers==4); assert(m[0].wanted_additional_bytes==(768ULL<<20));
    assert(m[0].incentive_factor_bps==11600); assert(m[0].incentive_factor_bps<=12000);
    qrx_storage_missions_free(m);
    puts("net_phase124_hosting_missions: ok"); return 0;
}
