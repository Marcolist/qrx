/* QRX 0.0.7.7 Phase 7.1 -- Privacy consensus state.
 *
 * This layer deliberately sits on top of the existing shielded cryptographic
 * primitives but moves authoritative state into QRXDB.  The old file backed
 * state remains a post-commit compatibility mirror only.
 *
 * Privacy transactions are normal VELOCITY transactions, therefore the outer
 * Ed25519+ML-DSA signature already binds network_id/genesis_hash/protocol_version,
 * sender, lane nonce, fee and expiry height.  The proof transcript below repeats
 * that binding so a proof bundle itself cannot be transplanted to another chain.
 */

#define QRX_PRIVACY_CONSENSUS_VERSION 1
#define QRX_PRIVACY_MAX_INPUTS 4
#define QRX_PRIVACY_MAX_OUTPUTS 2
#define QRX_PRIVACY_TX_MAX_BYTES 262144

static int privacy_is_mainnet(const char *chain_dir){
    char *n=chain_cfg_value(chain_dir,"network_id");
    int yes=n&&(strstr(n,"mainnet")||strstr(n,"Mainnet"));
    free(n);return yes;
}

static int privacy_binding_values(const char *chain_dir,char **chain_id,char **genesis,char **protocol){
    *chain_id=chain_cfg_value(chain_dir,"chain_id");
    *genesis=chain_cfg_value(chain_dir,"genesis_hash");
    *protocol=chain_cfg_value(chain_dir,"protocol_version");
    return (*chain_id&&*genesis&&*protocol)?0:-1;
}

static void privacy_transcript_prefix(const char *chain_dir,char out[1024]){
    char *cid=NULL,*gen=NULL,*pv=NULL;
    if(privacy_binding_values(chain_dir,&cid,&gen,&pv)!=0)die("privacy chain binding unavailable");
    snprintf(out,1024,"chain_id=%s|genesis=%s|protocol=%s",cid,gen,pv);
    free(cid);free(gen);free(pv);
}

static char *privacy_b64_decode_text(const char *b64){
    if(!b64||!*b64)return NULL;size_t n=0;unsigned char *raw=base64_decode(b64,&n);if(!raw)return NULL;
    char *s=malloc(n+1);if(!s){free(raw);return NULL;}memcpy(s,raw,n);s[n]=0;free(raw);return s;
}

static char *privacy_file_b64(const char *path){
    size_t n=0;char *txt=read_file(path,&n);if(!txt)return NULL;char *b=base64_encode((unsigned char*)txt,n);free(txt);return b;
}

static int privacy_write_b64_temp(const char *chain_dir,const char *tag,const char *b64,char path[1400]){
    if(!b64||strlen(b64)>200000)return -1;size_t n=0;unsigned char *raw=base64_decode(b64,&n);if(!raw)return -1;
    char h[129];sha3_512_hex_local(b64,h);char d[1100];snprintf(d,sizeof(d),"%s/privacy-verify-tmp",chain_dir);mkdir_p(d);
    snprintf(path,1400,"%s/%s-%ld-%.24s.proof",d,tag,(long)getpid(),h);
    FILE *f=fopen(path,"wb");if(!f){free(raw);return -1;}int rc=fwrite(raw,1,n,f)==n?0:-1;fclose(f);free(raw);return rc;
}

static void privacy_note_line(const ShieldedNoteRecord *r,char out[8192]){
    snprintf(out,8192,"%s|%s|%s|%s|%s|%s|%s|%s|%s|%lld",
        r->note_id,r->value_commitment,r->leaf_commitment,r->one_time_pub,r->ephemeral_pub,
        r->nonce_hex,r->ciphertext_b64,r->tag_hex,r->proof_hash,r->created_at);
}

static int privacy_note_from_b64(const char *b64,ShieldedNoteRecord *r){
    char *line=privacy_b64_decode_text(b64);if(!line)return -1;int rc=shielded_parse_line(line,r);free(line);return rc;
}

static int privacy_db_get(const char *chain_dir,const char *key,char *out,size_t outsz){
    QrxDB db;if(qrxdb_init(&db,chain_dir)!=0)return -1;int rc=qrxdb_get(&db,key,out,outsz);qrxdb_close(&db);return rc;
}

/* Phase 7.2 global KYC-provider registry. The registry is authoritative QRXDB
 * consensus state and can only be mutated by threshold-signed governance. */
static int kyc_provider_lookup_authoritative(const char *chain_dir,const char *provider,char *pub,size_t pubsz,char *authority,size_t authsz,char *caps,size_t capsz,int *active){
    if(!provider||!*provider)return -1;char k[512],v[2048];
    snprintf(k,sizeof(k),"kyc:provider:%s:status",provider);if(privacy_db_get(chain_dir,k,v,sizeof(v))!=0)return -1;if(active)*active=!strcmp(v,"ACTIVE");
    if(pub&&pubsz){snprintf(k,sizeof(k),"kyc:provider:%s:public_key_hex",provider);if(privacy_db_get(chain_dir,k,pub,pubsz)!=0)return -1;}
    if(authority&&authsz){snprintf(k,sizeof(k),"kyc:provider:%s:authority_address",provider);if(privacy_db_get(chain_dir,k,authority,authsz)!=0)return -1;}
    if(caps&&capsz){snprintf(k,sizeof(k),"kyc:provider:%s:capabilities",provider);if(privacy_db_get(chain_dir,k,caps,capsz)!=0)snprintf(caps,capsz,"-");}
    return 0;
}
static int kyc_provider_id_from_qualifier(const char *qualifier,char out[96]){
    if(!qualifier||strncmp(qualifier,"#KYC_",5))return -1;size_t i=0;const char*p=qualifier+5;while(*p&&*p!='/'&&i+1<96){unsigned char c=(unsigned char)*p;if(!(isalnum(c)||c=='_'||c=='.'||c=='-'))return -1;out[i++]=(char)c;p++;}out[i]=0;return i?0:-1;
}
static int kyc_provider_active_for_qualifier(const char *chain_dir,const char *qualifier,char *authority,size_t authsz){
    char id[96],pk[128],caps[2048];int active=0;if(kyc_provider_id_from_qualifier(qualifier,id)!=0)return 1; /* non-KYC qualifier */
    if(kyc_provider_lookup_authoritative(chain_dir,id,pk,sizeof(pk),authority,authsz,caps,sizeof(caps),&active)!=0||!active)return 0;
    return 1;
}

static int privacy_nullifier_exists_authoritative(const char *chain_dir,const char *ki){
    char k[256],v[256];snprintf(k,sizeof(k),"privacy:nullifier:%s",ki);return privacy_db_get(chain_dir,k,v,sizeof(v))==0;
}

static long long privacy_leaf_count(const char *chain_dir){
    char v[64];if(privacy_db_get(chain_dir,"privacy:merkle_count",v,sizeof(v))==0)return atoll(v);return 0;
}

typedef struct{char **leaves;size_t n,cap;} PrivacyLeafList;
static int privacy_leaf_cb(const char *key,const char *value,uint32_t value_len,void *ctx){
    (void)key;(void)value_len;PrivacyLeafList*l=ctx;if(l->n==l->cap){size_t nc=l->cap?l->cap*2:16;char **t=realloc(l->leaves,nc*sizeof(*t));if(!t)return -1;l->leaves=t;l->cap=nc;}l->leaves[l->n]=strdup(value);if(!l->leaves[l->n])return -1;l->n++;return 0;
}
static void privacy_leaf_list_free(PrivacyLeafList*l){for(size_t i=0;i<l->n;i++)free(l->leaves[i]);free(l->leaves);memset(l,0,sizeof(*l));}

static int privacy_load_leaves(const char *chain_dir,PrivacyLeafList *l){
    memset(l,0,sizeof(*l));QrxDB db;if(qrxdb_init(&db,chain_dir)!=0)return -1;int rc=qrxdb_scan_prefix(&db,"privacy:leaf:",privacy_leaf_cb,l);qrxdb_close(&db);return rc;
}

static int privacy_merkle_root_from_leaves(char **leaves,size_t n,char out[129]){
    if(!n){sha3_512_hex_local("QUB-SHIELDED-EMPTY-MERKLE-v1",out);return 0;}
    char (*a)[129]=calloc(n,129);if(!a)return -1;for(size_t i=0;i<n;i++){if(!leaves[i]||strlen(leaves[i])!=128){free(a);return -1;}snprintf(a[i],129,"%s",leaves[i]);}
    while(n>1){size_t m=(n+1)/2;for(size_t i=0;i<m;i++){char b[320];const char*x=a[2*i],*y=(2*i+1<n)?a[2*i+1]:a[2*i];snprintf(b,sizeof(b),"QUB-SHIELDED-MERKLE-NODE-v1|%s|%s",x,y);sha3_512_hex_local(b,a[i]);}n=m;}
    snprintf(out,129,"%s",a[0]);free(a);return 0;
}

