#include "storage/qrx_erasure.h"
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define QRX_FSEEK64(f,o,w) _fseeki64((f),(__int64)(o),(w))
#define QRX_FTELL64(f) ((int64_t)_ftelli64((f)))
#else
#define QRX_FSEEK64(f,o,w) fseeko((f),(off_t)(o),(w))
#define QRX_FTELL64(f) ((int64_t)ftello((f)))
#endif

static uint8_t gf_exp[512], gf_log[256];
static int gf_ready=0;
static void gf_init(void){if(gf_ready)return;unsigned x=1;for(int i=0;i<255;i++){gf_exp[i]=(uint8_t)x;gf_log[x]=(uint8_t)i;x<<=1;if(x&0x100)x^=0x11d;}for(int i=255;i<512;i++)gf_exp[i]=gf_exp[i-255];gf_log[0]=0;gf_ready=1;}
static uint8_t gf_mul(uint8_t a,uint8_t b){if(!a||!b)return 0;return gf_exp[gf_log[a]+gf_log[b]];}
static uint8_t gf_inv(uint8_t a){return a?gf_exp[255-gf_log[a]]:0;}
static uint8_t gf_pow(uint8_t a,unsigned p){if(!p)return 1;if(!a)return 0;return gf_exp[(gf_log[a]*p)%255];}

static int matrix_invert(const uint8_t *in,uint8_t *out,unsigned n){
    size_t cols=(size_t)n*2;uint8_t *aug=calloc(n,cols);if(!aug)return -1;
    for(unsigned r=0;r<n;r++){memcpy(aug+r*cols,in+r*n,n);aug[r*cols+n+r]=1;}
    for(unsigned c=0;c<n;c++){unsigned pivot=c;while(pivot<n&&!aug[pivot*cols+c])pivot++;if(pivot==n){free(aug);return -1;}if(pivot!=c)for(size_t j=0;j<cols;j++){uint8_t t=aug[c*cols+j];aug[c*cols+j]=aug[pivot*cols+j];aug[pivot*cols+j]=t;}
        uint8_t inv=gf_inv(aug[c*cols+c]);for(size_t j=0;j<cols;j++)aug[c*cols+j]=gf_mul(aug[c*cols+j],inv);
        for(unsigned r=0;r<n;r++)if(r!=c&&aug[r*cols+c]){uint8_t f=aug[r*cols+c];for(size_t j=0;j<cols;j++)aug[r*cols+j]^=gf_mul(f,aug[c*cols+j]);}
    }
    for(unsigned r=0;r<n;r++)memcpy(out+r*n,aug+r*cols+n,n);free(aug);return 0;
}

static int generator_matrix(unsigned k,unsigned m,uint8_t **mat_out){
    gf_init();unsigned n=k+m;if(!k||!m||n>255)return -1;uint8_t *v=malloc((size_t)n*k),*top=malloc((size_t)k*k),*inv=malloc((size_t)k*k),*g=malloc((size_t)n*k);if(!v||!top||!inv||!g){free(v);free(top);free(inv);free(g);return -1;}
    for(unsigned r=0;r<n;r++){uint8_t x=(uint8_t)(r+1);for(unsigned c=0;c<k;c++)v[(size_t)r*k+c]=gf_pow(x,c);}
    memcpy(top,v,(size_t)k*k);if(matrix_invert(top,inv,k)!=0){free(v);free(top);free(inv);free(g);return -1;}
    for(unsigned r=0;r<n;r++)for(unsigned c=0;c<k;c++){uint8_t z=0;for(unsigned j=0;j<k;j++)z^=gf_mul(v[(size_t)r*k+j],inv[(size_t)j*k+c]);g[(size_t)r*k+c]=z;}
    free(v);free(top);free(inv);*mat_out=g;return 0;
}

void qrx_erasure_free(QrxErasureSet *s){if(!s)return;if(s->shards){unsigned n=s->data_shards+s->parity_shards;for(unsigned i=0;i<n;i++)free(s->shards[i]);free(s->shards);}free(s->present);memset(s,0,sizeof(*s));}

