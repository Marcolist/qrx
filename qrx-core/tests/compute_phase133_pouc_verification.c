#include "compute/qrx_compute.h"
#include <stdio.h>
#include <string.h>
static void h(char x[65],char c){for(int i=0;i<64;i++)x[i]=c;x[64]=0;}
int main(void){QrxPoucReceipt r;memset(&r,0,sizeof(r));r.version=1;r.node_id=7;snprintf(r.provider_id,sizeof(r.provider_id),"provider-a");snprintf(r.runtime_id,sizeof(r.runtime_id),"qrx-ai-v1");h(r.graph_commitment,'a');h(r.input_commitment,'b');h(r.model_commitment,'c');h(r.execution_params_commitment,'d');h(r.result_commitment,'e');r.verified_compute_atoms=270000000;r.started_height=100;r.completed_height=110;r.verification_mode=QRX_POUC_VERIFY_REDUNDANT_2_OF_3;char c1[65],c2[65];if(qrx_pouc_receipt_commitment(&r,c1)||qrx_pouc_receipt_commitment(&r,c2)||strcmp(c1,c2))return 1;QrxPoucVerification v;if(qrx_pouc_verify_result(&r,3,2,&v)||!v.accepted)return 2;if(qrx_pouc_verify_result(&r,3,1,&v)||v.accepted)return 3;r.verified_compute_atoms=0;if(!qrx_pouc_receipt_validate(&r))return 4;puts("compute phase 133 PoUC verification: PASS");return 0;}