static int privacy_merkle_root_authoritative(const char *chain_dir,char out[129]){
    PrivacyLeafList l;if(privacy_load_leaves(chain_dir,&l)!=0)return -1;int rc=privacy_merkle_root_from_leaves(l.leaves,l.n,out);privacy_leaf_list_free(&l);return rc;
}

typedef struct{ShieldedNoteRecord *a;size_t n,cap;const char *chain_dir;} PrivacyNoteList;
static int privacy_note_collect_cb(const char *key,const char *value,uint32_t value_len,void *ctx){
    (void)value_len;PrivacyNoteList*l=ctx;if(strstr(key,":range"))return 0;
    ShieldedNoteRecord r;if(shielded_parse_line(value,&r)!=0)return -1;
    if(l->n==l->cap){size_t nc=l->cap?l->cap*2:16;ShieldedNoteRecord*t=realloc(l->a,nc*sizeof(*t));if(!t)return -1;l->a=t;l->cap=nc;}l->a[l->n++]=r;return 0;
}
static int privacy_load_notes_authoritative(const char *chain_dir,ShieldedNoteRecord **out,size_t *n){
    PrivacyNoteList l={0};l.chain_dir=chain_dir;QrxDB db;if(qrxdb_init(&db,chain_dir)!=0)return -1;int rc=qrxdb_scan_prefix(&db,"privacy:note:",privacy_note_collect_cb,&l);qrxdb_close(&db);if(rc){free(l.a);return -1;}*out=l.a;*n=l.n;return 0;
}

static int privacy_attester_lookup_authoritative(const char *chain_dir,const char *issuer,char pubhex[65],int *active){
    if(!vp_valid_token(issuer))return -1;char k[512],v[256];snprintf(k,sizeof(k),"privacy:attester:%s:pubkey",issuer);
    if(privacy_db_get(chain_dir,k,v,sizeof(v))==0){snprintf(pubhex,65,"%.64s",v);snprintf(k,sizeof(k),"privacy:attester:%s:status",issuer);if(privacy_db_get(chain_dir,k,v,sizeof(v))!=0)return -1;*active=!strcmp(v,"ACTIVE");return 0;}
    /* Pre-genesis/test migration compatibility only. Mainnet consensus never
       trusts the mutable legacy registry. */
    if(privacy_is_mainnet(chain_dir))return -1;return vp_attester_lookup(chain_dir,issuer,pubhex,active);
}

static int privacy_credential_revoked_authoritative(const char *chain_dir,const char *serial_hash){
    char k[320],v[64];snprintf(k,sizeof(k),"privacy:revoked:%s",serial_hash);if(privacy_db_get(chain_dir,k,v,sizeof(v))==0)return !strcmp(v,"1");
    return privacy_is_mainnet(chain_dir)?0:vp_revoked(chain_dir,serial_hash);
}

static int privacy_verify_credential_text(const char *chain_dir,const char *from,const char *txt,char *why,size_t wsz){
    char *fmt=cfg_get(txt,"format"),*issuer=cfg_get(txt,"issuer"),*sub=cfg_get(txt,"subject_commitment"),*salt=cfg_get(txt,"subject_salt"),*sh=cfg_get(txt,"serial_hash"),*vf=cfg_get(txt,"valid_from_height"),*vu=cfg_get(txt,"valid_until_height"),*cid=cfg_get(txt,"chain_id"),*gen=cfg_get(txt,"genesis_hash"),*pv=cfg_get(txt,"protocol_version"),*pol=cfg_get(txt,"policy"),*sighex=cfg_get(txt,"signature_hex");
    int rc=-1;char *ecid=NULL,*egen=NULL,*epv=NULL;char pubhex[65];int active=0;
    if(!fmt||strcmp(fmt,"qrx-verified-privacy-v2")||!issuer||!sub||!salt||!sh||!vf||!vu||!cid||!gen||!pv||!pol||strcmp(pol,"hidden-balance-v2")||!sighex){snprintf(why,wsz,"credential-v2-malformed");goto done;}
    if(privacy_binding_values(chain_dir,&ecid,&egen,&epv)!=0||strcmp(cid,ecid)||strcmp(gen,egen)||strcmp(pv,epv)){snprintf(why,wsz,"credential-chain-binding-mismatch");goto done;}
    if(privacy_attester_lookup_authoritative(chain_dir,issuer,pubhex,&active)!=0||!active){snprintf(why,wsz,"issuer-not-consensus-active");goto done;}
    long long h=current_height_from_chain(chain_dir)+1,fromh=atoll(vf),until=atoll(vu);if(fromh<0||until<fromh||h<fromh||h>until){snprintf(why,wsz,"credential-height-invalid");goto done;}
    if(strlen(sh)!=128||privacy_credential_revoked_authoritative(chain_dir,sh)){snprintf(why,wsz,"credential-revoked-or-bad-serial");goto done;}
    char pre[4096],calc[129];snprintf(pre,sizeof(pre),"QUB-VERIFIED-PRIVACY-SUBJECT-v2|chain_id=%s|address=%s|salt=%s",cid,from,salt);sha3_512_hex_local(pre,calc);if(strcmp(calc,sub)){snprintf(why,wsz,"credential-subject-mismatch");goto done;}
    snprintf(pre,sizeof(pre),"QUB-VERIFIED-PRIVACY-CREDENTIAL-v2|chain_id=%s|genesis=%s|protocol=%s|issuer=%s|subject=%s|serial_hash=%s|valid_from_height=%lld|valid_until_height=%lld|policy=hidden-balance-v2",cid,gen,pv,issuer,sub,sh,fromh,until);
    unsigned char raw[32];if(hex_to_bytes_local(pubhex,raw,32)!=0||strlen(sighex)%2){snprintf(why,wsz,"credential-key-or-signature-invalid");goto done;}EVP_PKEY *pub=EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519,NULL,raw,32);size_t sl=strlen(sighex)/2;unsigned char *sig=malloc(sl);if(!pub||!sig||hex_to_bytes_local(sighex,sig,sl)!=0){if(pub)EVP_PKEY_free(pub);free(sig);snprintf(why,wsz,"credential-signature-malformed");goto done;}if(verify_oneshot(pub,(unsigned char*)pre,strlen(pre),sig,sl)!=0){EVP_PKEY_free(pub);free(sig);snprintf(why,wsz,"credential-signature-invalid");goto done;}EVP_PKEY_free(pub);free(sig);snprintf(why,wsz,"verified-v2");rc=0;
done:free(fmt);free(issuer);free(sub);free(salt);free(sh);free(vf);free(vu);free(cid);free(gen);free(pv);free(pol);free(sighex);free(ecid);free(egen);free(epv);return rc;
}

static int privacy_require_payload_credential(const char *chain_dir,const char *from,const char *payload){
    char *b=payload_get_field(payload,"credential_b64");if(!b)return -1;char *t=privacy_b64_decode_text(b);free(b);if(!t)return -1;char why[160];int rc=privacy_verify_credential_text(chain_dir,from,t,why,sizeof(why));free(t);if(rc)fprintf(stderr,"privacy credential rejected: %s\n",why);return rc;
}

static void privacy_range_context(const char *chain_dir,const ShieldedNoteRecord *r,char out[1600]){
    char bind[1024];privacy_transcript_prefix(chain_dir,bind);snprintf(out,1600,"QUB-PRIVACY-RANGE-v2|%s|leaf=%s|C=%s",bind,r->leaf_commitment,r->value_commitment);
}