int qrx_erasure_encode(const uint8_t *data,size_t len,unsigned k,unsigned m,QrxErasureSet *out){
    if(!out||(!data&&len)||!k||!m||k+m>255)return -1;memset(out,0,sizeof(*out));size_t ss=len?(len+k-1)/k:1;unsigned n=k+m;out->shards=calloc(n,sizeof(uint8_t*));out->present=calloc(n,1);if(!out->shards||!out->present){qrx_erasure_free(out);return -1;}out->data_shards=k;out->parity_shards=m;out->shard_size=ss;out->original_size=len;
    for(unsigned i=0;i<n;i++){out->shards[i]=calloc(ss,1);if(!out->shards[i]){qrx_erasure_free(out);return -1;}out->present[i]=1;}
    for(unsigned i=0;i<k;i++){size_t off=(size_t)i*ss;if(off<len){size_t c=len-off;if(c>ss)c=ss;memcpy(out->shards[i],data+off,c);}}
    uint8_t *g=NULL;if(generator_matrix(k,m,&g)!=0){qrx_erasure_free(out);return -1;}
    for(unsigned r=k;r<n;r++)for(size_t b=0;b<ss;b++){uint8_t z=0;for(unsigned c=0;c<k;c++)z^=gf_mul(g[(size_t)r*k+c],out->shards[c][b]);out->shards[r][b]=z;}free(g);return 0;
}

int qrx_erasure_reconstruct(QrxErasureSet *s){
    if(!s||!s->shards||!s->present||!s->data_shards||!s->parity_shards)return -1;unsigned k=s->data_shards,n=k+s->parity_shards;unsigned avail=0;for(unsigned i=0;i<n;i++)if(s->present[i]&&s->shards[i])avail++;if(avail<k)return -1;
    uint8_t *g=NULL;if(generator_matrix(k,s->parity_shards,&g)!=0)return -1;unsigned *rows=malloc(k*sizeof(unsigned));uint8_t *sub=malloc((size_t)k*k),*inv=malloc((size_t)k*k);if(!rows||!sub||!inv){free(g);free(rows);free(sub);free(inv);return -1;}unsigned z=0;for(unsigned i=0;i<n&&z<k;i++)if(s->present[i]&&s->shards[i])rows[z++]=i;for(unsigned r=0;r<k;r++)memcpy(sub+(size_t)r*k,g+(size_t)rows[r]*k,k);if(matrix_invert(sub,inv,k)!=0){free(g);free(rows);free(sub);free(inv);return -1;}
    uint8_t **orig=calloc(k,sizeof(uint8_t*));if(!orig){free(g);free(rows);free(sub);free(inv);return -1;}for(unsigned d=0;d<k;d++){orig[d]=calloc(s->shard_size,1);if(!orig[d]){for(unsigned j=0;j<d;j++)free(orig[j]);free(orig);free(g);free(rows);free(sub);free(inv);return -1;}for(size_t b=0;b<s->shard_size;b++){uint8_t v=0;for(unsigned r=0;r<k;r++)v^=gf_mul(inv[(size_t)d*k+r],s->shards[rows[r]][b]);orig[d][b]=v;}}
    for(unsigned d=0;d<k;d++){if(!s->shards[d])s->shards[d]=malloc(s->shard_size);if(!s->shards[d]){for(unsigned j=0;j<k;j++)free(orig[j]);free(orig);free(g);free(rows);free(sub);free(inv);return -1;}memcpy(s->shards[d],orig[d],s->shard_size);s->present[d]=1;}
    for(unsigned r=k;r<n;r++)if(!s->present[r]||!s->shards[r]){if(!s->shards[r])s->shards[r]=malloc(s->shard_size);if(!s->shards[r]){for(unsigned j=0;j<k;j++)free(orig[j]);free(orig);free(g);free(rows);free(sub);free(inv);return -1;}for(size_t b=0;b<s->shard_size;b++){uint8_t v=0;for(unsigned c=0;c<k;c++)v^=gf_mul(g[(size_t)r*k+c],orig[c][b]);s->shards[r][b]=v;}s->present[r]=1;}
    for(unsigned j=0;j<k;j++)free(orig[j]);free(orig);free(g);free(rows);free(sub);free(inv);return 0;
}

