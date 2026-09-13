#include "storage/qrx_erasure.h"
#include <assert.h>
#include <openssl/rand.h>
#include <stdlib.h>
#include <string.h>
static void drop(QrxErasureSet *s,unsigned i){free(s->shards[i]);s->shards[i]=NULL;s->present[i]=0;}
static void run(unsigned k,unsigned m){size_t n=123457;uint8_t *data=malloc(n);assert(data&&RAND_bytes(data,(int)n)==1);QrxErasureSet s;assert(qrx_erasure_encode(data,n,k,m,&s)==0);drop(&s,0);drop(&s,k/2);drop(&s,k);drop(&s,k+m-1);assert(qrx_erasure_reconstruct(&s)==0);uint8_t *out=NULL;size_t olen=0;assert(qrx_erasure_join(&s,&out,&olen)==0&&olen==n&&memcmp(data,out,n)==0);free(out);qrx_erasure_free(&s);assert(qrx_erasure_encode(data,n,k,m,&s)==0);for(unsigned i=0;i<m+1;i++)drop(&s,i);assert(qrx_erasure_reconstruct(&s)!=0);qrx_erasure_free(&s);free(data);}
int main(void){run(10,4);run(16,4);return 0;}