static int privacy_create_bound_note(const char *chain_dir,const char *zaddr,long long value,const char *memo,ShieldedNoteRecord *r,ShieldedOwnedNote *w,char **proof_b64){
    ShieldedOwnedNote lw;memset(&lw,0,sizeof(lw));if(shielded_create_note(chain_dir,zaddr,value,memo,r,&lw)!=0)return -1;
    BIGNUM *blind=shielded_bn_from_hex(lw.blind_hex);if(!blind)return -1;char pdir[1024],path[1400],ctx[1600];shielded_proof_dir(chain_dir,pdir,sizeof(pdir));mkdir_p(pdir);snprintf(path,sizeof(path),"%s/%s.range",pdir,r->leaf_commitment);privacy_range_context(chain_dir,r,ctx);
    if(shielded_range_prove(value,blind,r->value_commitment,ctx,path)!=0||shielded_range_verify(r->value_commitment,ctx,path)!=0){BN_clear_free(blind);return -1;}BN_clear_free(blind);char *pf=read_file(path,NULL);if(!pf)return -1;sha3_512_hex_local(pf,r->proof_hash);free(pf);*proof_b64=privacy_file_b64(path);if(!*proof_b64)return -1;if(w)*w=lw;return 0;
}

static int privacy_verify_bound_note(const char *chain_dir,const ShieldedNoteRecord *r,const char *proof_b64){
    char leaf[129];shielded_leaf_hash(r,leaf);if(strcmp(leaf,r->leaf_commitment))return -1;char path[1400],ctx[1600];if(privacy_write_b64_temp(chain_dir,"range",proof_b64,path)!=0)return -1;privacy_range_context(chain_dir,r,ctx);int rc=shielded_range_verify(r->value_commitment,ctx,path);if(rc==0){char *pf=read_file(path,NULL);if(!pf)rc=-1;else{char h[129];sha3_512_hex_local(pf,h);if(strcmp(h,r->proof_hash))rc=-1;free(pf);}}unlink_qrx(path);return rc;
}

static int privacy_shield_link_verify(const ShieldedNoteRecord *r,long long amount,const char *ctx,const char *A,const char *c,const char *s){
    EC_GROUP*g=EC_GROUP_new_by_curve_name(NID_secp256k1);BN_CTX*b=BN_CTX_new();EC_POINT*C=g?shielded_point_from_hex(g,r->value_commitment,b):NULL,*VG=g?EC_POINT_new(g):NULL,*D=g?EC_POINT_new(g):NULL;BIGNUM*v=BN_new();int rc=-1;if(!g||!b||!C||!VG||!D||!v||amount<0)goto done;BN_set_word(v,(BN_ULONG)amount);if(EC_POINT_mul(g,VG,v,NULL,NULL,b)!=1)goto done;EC_POINT_invert(g,VG,b);EC_POINT_add(g,D,C,VG,b);rc=shielded_schnorr_verify_H(D,ctx,A,c,s);done:if(v)BN_free(v);if(D)EC_POINT_free(D);if(VG)EC_POINT_free(VG);if(C)EC_POINT_free(C);if(b)BN_CTX_free(b);if(g)EC_GROUP_free(g);return rc;
}

static int privacy_shield_link_prove(const ShieldedNoteRecord *r,const ShieldedOwnedNote *w,long long amount,const char *ctx,char A[67],char c[65],char s[65]){
    EC_GROUP*g=EC_GROUP_new_by_curve_name(NID_secp256k1);BN_CTX*b=BN_CTX_new();EC_POINT*C=g?shielded_point_from_hex(g,r->value_commitment,b):NULL,*VG=g?EC_POINT_new(g):NULL,*D=g?EC_POINT_new(g):NULL;BIGNUM*v=BN_new(),*blind=shielded_bn_from_hex(w->blind_hex);int rc=-1;if(!g||!b||!C||!VG||!D||!v||!blind||amount<0)goto done;BN_set_word(v,(BN_ULONG)amount);if(EC_POINT_mul(g,VG,v,NULL,NULL,b)!=1)goto done;EC_POINT_invert(g,VG,b);EC_POINT_add(g,D,C,VG,b);rc=shielded_schnorr_prove_H(D,blind,ctx,A,c,s);done:if(blind)BN_clear_free(blind);if(v)BN_free(v);if(D)EC_POINT_free(D);if(VG)EC_POINT_free(VG);if(C)EC_POINT_free(C);if(b)BN_CTX_free(b);if(g)EC_GROUP_free(g);return rc;
}

static int privacy_collect_owned_authoritative(const char *chain_dir,const char *wallet_dir,long long needed,ShieldedNoteRecord *arr,size_t n,size_t **idx_out,ShieldedOwnedNote **w_out,size_t *cnt,long long *sum){
    size_t *idx=calloc(n?n:1,sizeof(*idx));ShieldedOwnedNote *w=calloc(n?n:1,sizeof(*w));if(!idx||!w){free(idx);free(w);return -1;}size_t c=0;long long total=0;
    for(size_t i=0;i<n&&total<needed;i++){ShieldedOwnedNote x;if(!shielded_note_decrypt(&arr[i],wallet_dir,&x))continue;char ki[67];if(!x.one_time_priv||shielded_key_image(arr[i].one_time_pub,x.one_time_priv,ki)!=0||privacy_nullifier_exists_authoritative(chain_dir,ki)){if(x.one_time_priv)BN_clear_free(x.one_time_priv);continue;}idx[c]=i;w[c]=x;checked_add_ll(total,x.value,"privacy selected input total",&total);c++;if(c>=QRX_PRIVACY_MAX_INPUTS)break;}
    if(total<needed){for(size_t i=0;i<c;i++)if(w[i].one_time_priv)BN_clear_free(w[i].one_time_priv);free(idx);free(w);return -2;}*idx_out=idx;*w_out=w;*cnt=c;*sum=total;return 0;
}

static char *privacy_wallet_credential_b64(const char *wallet_dir){char p[1024];vp_cred_path(wallet_dir,p,sizeof(p));size_t n=0;char *t=read_file(p,&n);if(!t)return NULL;char*b=base64_encode((unsigned char*)t,n);free(t);return b;}

static int privacy_payload_append(char **buf,size_t *cap,size_t *len,const char *fmt,...){
    va_list ap;while(1){va_start(ap,fmt);int n=vsnprintf(*buf+*len,*cap-*len,fmt,ap);va_end(ap);if(n<0)return -1;if((size_t)n<*cap-*len){*len+=(size_t)n;return 0;}size_t nc=*cap*2+(size_t)n+1024;if(nc>QRX_PRIVACY_TX_MAX_BYTES*2)return -1;char*t=realloc(*buf,nc);if(!t)return -1;*buf=t;*cap=nc;}
}

static int privacy_prepare_shield_payload(const char *chain_dir,const char *wallet_dir,long long amount,const char *dest,char **payload_out){
    char z[512];if(!dest||strncmp(dest,"zqub2",5)){if(shielded_make_address_from_wallet(wallet_dir,z)!=0)return -1;dest=z;}ShieldedNoteRecord r;ShieldedOwnedNote w;char *rp=NULL;if(privacy_create_bound_note(chain_dir,dest,amount,"consensus-shield",&r,&w,&rp)!=0)return -1;
    char bind[1024],linkctx[1800],A[67],c[65],ss[65];privacy_transcript_prefix(chain_dir,bind);snprintf(linkctx,sizeof(linkctx),"QUB-PRIVACY-SHIELD-LINK-v1|%s|leaf=%s|amount=%lld",bind,r.leaf_commitment,amount);if(privacy_shield_link_prove(&r,&w,amount,linkctx,A,c,ss)!=0){free(rp);return -1;}
    char line[8192];privacy_note_line(&r,line);char *nb=base64_encode((unsigned char*)line,strlen(line)),*cred=privacy_wallet_credential_b64(wallet_dir);if(!nb||!cred){free(nb);free(cred);free(rp);return -1;}size_t cap=65536+strlen(rp),len=0;char *p=calloc(cap,1);privacy_payload_append(&p,&cap,&len,"privacy_version=1;credential_b64=%s;output_count=1;note0_b64=%s;range0_b64=%s;shield_A=%s;shield_c=%s;shield_s=%s",cred,nb,rp,A,c,ss);free(nb);free(cred);free(rp);*payload_out=p;return 0;
}