int qrx_erasure_join(const QrxErasureSet *s,uint8_t **data_out,size_t *len_out){if(!s||!data_out||!len_out||!s->shards)return -1;for(unsigned i=0;i<s->data_shards;i++)if(!s->present[i]||!s->shards[i])return -1;uint8_t *d=malloc(s->original_size?s->original_size:1);if(!d)return -1;size_t copied=0;for(unsigned i=0;i<s->data_shards&&copied<s->original_size;i++){size_t c=s->original_size-copied;if(c>s->shard_size)c=s->shard_size;memcpy(d+copied,s->shards[i],c);copied+=c;}*data_out=d;*len_out=s->original_size;return 0;}


#include <stdio.h>
#include <stdint.h>
int qrx_erasure_encode_file(const char*source,unsigned k,unsigned m,const char*const*outp,size_t stripe,uint64_t*orig_out,uint64_t*ss_out){
    if(!source||!outp||!k||!m||k+m>255)return -1;if(!stripe)stripe=128*1024;
    FILE*in=fopen(source,"rb");if(!in)return -1;
    if(QRX_FSEEK64(in,0,SEEK_END)){fclose(in);return -1;}int64_t zl=QRX_FTELL64(in);if(zl<0||QRX_FSEEK64(in,0,SEEK_SET)){fclose(in);return -1;}uint64_t len=(uint64_t)zl,ss=len?(len+k-1)/k:1;unsigned n=k+m;
    FILE**fo=calloc(n,sizeof(*fo));uint8_t**db=calloc(k,sizeof(*db));uint8_t**pb=calloc(m,sizeof(*pb));uint8_t*g=NULL;int rc=-1;
    if(!fo||!db||!pb||generator_matrix(k,m,&g))goto done;
    for(unsigned i=0;i<n;i++){if(!outp[i]||!(fo[i]=fopen(outp[i],"wb")))goto done;}
    for(unsigned i=0;i<k;i++){db[i]=calloc(stripe,1);if(!db[i])goto done;}for(unsigned i=0;i<m;i++){pb[i]=calloc(stripe,1);if(!pb[i])goto done;}
    for(uint64_t off=0;off<ss;off+=stripe){size_t take=(size_t)((ss-off)<stripe?(ss-off):stripe);for(unsigned d=0;d<k;d++){memset(db[d],0,take);uint64_t pos=(uint64_t)d*ss+off;if(pos<len){size_t want=(size_t)((len-pos)<take?(len-pos):take);if(QRX_FSEEK64(in,pos,SEEK_SET)||fread(db[d],1,want,in)!=want)goto done;}if(fwrite(db[d],1,take,fo[d])!=take)goto done;}
        for(unsigned pr=0;pr<m;pr++){memset(pb[pr],0,take);unsigned row=k+pr;for(size_t b=0;b<take;b++){uint8_t z=0;for(unsigned d=0;d<k;d++)z^=gf_mul(g[(size_t)row*k+d],db[d][b]);pb[pr][b]=z;}if(fwrite(pb[pr],1,take,fo[k+pr])!=take)goto done;}}
    for(unsigned i=0;i<n;i++){if(fflush(fo[i])||fclose(fo[i])){fo[i]=NULL;goto done;}fo[i]=NULL;}if(orig_out)*orig_out=len;if(ss_out)*ss_out=ss;rc=0;
done:
    if(fo)for(unsigned i=0;i<n;i++)if(fo[i])fclose(fo[i]);if(rc&&outp)for(unsigned i=0;i<n;i++)if(outp[i])remove(outp[i]);if(db)for(unsigned i=0;i<k;i++)free(db[i]);if(pb)for(unsigned i=0;i<m;i++)free(pb[i]);free(db);free(pb);free(fo);free(g);fclose(in);return rc;
}
