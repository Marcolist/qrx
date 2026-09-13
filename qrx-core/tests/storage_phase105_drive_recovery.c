#include "storage/qrx_drive_runtime.h"
#include "storage/qrx_transfer_journal.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int main(void){char d[]="/tmp/qrx-recover-XXXXXX";assert(mkdtemp(d));char jr[512];snprintf(jr,sizeof(jr),"%s/journals",d);QrxTransferJournal j={0};snprintf(j.transfer_id,sizeof(j.transfer_id),"recovered-1");snprintf(j.contract_id,sizeof(j.contract_id),"contract-R");snprintf(j.direction,sizeof(j.direction),"download");snprintf(j.path,sizeof(j.path),"%s/restored file.bin",d);j.total_bytes=900000;j.completed_bytes=123456;j.completed_shards=4;j.required_shards=10;j.data_shards=10;j.parity_shards=4;j.shard_size=90000;snprintf(j.state,sizeof(j.state),"RUNNING");assert(qrx_transfer_journal_save(jr,&j)==0);QrxDriveRuntime*r=NULL;assert(qrx_drive_runtime_open(d,jr,&r)==0);QrxDriveJobSnapshot s[4];assert(qrx_drive_runtime_list(r,s,4)==1);assert(!strcmp(s[0].transfer_id,"recovered-1")&&s[0].state==QRX_DRIVE_JOB_PAUSED&&s[0].completed_bytes==123456&&s[0].required_shards==10);assert(qrx_drive_runtime_resume(r,"recovered-1")==-2);qrx_drive_runtime_close(r);puts("PASS: unfinished Drive journal recovers paused and fails closed until verified discovery is restored");return 0;}