static int privacy_build_spend_payload(const char *chain_dir,const char *wallet_dir,const char *to,long long amount,int unshield,char **payload_out){
    ShieldedNoteRecord *arr=NULL;size_t n=0;if(privacy_load_notes_authoritative(chain_dir,&arr,&n)!=0||!n){free(arr);return -1;}
    size_t *idx=NULL,cnt=0;ShieldedOwnedNote *w=NULL;long long sum=0;if(privacy_collect_owned_authoritative(chain_dir,wallet_dir,amount,arr,n,&idx,&w,&cnt,&sum)!=0){free(arr);return -1;}
    char selfz[512];if(shielded_make_address_from_wallet(wallet_dir,selfz)!=0)goto fail;size_t outc=unshield?(sum>amount?1:0):(sum>amount?2:1);if(outc>QRX_PRIVACY_MAX_OUTPUTS)goto fail;ShieldedNoteRecord outs[2];ShieldedOwnedNote ow[2];char *ranges[2]={0};memset(ow,0,sizeof(ow));
    if(unshield){if(outc&&privacy_create_bound_note(chain_dir,selfz,sum-amount,"consensus-unshield-change",&outs[0],&ow[0],&ranges[0])!=0)goto fail;}
    else{if(!to||strncmp(to,"zqub2",5))goto fail;if(privacy_create_bound_note(chain_dir,to,amount,"consensus-private-payment",&outs[0],&ow[0],&ranges[0])!=0)goto fail;if(outc==2&&privacy_create_bound_note(chain_dir,selfz,sum-amount,"consensus-private-change",&outs[1],&ow[1],&ranges[1])!=0)goto fail;}
    char root[129];if(privacy_merkle_root_authoritative(chain_dir,root)!=0)goto fail;char bind[1024],ctx[4096];privacy_transcript_prefix(chain_dir,bind);
    if(unshield)snprintf(ctx,sizeof(ctx),"QUB-PRIVACY-UNSHIELD-v2|%s|root=%s|to=%s|public_amount=%lld|change=%s",bind,root,to,amount,outc?outs[0].leaf_commitment:"-");
    else snprintf(ctx,sizeof(ctx),"QUB-PRIVACY-TRANSFER-v2|%s|root=%s|out0=%s|out1=%s|outputs=%zu",bind,root,outs[0].leaf_commitment,outc==2?outs[1].leaf_commitment:"-",outc);
    BIGNUM **pbl=NULL;char **pcom=NULL;if(shielded_prepare_pseudo_blinds(w,cnt,ow,outc,&pbl,&pcom)!=0)goto fail;if(shielded_global_balance_equation((const char**)pcom,cnt,outs,outc,unshield?amount:0)!=0){shielded_free_pseudo(cnt,pbl,pcom);goto fail;}
    char **ringb=calloc(cnt,sizeof(*ringb)),**kis=calloc(cnt,sizeof(*kis));if(!ringb||!kis){shielded_free_pseudo(cnt,pbl,pcom);free(ringb);free(kis);goto fail;}char pdir[1024];shielded_proof_dir(chain_dir,pdir,sizeof(pdir));mkdir_p(pdir);
    for(size_t i=0;i<cnt;i++){char path[1400],rid[65],vk[67],vp[67];shielded_random_hex(rid,16);snprintf(path,sizeof(path),"%s/consensus-ring-%s.proof",pdir,rid);kis[i]=calloc(67,1);if(!kis[i]||shielded_ring_prove(arr,n,idx[i],&w[i],pbl[i],pcom[i],ctx,path,kis[i])!=0||shielded_ring_verify(arr,n,ctx,path,vk,vp)!=0||strcmp(vk,kis[i])||strcmp(vp,pcom[i])||privacy_nullifier_exists_authoritative(chain_dir,kis[i])){unlink_qrx(path);goto ringfail;}ringb[i]=privacy_file_b64(path);unlink_qrx(path);if(!ringb[i])goto ringfail;}
    {char *cred=privacy_wallet_credential_b64(wallet_dir);if(!cred)goto ringfail;size_t cap=65536,len=0;for(size_t i=0;i<outc;i++)cap+=strlen(ranges[i]?ranges[i]:"")+12000;for(size_t i=0;i<cnt;i++)cap+=strlen(ringb[i])+512;char *p=calloc(cap,1);privacy_payload_append(&p,&cap,&len,"privacy_version=1;credential_b64=%s;root_before=%s;input_count=%zu;output_count=%zu",cred,root,cnt,outc);free(cred);for(size_t i=0;i<cnt;i++)privacy_payload_append(&p,&cap,&len,";key_image%zu=%s;pseudo%zu=%s;ring%zu_b64=%s",i,kis[i],i,pcom[i],i,ringb[i]);for(size_t i=0;i<outc;i++){char line[8192];privacy_note_line(&outs[i],line);char *nb=base64_encode((unsigned char*)line,strlen(line));privacy_payload_append(&p,&cap,&len,";note%zu_b64=%s;range%zu_b64=%s",i,nb,i,ranges[i]);free(nb);}*payload_out=p;}
    for(size_t i=0;i<cnt;i++){free(ringb[i]);free(kis[i]);}free(ringb);free(kis);shielded_free_pseudo(cnt,pbl,pcom);for(size_t i=0;i<cnt;i++)if(w[i].one_time_priv)BN_clear_free(w[i].one_time_priv);for(size_t i=0;i<outc;i++)free(ranges[i]);free(w);free(idx);free(arr);return 0;
ringfail:for(size_t i=0;i<cnt;i++){free(ringb?ringb[i]:NULL);free(kis?kis[i]:NULL);}free(ringb);free(kis);shielded_free_pseudo(cnt,pbl,pcom);
fail:for(size_t i=0;i<cnt;i++)if(w&&w[i].one_time_priv)BN_clear_free(w[i].one_time_priv);for(size_t i=0;i<2;i++)free(ranges[i]);free(w);free(idx);free(arr);return -1;
}

static int privacy_prepare_payload_cmd(const char *chain_dir,const char *wallet_dir,const char *action,long long amount,const char *destination){
    char *p=NULL;int rc=-1;if(!strcmp(action,"SHIELD"))rc=privacy_prepare_shield_payload(chain_dir,wallet_dir,amount,destination,&p);else if(!strcmp(action,"TRANSFER"))rc=privacy_build_spend_payload(chain_dir,wallet_dir,destination,amount,0,&p);else if(!strcmp(action,"UNSHIELD"))rc=privacy_build_spend_payload(chain_dir,wallet_dir,destination,amount,1,&p);else die("unknown privacy consensus action");if(rc||!p)die("privacy consensus proof preparation failed");if(strlen(p)>QRX_PRIVACY_TX_MAX_BYTES-8192){free(p);die("privacy proof bundle exceeds consensus transaction ceiling");}printf("payload=%s\n",p);printf("payload_bytes=%zu\n",strlen(p));printf("consensus=true\nchain_bound_proofs=true\nverified_privacy_required=true\n");free(p);return 0;
}

static int privacy_parse_outputs_verify(const char *chain_dir,const char *payload,ShieldedNoteRecord outs[QRX_PRIVACY_MAX_OUTPUTS],size_t outc){
    for(size_t i=0;i<outc;i++){char nk[64],rk[64];snprintf(nk,sizeof(nk),"note%zu_b64",i);snprintf(rk,sizeof(rk),"range%zu_b64",i);char *nb=payload_get_field(payload,nk),*rb=payload_get_field(payload,rk);if(!nb||!rb||privacy_note_from_b64(nb,&outs[i])!=0||privacy_verify_bound_note(chain_dir,&outs[i],rb)!=0){free(nb);free(rb);return -1;}free(nb);free(rb);}return 0;
}

static int validate_privacy_spend_bundle(const char *chain_dir,const char *tx_type,const char *from,const char *to,long long amount,const char *payload){
    (void)from;
    char *root=NULL,*ics=NULL,*ocs=NULL;
    ShieldedNoteRecord *arr=NULL,outs[QRX_PRIVACY_MAX_OUTPUTS];
    size_t n=0; long long ic=0,oc=0; int rc=-1; const char *why="unknown";
    char *owned[QRX_PRIVACY_MAX_INPUTS]={0}; const char *pseudos[QRX_PRIVACY_MAX_INPUTS]={0};
    char seen[QRX_PRIVACY_MAX_INPUTS][67]; memset(seen,0,sizeof(seen)); memset(outs,0,sizeof(outs));
    root=payload_get_field(payload,"root_before"); ics=payload_get_field(payload,"input_count"); ocs=payload_get_field(payload,"output_count");
    if(!root||!ics||!ocs){why="missing-root-or-count";goto done;}
    ic=atoll(ics); oc=atoll(ocs);
    if(ic<1||ic>QRX_PRIVACY_MAX_INPUTS||oc<0||oc>QRX_PRIVACY_MAX_OUTPUTS){why="invalid-input-output-count";goto done;}
    char curroot[129]; if(privacy_merkle_root_authoritative(chain_dir,curroot)!=0||strcmp(root,curroot)){why="merkle-root-mismatch";goto done;}
    if(privacy_load_notes_authoritative(chain_dir,&arr,&n)!=0||!n){why="authoritative-note-set-unavailable";goto done;}
    if(privacy_parse_outputs_verify(chain_dir,payload,outs,(size_t)oc)!=0){why="output-note-or-range-proof-invalid";goto done;}
    char bind[1024],ctx[4096]; privacy_transcript_prefix(chain_dir,bind);
    if(!strcmp(tx_type,"PRIVACY_TRANSFER")){
        if(amount!=0||oc<1){why="transfer-public-amount-or-output-invalid";goto done;}
        snprintf(ctx,sizeof(ctx),"QUB-PRIVACY-TRANSFER-v2|%s|root=%s|out0=%s|out1=%s|outputs=%lld",bind,root,outs[0].leaf_commitment,oc==2?outs[1].leaf_commitment:"-",oc);
    }else{
        if(strcmp(tx_type,"PRIVACY_UNSHIELD")||amount<=0||strncmp(to,"qrx",3)){why="unshield-public-output-invalid";goto done;}
        snprintf(ctx,sizeof(ctx),"QUB-PRIVACY-UNSHIELD-v2|%s|root=%s|to=%s|public_amount=%lld|change=%s",bind,root,to,amount,oc?outs[0].leaf_commitment:"-");
    }
    for(long long i=0;i<ic;i++){
        char kk[64],pk[64],rk[64]; snprintf(kk,sizeof(kk),"key_image%lld",i); snprintf(pk,sizeof(pk),"pseudo%lld",i); snprintf(rk,sizeof(rk),"ring%lld_b64",i);
        char *ki=payload_get_field(payload,kk),*ps=payload_get_field(payload,pk),*rb=payload_get_field(payload,rk);
        if(!ki||!ps||!rb||strlen(ki)!=66||strlen(ps)!=66){free(ki);free(ps);free(rb);why="malformed-ring-fields";goto done;}
        if(privacy_nullifier_exists_authoritative(chain_dir,ki)){free(ki);free(ps);free(rb);why="key-image-already-spent";goto done;}
        for(long long j=0;j<i;j++)if(!strcmp(seen[j],ki)){free(ki);free(ps);free(rb);why="duplicate-key-image";goto done;}
        snprintf(seen[i],67,"%s",ki);
        char path[1400]={0},vki[67],vps[67];
        if(privacy_write_b64_temp(chain_dir,"ring",rb,path)!=0){free(ki);free(ps);free(rb);why="ring-proof-decode-failed";goto done;}
        int vr=shielded_ring_verify(arr,n,ctx,path,vki,vps); unlink_qrx(path);
        if(vr!=0||strcmp(vki,ki)||strcmp(vps,ps)){free(ki);free(ps);free(rb);why="ring-proof-invalid";goto done;}
        owned[i]=ps; pseudos[i]=owned[i]; free(ki); free(rb);
    }
    if(shielded_global_balance_equation(pseudos,(size_t)ic,outs,(size_t)oc,!strcmp(tx_type,"PRIVACY_UNSHIELD")?amount:0)!=0){why="global-balance-equation-invalid";goto done;}
    rc=0; why="verified";
done:
    for(long long i=0;i<QRX_PRIVACY_MAX_INPUTS;i++)free(owned[i]);
    free(arr); free(root); free(ics); free(ocs);
    if(rc!=0&&getenv("QRX_PRIVACY_DEBUG"))fprintf(stderr,"privacy spend bundle rejected: %s\n",why);
    return rc;
}

static int validate_privacy_governance_tx(const char *chain_dir,const char *payload){
    char *pb=payload_get_field(payload,"proposal_b64"),*sc=payload_get_field(payload,"signature_count");if(!pb||!sc){free(pb);free(sc);return -1;}char *pt=privacy_b64_decode_text(pb);long long n=atoll(sc),threshold=gov_threshold(chain_dir);free(pb);free(sc);if(!pt||threshold<=0||n<threshold||n>16){free(pt);return -1;}
    char *fmt=cfg_get(pt,"format"),*cid=cfg_get(pt,"chain_id"),*gen=cfg_get(pt,"genesis_hash"),*pv=cfg_get(pt,"protocol_version"),*action=cfg_get(pt,"action"),*nonce=cfg_get(pt,"nonce");char *ecid=NULL,*egen=NULL,*epv=NULL;if(!fmt||strcmp(fmt,"qrx-governance-proposal-v2")||!cid||!gen||!pv||!action||!nonce||privacy_binding_values(chain_dir,&ecid,&egen,&epv)||strcmp(cid,ecid)||strcmp(gen,egen)||strcmp(pv,epv)){free(fmt);free(cid);free(gen);free(pv);free(action);free(nonce);free(ecid);free(egen);free(epv);free(pt);return -1;}free(ecid);free(egen);free(epv);
    char ph[129];gov_hash_text("QUB-GOVERNANCE-PROPOSAL-v2",pt,ph);char replayk[320],rv[64];snprintf(replayk,sizeof(replayk),"privacy:governance:applied:%s",ph);if(privacy_db_get(chain_dir,replayk,rv,sizeof(rv))==0){free(fmt);free(cid);free(gen);free(pv);free(action);free(nonce);free(pt);return -1;}
    char ids[16][128];int valid=0;for(long long i=0;i<n;i++){char sk[64];snprintf(sk,sizeof(sk),"signature%lld_b64",i);char *sb=payload_get_field(payload,sk);char *st=privacy_b64_decode_text(sb);free(sb);if(!st)continue;char *f=cfg_get(st,"format"),*id=cfg_get(st,"key_id"),*sph=cfg_get(st,"proposal_hash"),*sighex=cfg_get(st,"signature_hex");char pk[65];int ok=0;if(f&&!strcmp(f,"qrx-governance-signature-v2")&&id&&sph&&!strcmp(sph,ph)&&sighex&&gov_root_lookup(chain_dir,id,pk)==0&&strlen(sighex)%2==0){unsigned char raw[32];if(hex_to_bytes_local(pk,raw,32)==0){EVP_PKEY*pub=EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519,NULL,raw,32);size_t sl=strlen(sighex)/2;unsigned char*sig=malloc(sl);if(pub&&sig&&hex_to_bytes_local(sighex,sig,sl)==0&&verify_oneshot(pub,(unsigned char*)pt,strlen(pt),sig,sl)==0)ok=1;if(pub)EVP_PKEY_free(pub);free(sig);}}if(ok){int dup=0;for(int j=0;j<valid;j++)if(!strcmp(ids[j],id))dup=1;if(!dup&&valid<16)snprintf(ids[valid++],128,"%s",id);}free(f);free(id);free(sph);free(sighex);free(st);}
    int rc=valid>=threshold?0:-1;free(fmt);free(cid);free(gen);free(pv);free(action);free(nonce);free(pt);return rc;
}

static int validate_privacy_consensus_tx(const char *chain_dir,const char *tx_type,const char *from,const char *to,const char *amount_s,const char *payload){
    if(!tx_type||!from||!to||!amount_s||!payload||strlen(payload)>QRX_PRIVACY_TX_MAX_BYTES)return -1;long long amount=parse_nonnegative_ll_strict(amount_s,"privacy amount");char *v=payload_get_field(payload,"privacy_version");if(!v||atoi(v)!=QRX_PRIVACY_CONSENSUS_VERSION){free(v);return -1;}free(v);
    if(!strcmp(tx_type,"PRIVACY_GOVERNANCE"))return amount==0?validate_privacy_governance_tx(chain_dir,payload):-1;
    if(privacy_require_payload_credential(chain_dir,from,payload)!=0)return -1;
    if(!strcmp(tx_type,"PRIVACY_SHIELD")){if(amount<=0||strcmp(from,to))return -1;char *oc=payload_get_field(payload,"output_count"),*nb=payload_get_field(payload,"note0_b64"),*rb=payload_get_field(payload,"range0_b64"),*A=payload_get_field(payload,"shield_A"),*c=payload_get_field(payload,"shield_c"),*s=payload_get_field(payload,"shield_s");ShieldedNoteRecord r;int rc=-1;if(oc&&atoll(oc)==1&&nb&&rb&&A&&c&&s&&privacy_note_from_b64(nb,&r)==0&&privacy_verify_bound_note(chain_dir,&r,rb)==0){char bind[1024],ctx[1800];privacy_transcript_prefix(chain_dir,bind);snprintf(ctx,sizeof(ctx),"QUB-PRIVACY-SHIELD-LINK-v1|%s|leaf=%s|amount=%lld",bind,r.leaf_commitment,amount);rc=privacy_shield_link_verify(&r,amount,ctx,A,c,s);}free(oc);free(nb);free(rb);free(A);free(c);free(s);return rc;}
    if(!strcmp(tx_type,"PRIVACY_TRANSFER")||!strcmp(tx_type,"PRIVACY_UNSHIELD"))return validate_privacy_spend_bundle(chain_dir,tx_type,from,to,amount,payload);
    return -1;
}

static int privacy_batch_append_outputs(QrxDBBatch *b,const char *chain_dir,const char *payload,size_t outc,char newroot[129]){
    PrivacyLeafList leaves;if(privacy_load_leaves(chain_dir,&leaves)!=0)return -1;long long base=(long long)leaves.n;int rc=0;for(size_t i=0;i<outc;i++){char nk[64],rk[64];snprintf(nk,sizeof(nk),"note%zu_b64",i);snprintf(rk,sizeof(rk),"range%zu_b64",i);char *nb=payload_get_field(payload,nk),*rb=payload_get_field(payload,rk);ShieldedNoteRecord r;if(!nb||!rb||privacy_note_from_b64(nb,&r)!=0){free(nb);free(rb);privacy_leaf_list_free(&leaves);return -1;}char k[512],line[8192];privacy_note_line(&r,line);snprintf(k,sizeof(k),"privacy:note:%s",r.leaf_commitment);rc|=qrxdb_batch_put(b,k,line);snprintf(k,sizeof(k),"privacy:range:%s",r.leaf_commitment);rc|=qrxdb_batch_put(b,k,rb);snprintf(k,sizeof(k),"privacy:leaf:%020lld",base+(long long)i);rc|=qrxdb_batch_put(b,k,r.leaf_commitment);char **t=realloc(leaves.leaves,(leaves.n+1)*sizeof(*t));if(!t){free(nb);free(rb);privacy_leaf_list_free(&leaves);return -1;}leaves.leaves=t;leaves.leaves[leaves.n++]=strdup(r.leaf_commitment);free(nb);free(rb);}
    if(privacy_merkle_root_from_leaves(leaves.leaves,leaves.n,newroot)!=0){privacy_leaf_list_free(&leaves);return -1;}rc|=velocity_batch_put_ll(b,"privacy:merkle_count",(long long)leaves.n);rc|=qrxdb_batch_put(b,"privacy:merkle_root",newroot);privacy_leaf_list_free(&leaves);return rc?-1:0;
}

static int atomic_stage_privacy_governance(QrxDBBatch *b,const char *chain_dir,const char *payload,const char *txid,long long height){
    char *pb=payload_get_field(payload,"proposal_b64");char *pt=privacy_b64_decode_text(pb);free(pb);if(!pt)return -1;char ph[129];gov_hash_text("QUB-GOVERNANCE-PROPOSAL-v2",pt,ph);char *action=cfg_get(pt,"action");int rc=0;if(!action){free(pt);return -1;}
    if(!strcmp(action,"ATTESTER_ADD")||!strcmp(action,"ATTESTER_ROTATE_KEY")||!strcmp(action,"ATTESTER_DISABLE")){char *issuer=cfg_get(pt,"issuer"),*pk=cfg_get(pt,"public_key_hex"),*ah=cfg_get(pt,"activation_height");if(!issuer||!pk||!ah||height<atoll(ah)){free(issuer);free(pk);free(ah);free(action);free(pt);return -1;}char k[512],existing[65]={0};int active=0;if(!strcmp(action,"ATTESTER_DISABLE")&&privacy_attester_lookup_authoritative(chain_dir,issuer,existing,&active)!=0){free(issuer);free(pk);free(ah);free(action);free(pt);return -1;}snprintf(k,sizeof(k),"privacy:attester:%s:pubkey",issuer);rc|=qrxdb_batch_put(b,k,!strcmp(action,"ATTESTER_DISABLE")?existing:pk);snprintf(k,sizeof(k),"privacy:attester:%s:status",issuer);rc|=qrxdb_batch_put(b,k,!strcmp(action,"ATTESTER_DISABLE")?"DISABLED":"ACTIVE");snprintf(k,sizeof(k),"privacy:attester:%s:last_tx",issuer);rc|=qrxdb_batch_put(b,k,txid);free(issuer);free(pk);free(ah);}
    else if(!strcmp(action,"KYC_PROVIDER_ADD")||!strcmp(action,"KYC_PROVIDER_DISABLE")||!strcmp(action,"KYC_PROVIDER_ROTATE_KEY")||!strcmp(action,"KYC_PROVIDER_UPDATE")){
        char *id=cfg_get(pt,"provider_id"),*pk=cfg_get(pt,"public_key_hex"),*auth=cfg_get(pt,"authority_address"),*caps=cfg_get(pt,"capabilities"),*ah=cfg_get(pt,"activation_height");
        if(!id||!ah||!vp_valid_token(id)||height<atoll(ah)){free(id);free(pk);free(auth);free(caps);free(ah);free(action);free(pt);return -1;}
        char oldpk[128]={0},oldauth[512]={0},oldcaps[2048]={0};int oldactive=0,exists=kyc_provider_lookup_authoritative(chain_dir,id,oldpk,sizeof(oldpk),oldauth,sizeof(oldauth),oldcaps,sizeof(oldcaps),&oldactive)==0;
        if(!strcmp(action,"KYC_PROVIDER_ADD")&&exists){free(id);free(pk);free(auth);free(caps);free(ah);free(action);free(pt);return -1;}
        if(strcmp(action,"KYC_PROVIDER_ADD")&&!exists){free(id);free(pk);free(auth);free(caps);free(ah);free(action);free(pt);return -1;}
        const char *newpk=oldpk,*newauth=oldauth,*newcaps=oldcaps,*status="ACTIVE";
        if(!strcmp(action,"KYC_PROVIDER_DISABLE")){status="DISABLED";}
        else {
            if((!pk||strlen(pk)!=64)&&(!strcmp(action,"KYC_PROVIDER_ADD")||!strcmp(action,"KYC_PROVIDER_ROTATE_KEY"))){free(id);free(pk);free(auth);free(caps);free(ah);free(action);free(pt);return -1;}
            if(pk&&strcmp(pk,"-")){for(size_t z=0;z<64;z++)if(!isxdigit((unsigned char)pk[z])){free(id);free(pk);free(auth);free(caps);free(ah);free(action);free(pt);return -1;}newpk=pk;}
            if((!auth||strncmp(auth,"qrx1",4))&&(!strcmp(action,"KYC_PROVIDER_ADD"))){free(id);free(pk);free(auth);free(caps);free(ah);free(action);free(pt);return -1;}
            if(auth&&strcmp(auth,"-")&&strncmp(auth,"qrx1",4)==0)newauth=auth;
            if(caps&&strcmp(caps,"-"))newcaps=caps;
        }
        if(!newpk||strlen(newpk)!=64||!newauth||strncmp(newauth,"qrx1",4)){free(id);free(pk);free(auth);free(caps);free(ah);free(action);free(pt);return -1;}
        char k[512];snprintf(k,sizeof(k),"kyc:provider:%s:public_key_hex",id);rc|=qrxdb_batch_put(b,k,newpk);snprintf(k,sizeof(k),"kyc:provider:%s:authority_address",id);rc|=qrxdb_batch_put(b,k,newauth);snprintf(k,sizeof(k),"kyc:provider:%s:capabilities",id);rc|=qrxdb_batch_put(b,k,newcaps&&*newcaps?newcaps:"KYC");snprintf(k,sizeof(k),"kyc:provider:%s:status",id);rc|=qrxdb_batch_put(b,k,status);snprintf(k,sizeof(k),"kyc:provider:%s:activation_height",id);char hv[64];snprintf(hv,sizeof(hv),"%lld",height);rc|=qrxdb_batch_put(b,k,hv);snprintf(k,sizeof(k),"kyc:provider:%s:last_tx",id);rc|=qrxdb_batch_put(b,k,txid);
        free(id);free(pk);free(auth);free(caps);free(ah);
    }
    else if(!strcmp(action,"CREDENTIAL_REVOKE")){char *sh=cfg_get(pt,"serial_hash");if(!sh||strlen(sh)!=128){free(sh);free(action);free(pt);return -1;}char k[320];snprintf(k,sizeof(k),"privacy:revoked:%s",sh);rc|=qrxdb_batch_put(b,k,"1");free(sh);}
    else if(!strcmp(action,"PRIVACY_PROTOCOL_UPDATE")){char *ver=cfg_get(pt,"privacy_protocol_version"),*ah=cfg_get(pt,"activation_height");if(!ver||!ah||height<atoll(ah)){free(ver);free(ah);free(action);free(pt);return -1;}rc|=qrxdb_batch_put(b,"privacy:protocol_version",ver);free(ver);free(ah);}
    else{free(action);free(pt);return -1;}
    char k[320];snprintf(k,sizeof(k),"privacy:governance:applied:%s",ph);rc|=qrxdb_batch_put(b,k,txid);rc|=qrxdb_batch_put(b,"privacy:governance:last_tx",txid);free(action);free(pt);return rc?-1:0;
}

static int atomic_stage_privacy(QrxDBBatch *b,const char *chain_dir,const char *from,const char *to,const char *tx_type,const char *payload,const char *txid,long long height,long long amount){
    if(!strcmp(tx_type,"PRIVACY_GOVERNANCE"))return atomic_stage_privacy_governance(b,chain_dir,payload,txid,height);
    char *ocs=payload_get_field(payload,"output_count");size_t outc=ocs?(size_t)atoll(ocs):0;free(ocs);int rc=0;char newroot[129];if(outc&&privacy_batch_append_outputs(b,chain_dir,payload,outc,newroot)!=0)return -1;if(!outc){if(privacy_merkle_root_authoritative(chain_dir,newroot)!=0)return -1;}
    if(strcmp(tx_type,"PRIVACY_SHIELD")){char *ics=payload_get_field(payload,"input_count");long long ic=ics?atoll(ics):0;free(ics);for(long long i=0;i<ic;i++){char kk[64];snprintf(kk,sizeof(kk),"key_image%lld",i);char *ki=payload_get_field(payload,kk);if(!ki)return -1;char k[256];snprintf(k,sizeof(k),"privacy:nullifier:%s",ki);rc|=qrxdb_batch_put(b,k,txid);free(ki);}}
    char k[512];snprintf(k,sizeof(k),"privacy:tx:%s:type",txid);rc|=qrxdb_batch_put(b,k,tx_type);snprintf(k,sizeof(k),"privacy:tx:%s:from",txid);rc|=qrxdb_batch_put(b,k,from);snprintf(k,sizeof(k),"privacy:tx:%s:to",txid);rc|=qrxdb_batch_put(b,k,to);snprintf(k,sizeof(k),"privacy:tx:%s:height",txid);rc|=velocity_batch_put_ll(b,k,height);snprintf(k,sizeof(k),"privacy:tx:%s:public_amount",txid);rc|=velocity_batch_put_ll(b,k,amount);snprintf(k,sizeof(k),"privacy:tx:%s:merkle_root",txid);rc|=qrxdb_batch_put(b,k,newroot);return rc?-1:0;
}

static int privacy_consensus_balance_cmd(const char *chain_dir,const char *wallet_dir){
    ShieldedNoteRecord *arr=NULL;size_t n=0;if(privacy_load_notes_authoritative(chain_dir,&arr,&n)!=0)die("privacy authoritative state unavailable");long long total=0;for(size_t i=0;i<n;i++){ShieldedOwnedNote w;if(shielded_note_decrypt(&arr[i],wallet_dir,&w)){char ki[67];int spent=!w.one_time_priv||shielded_key_image(arr[i].one_time_pub,w.one_time_priv,ki)!=0||privacy_nullifier_exists_authoritative(chain_dir,ki);if(!spent&&w.value>0)checked_add_ll(total,w.value,"privacy wallet balance",&total);if(w.one_time_priv)BN_clear_free(w.one_time_priv);}}free(arr);printf("%lld\n",total);return 0;
}

static int privacy_mirror_authoritative(const char *chain_dir){
    ShieldedNoteRecord *arr=NULL;size_t n=0;if(privacy_load_notes_authoritative(chain_dir,&arr,&n)!=0)return -1;if(shielded_save_all(chain_dir,arr,n)!=0){free(arr);return -1;}char mp[1024];shielded_merkle_path(chain_dir,mp,sizeof(mp));FILE *mf=fopen(mp,"wb");if(!mf){free(arr);return -1;}PrivacyLeafList l;if(privacy_load_leaves(chain_dir,&l)!=0){fclose(mf);free(arr);return -1;}for(size_t i=0;i<l.n;i++)fprintf(mf,"%s\n",l.leaves[i]);fclose(mf);privacy_leaf_list_free(&l);char pdir[1024];shielded_proof_dir(chain_dir,pdir,sizeof(pdir));mkdir_p(pdir);QrxDB db;if(qrxdb_init(&db,chain_dir)==0){for(size_t i=0;i<n;i++){char k[512],b64[200000];snprintf(k,sizeof(k),"privacy:range:%s",arr[i].leaf_commitment);if(qrxdb_get(&db,k,b64,sizeof(b64))==0){size_t z=0;unsigned char *raw=base64_decode(b64,&z);if(raw){char pp[1400];snprintf(pp,sizeof(pp),"%s/%s.range",pdir,arr[i].leaf_commitment);FILE*f=fopen(pp,"wb");if(f){fwrite(raw,1,z,f);fclose(f);}free(raw);}}}qrxdb_close(&db);}free(arr);return 0;
}

static int privacy_credential_issue_v2_cmd(const char *chain_dir,const char *wallet_dir,const char *issuer,const char *privpem,long long until_height){
    long long h=current_height_from_chain(chain_dir)+1;if(until_height<h)die("credential valid-until height must be in the future");char pkhex[65];int active=0;if(privacy_attester_lookup_authoritative(chain_dir,issuer,pkhex,&active)!=0||!active)die("issuer is not active in consensus privacy registry");EVP_PKEY *priv=load_priv_pem(privpem,"");if(!priv){char pass[256];if(gov_read_passphrase(pass,sizeof(pass),"QRX_ATTESTER_PASSPHRASE",0)!=0)die("attester passphrase failed");priv=load_priv_pem(privpem,pass);OPENSSL_cleanse(pass,sizeof(pass));}if(!priv||EVP_PKEY_id(priv)!=EVP_PKEY_ED25519)die("attester private key must be Ed25519");unsigned char raw[32];size_t rl=32;char got[65];if(EVP_PKEY_get_raw_public_key(priv,raw,&rl)!=1)die("attester private key unreadable");bytes_to_hex_local(raw,32,got);if(strcmp(got,pkhex))die("attester key does not match consensus registry");char *addr=wallet_address(wallet_dir);if(!addr)die("wallet address unavailable");addr[strcspn(addr,"\r\n")]=0;char *cid=NULL,*gen=NULL,*pv=NULL;if(privacy_binding_values(chain_dir,&cid,&gen,&pv)!=0)die("chain binding unavailable");char salt[65],serial[65],sh[129],sub[129],pre[4096];shielded_random_hex(salt,32);shielded_random_hex(serial,32);snprintf(pre,sizeof(pre),"QUB-VERIFIED-PRIVACY-SERIAL-v2|chain_id=%s|serial=%s",cid,serial);sha3_512_hex_local(pre,sh);snprintf(pre,sizeof(pre),"QUB-VERIFIED-PRIVACY-SUBJECT-v2|chain_id=%s|address=%s|salt=%s",cid,addr,salt);sha3_512_hex_local(pre,sub);snprintf(pre,sizeof(pre),"QUB-VERIFIED-PRIVACY-CREDENTIAL-v2|chain_id=%s|genesis=%s|protocol=%s|issuer=%s|subject=%s|serial_hash=%s|valid_from_height=%lld|valid_until_height=%lld|policy=hidden-balance-v2",cid,gen,pv,issuer,sub,sh,h,until_height);unsigned char *sig=NULL;size_t sl=0;if(sign_oneshot(priv,(unsigned char*)pre,strlen(pre),&sig,&sl)!=0)die("credential signing failed");char *sighex=malloc(sl*2+1);bytes_to_hex_local(sig,sl,sighex);char path[1024];vp_cred_path(wallet_dir,path,sizeof(path));FILE*f=fopen(path,"wb");if(!f)die("credential write failed");fprintf(f,"format=qrx-verified-privacy-v2\nissuer=%s\nsubject_commitment=%s\nsubject_salt=%s\nserial_hash=%s\nvalid_from_height=%lld\nvalid_until_height=%lld\nchain_id=%s\ngenesis_hash=%s\nprotocol_version=%s\npolicy=hidden-balance-v2\nsignature_hex=%s\n",issuer,sub,salt,sh,h,until_height,cid,gen,pv,sighex);fclose(f);OPENSSL_cleanse(serial,sizeof(serial));OPENSSL_cleanse(sig,sl);free(sig);free(sighex);EVP_PKEY_free(priv);free(addr);free(cid);free(gen);free(pv);printf("status=issued-v2\nvalid_from_height=%lld\nvalid_until_height=%lld\nserial_hash=%s\nchain_bound=true\n",h,until_height,sh);return 0;
}

/* Consensus governance proposal/signature tooling. Existing v1 governance
 * commands remain for pre-mainnet migration, while these v2 artifacts are the
 * only ones accepted inside PRIVACY_GOVERNANCE. */
static int privacy_gov_propose_v2_cmd(const char *chain_dir,const char *out,const char *action,const char *issuer_or_serial,const char *pub_or_version,long long activation_height){
    char *cid=NULL,*gen=NULL,*pv=NULL;if(privacy_binding_values(chain_dir,&cid,&gen,&pv)!=0)die("chain binding unavailable");char nonce[65];shielded_random_hex(nonce,32);char b[4096];if(!strcmp(action,"ATTESTER_ADD")||!strcmp(action,"ATTESTER_ROTATE_KEY")||!strcmp(action,"ATTESTER_DISABLE"))snprintf(b,sizeof(b),"format=qrx-governance-proposal-v2\nchain_id=%s\ngenesis_hash=%s\nprotocol_version=%s\naction=%s\nissuer=%s\npublic_key_hex=%s\nactivation_height=%lld\nnonce=%s\n",cid,gen,pv,action,issuer_or_serial,pub_or_version?pub_or_version:"-",activation_height,nonce);else if(!strcmp(action,"CREDENTIAL_REVOKE"))snprintf(b,sizeof(b),"format=qrx-governance-proposal-v2\nchain_id=%s\ngenesis_hash=%s\nprotocol_version=%s\naction=CREDENTIAL_REVOKE\nserial_hash=%s\nactivation_height=%lld\nnonce=%s\n",cid,gen,pv,issuer_or_serial,activation_height,nonce);else if(!strcmp(action,"PRIVACY_PROTOCOL_UPDATE"))snprintf(b,sizeof(b),"format=qrx-governance-proposal-v2\nchain_id=%s\ngenesis_hash=%s\nprotocol_version=%s\naction=PRIVACY_PROTOCOL_UPDATE\nprivacy_protocol_version=%s\nactivation_height=%lld\nnonce=%s\n",cid,gen,pv,pub_or_version?pub_or_version:"1",activation_height,nonce);else die("unsupported privacy governance v2 action");write_text(out,b);char ph[129];gov_hash_text("QUB-GOVERNANCE-PROPOSAL-v2",b,ph);printf("status=proposed-v2\nproposal_hash=%s\nproposal_file=%s\n",ph,out);free(cid);free(gen);free(pv);return 0;
}

static int kyc_provider_propose_v2_cmd(const char *chain_dir,const char *out,const char *action,const char *provider,const char *authority,const char *pubkey,const char *caps,long long activation_height){
    if(strcmp(action,"KYC_PROVIDER_ADD")&&strcmp(action,"KYC_PROVIDER_DISABLE")&&strcmp(action,"KYC_PROVIDER_ROTATE_KEY")&&strcmp(action,"KYC_PROVIDER_UPDATE"))die("unsupported KYC provider governance action");
    if(!vp_valid_token(provider))die("invalid provider id");
    if(!strcmp(action,"KYC_PROVIDER_ADD")&&(!authority||strncmp(authority,"qrx1",4)))die("KYC provider ADD requires QRX authority address");
    if((!strcmp(action,"KYC_PROVIDER_ADD")||!strcmp(action,"KYC_PROVIDER_ROTATE_KEY"))&&(!pubkey||strlen(pubkey)!=64))die("KYC provider action requires 64-hex Ed25519 public key");
    char *cid=NULL,*gen=NULL,*pv=NULL;if(privacy_binding_values(chain_dir,&cid,&gen,&pv)!=0)die("chain binding unavailable");char nonce[65];shielded_random_hex(nonce,32);char b[6144];
    snprintf(b,sizeof(b),"format=qrx-governance-proposal-v2\nchain_id=%s\ngenesis_hash=%s\nprotocol_version=%s\naction=%s\nprovider_id=%s\nauthority_address=%s\npublic_key_hex=%s\ncapabilities=%s\nactivation_height=%lld\nnonce=%s\n",cid,gen,pv,action,provider,authority&&*authority?authority:"-",pubkey&&*pubkey?pubkey:"-",caps&&*caps?caps:"KYC",activation_height,nonce);
    write_text(out,b);char ph[129];gov_hash_text("QUB-GOVERNANCE-PROPOSAL-v2",b,ph);printf("status=proposed-v2\naction=%s\nprovider_id=%s\nproposal_hash=%s\nproposal_file=%s\n",action,provider,ph,out);free(cid);free(gen);free(pv);return 0;
}

static int kyc_provider_info_cmd(const char *chain_dir,const char *provider){
    char pk[128],auth[512],caps[2048];int active=0;if(kyc_provider_lookup_authoritative(chain_dir,provider,pk,sizeof(pk),auth,sizeof(auth),caps,sizeof(caps),&active)!=0)die("KYC provider not found");
    printf("provider_id=%s\nstatus=%s\nauthority_address=%s\npublic_key_hex=%s\ncapabilities=%s\nqualifier=#KYC_%s\nconsensus_registry=true\ngovernance=developer_threshold\n",provider,active?"ACTIVE":"DISABLED",auth,pk,caps,provider);return 0;
}

static int privacy_gov_sign_v2_cmd(const char *chain_dir,const char *proposal,const char *keydir,const char *out){
    char *pt=read_file(proposal,NULL);if(!pt)die("proposal missing");char *fmt=cfg_get(pt,"format");if(!fmt||strcmp(fmt,"qrx-governance-proposal-v2"))die("not a v2 proposal");free(fmt);char ph[129];gov_hash_text("QUB-GOVERNANCE-PROPOSAL-v2",pt,ph);char desc[1024],privp[1024],id[128],pk[65],fp[33],rootpk[65];snprintf(desc,sizeof(desc),"%s/governance.pub",keydir);snprintf(privp,sizeof(privp),"%s/governance.key",keydir);if(gov_descriptor(desc,id,pk,fp)!=0||gov_root_lookup(chain_dir,id,rootpk)!=0)die("key is not a genesis governance root");char pass[256];if(gov_read_passphrase(pass,sizeof(pass),"QRX_GOV_PASSPHRASE",0)!=0)die("passphrase failed");EVP_PKEY*priv=load_priv_pem(privp,pass);OPENSSL_cleanse(pass,sizeof(pass));if(!priv)die("governance key unlock failed");unsigned char *sig=NULL;size_t sl=0;if(sign_oneshot(priv,(unsigned char*)pt,strlen(pt),&sig,&sl)!=0)die("governance signature failed");char *sh=malloc(sl*2+1);bytes_to_hex_local(sig,sl,sh);char o[4096];snprintf(o,sizeof(o),"format=qrx-governance-signature-v2\nkey_id=%s\nproposal_hash=%s\nsignature_hex=%s\n",id,ph,sh);write_text(out,o);OPENSSL_cleanse(sig,sl);free(sig);free(sh);free(pt);EVP_PKEY_free(priv);printf("status=signed-v2\nkey_id=%s\nproposal_hash=%s\n",id,ph);return 0;
}

static int privacy_gov_payload_v2_cmd(const char *proposal,int argc,char **sigs){
    char *pt=read_file(proposal,NULL);if(!pt)die("proposal missing");char *pb=base64_encode((unsigned char*)pt,strlen(pt));free(pt);if(!pb)die("proposal encode failed");size_t cap=16384,len=0;char *p=calloc(cap,1);privacy_payload_append(&p,&cap,&len,"privacy_version=1;proposal_b64=%s;signature_count=%d",pb,argc);free(pb);for(int i=0;i<argc;i++){char *st=read_file(sigs[i],NULL);if(!st)die("signature file missing");char *sb=base64_encode((unsigned char*)st,strlen(st));free(st);privacy_payload_append(&p,&cap,&len,";signature%d_b64=%s",i,sb);free(sb);}printf("payload=%s\n",p);free(p);return 0;
}
