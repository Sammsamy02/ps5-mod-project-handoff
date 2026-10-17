/* Throwaway host TLS-only experiment; real checked host getentropy, never deploy. */
#include "tls_effective.h"
#include "fixture_root.h"
#include "official_root.h"
#include "experiment_identity.h"
#include <arpa/inet.h>
#include <sys/random.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#define EXP_FAIL (-0x7100)
#define TOTAL_NS INT64_C(20000000000)
#define CLOCK_LIMIT 4096u
#define TLS_LIMIT 512u
#define BIO_LIMIT 512u
#define POLL_LIMIT 256u
#define RNG_LIMIT 96u
#define RNG_BYTES_LIMIT 6144u
static const int suites[]={MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,0};
static const uint16_t groups[]={23,24,0};
static const uint16_t sigs[]={0x0403,0x0503,0x0401,0};
static const mbedtls_x509_crt_profile profile={
    MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA256)|MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA384),
    MBEDTLS_X509_ID_FLAG(MBEDTLS_PK_ECKEY)|MBEDTLS_X509_ID_FLAG(MBEDTLS_PK_ECDSA)|MBEDTLS_X509_ID_FLAG(MBEDTLS_PK_RSA),
    MBEDTLS_X509_ID_FLAG(MBEDTLS_ECP_DP_SECP256R1)|MBEDTLS_X509_ID_FLAG(MBEDTLS_ECP_DP_SECP384R1),2048
};
struct operation {
    mbedtls_entropy_context entropy; mbedtls_ctr_drbg_context drbg;
    mbedtls_ssl_config conf; mbedtls_ssl_context ssl; mbedtls_x509_crt root;
    int initialized, fd, in_library, source_closed, clock_failed, mode;
    int handshake_rc, seed_rc, add_rc, close_rc, setup_rc, last_tls_rc;
    const char *phase,*reason,*clock_reason;
    unsigned clock_attempts, wall_calls, injected_reads, library_reads, utc_calls, cert_utc_calls;
    unsigned clock_fail_read, source_calls, rng_calls, rng_bytes, tls_calls, bio_calls, polls;
    unsigned wire_send,wire_recv,request_bytes,response_bytes,closes;
    time_t anchor,last_wall; int have_anchor; int64_t anchor_a,anchor_b,last_mono,last_a,last_b;
    int clock_errno; int clean_peer,body_accepted;
    int version_number,ciphersuite_id,descriptor_close_rc; unsigned peer_cert_count;
    unsigned char staging[32],output[256];
};
static struct operation *owner;
static void wipe(void *p,size_t n){volatile unsigned char *q=p;while(n--)*q++=0;}
static void fail(struct operation *o,const char *reason){if(!o->reason)o->reason=reason;}
static void clock_fail(struct operation *o,const char *reason){if(!o->clock_failed){o->clock_reason=reason;o->clock_fail_read=o->library_reads;}o->clock_failed=1;fail(o,reason);}
static int mono(struct operation *o,int64_t *out){
    struct timespec t;errno=0;int rc=clock_gettime(CLOCK_MONOTONIC,&t);int e=errno;
    if(rc||e||t.tv_sec<0||t.tv_sec>INT64_MAX/INT64_C(1000000000)||t.tv_nsec<0||t.tv_nsec>=1000000000){clock_fail(o,"monotonic_unavailable");return -1;}
    int64_t v=(int64_t)t.tv_sec*INT64_C(1000000000)+t.tv_nsec;
    if(v<o->last_mono){clock_fail(o,"monotonic_backward");return -1;}
    o->last_mono=v;*out=v;return 0;
}
static time_t sample(struct operation *o,int library){
    if(!o||o->clock_failed)return (time_t)-1;
    if(o->clock_attempts>=CLOCK_LIMIT){clock_fail(o,"clock_budget");return (time_t)-1;}
    o->clock_attempts++;
    int64_t a,b;if(mono(o,&a))return (time_t)-1;
    errno=0;time_t w;int e;
    if(library&&o->mode&&o->library_reads==3){
        o->injected_reads++;
        if(o->mode==1){w=(time_t)-1;e=0;}
        else {w=time(NULL);e=errno;o->wall_calls++;if(o->mode==2)e=EIO;else if(o->mode==3)w=o->last_wall-1;else if(o->mode==4)w+=600;}
    }else{w=time(NULL);e=errno;o->wall_calls++;}
    o->clock_errno=e;
    if(mono(o,&b))return (time_t)-1;
    o->last_a=a;o->last_b=b;
    if(w==(time_t)-1){clock_fail(o,"time_unavailable");return (time_t)-1;}
    if(e){clock_fail(o,"time_errno_nonzero");return (time_t)-1;}
    if(b-a>INT64_C(100000000)){clock_fail(o,"clock_sample_late");return (time_t)-1;}
    if(o->have_anchor){
        if(w<o->last_wall){clock_fail(o,"time_backward");return (time_t)-1;}
        int64_t lo=(a-o->anchor_b)/INT64_C(1000000000);
        int64_t hi=(b-o->anchor_a)/INT64_C(1000000000)+((b-o->anchor_a)%INT64_C(1000000000)!=0);
        if(w<o->anchor+lo||w>o->anchor+hi){clock_fail(o,"time_changed");return (time_t)-1;}
        if(b-o->anchor_b>TOTAL_NS){clock_fail(o,"operation_deadline");return (time_t)-1;}
    }else{o->anchor=w;o->anchor_a=a;o->anchor_b=b;o->have_anchor=1;}
    o->last_wall=w;return w;
}
time_t exp_clock_time(time_t *out){
    struct operation *o=owner;time_t w=(time_t)-1;
    if(o){o->library_reads++;w=sample(o,1);}
    if(out)*out=w;return w;
}
struct tm *mbedtls_platform_gmtime_r(const mbedtls_time_t *tt,struct tm *buf){
    struct operation *o=owner;
    if(o){o->utc_calls++;if(o->in_library)o->cert_utc_calls++;}
    if(!o||!tt||!buf||o->clock_failed||*tt==(mbedtls_time_t)-1){if(buf)wipe(buf,sizeof(*buf));return NULL;}
    errno=0;struct tm *p=gmtime(tt);int e=errno;
    if(!p||e){clock_fail(o,"utc_unavailable");wipe(buf,sizeof(*buf));return NULL;}
    struct tm t=*p;
    if(t.tm_year< -1899||t.tm_year>8099||t.tm_mon<0||t.tm_mon>11||t.tm_mday<1||t.tm_mday>31||t.tm_hour<0||t.tm_hour>23||t.tm_min<0||t.tm_min>59||t.tm_sec<0||t.tm_sec>59){clock_fail(o,"utc_invalid");wipe(buf,sizeof(*buf));return NULL;}
    *buf=t;return buf;
}
static int guard(struct operation *o){return !o->clock_failed&&sample(o,0)!=(time_t)-1;}
static int utc_guard(struct operation *o){
    mbedtls_x509_time t;time_t w=sample(o,0);
    return w!=(time_t)-1&&mbedtls_x509_time_gmtime(w,&t)==0;
}
static int source(void *p,unsigned char *out,size_t cap,size_t *olen){
    struct operation *o=p;if(olen)*olen=0;
    if(!o||!out||!olen||cap<32||cap>128)return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
    if(o->source_closed||o->source_calls>=2){wipe(out,cap);fail(o,"post_seed_entropy_denied");return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;}
    o->source_calls++;errno=0;int rc=getentropy(o->staging,32);int e=errno;
    if(rc||e){wipe(o->staging,sizeof(o->staging));wipe(out,cap);fail(o,"host_getentropy_failed");return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;}
    memcpy(out,o->staging,32);*olen=32;wipe(o->staging,32);return 0;
}
static int rng(void *p,unsigned char *out,size_t n){
    struct operation *o=p;
    if(!out||!n||n>64)return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;
    if(!guard(o)||o->rng_calls>=RNG_LIMIT||n>RNG_BYTES_LIMIT-o->rng_bytes){wipe(out,n);fail(o,"rng_policy");return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;}
    o->rng_calls++;o->rng_bytes+=(unsigned)n;
    int rc=mbedtls_ctr_drbg_random(&o->drbg,out,n);
    if(rc){wipe(out,n);fail(o,"rng_failed");}return rc;
}
static int bio_send(void *p,const unsigned char *buf,size_t n){
    struct operation *o=p;
    if(!guard(o)||o->bio_calls>=BIO_LIMIT||n>32768u-o->wire_send){fail(o,"send_policy");return EXP_FAIL;}
    o->bio_calls++;errno=0;ssize_t rc=send(o->fd,buf,n,0);int e=errno;
    if(rc>0)o->wire_send+=(unsigned)rc;
    if(!guard(o))return EXP_FAIL;
    if(rc>0)return (int)rc;
    if(rc<0&&(e==EINTR||e==EAGAIN||e==EWOULDBLOCK))return MBEDTLS_ERR_SSL_WANT_WRITE;
    fail(o,"send_failed");return EXP_FAIL;
}
static int bio_recv(void *p,unsigned char *buf,size_t n){
    struct operation *o=p;
    if(!guard(o)||o->bio_calls>=BIO_LIMIT||n>65536u-o->wire_recv){fail(o,"recv_policy");return EXP_FAIL;}
    o->bio_calls++;errno=0;ssize_t rc=recv(o->fd,buf,n,0);int e=errno;
    if(rc>0)o->wire_recv+=(unsigned)rc;
    if(!guard(o))return EXP_FAIL;
    if(rc>=0)return (int)rc;
    if(e==EINTR||e==EAGAIN||e==EWOULDBLOCK)return MBEDTLS_ERR_SSL_WANT_READ;
    fail(o,"recv_failed");return EXP_FAIL;
}
static int wait_ready(struct operation *o,int want,int64_t until){
    if(!guard(o)||o->polls>=POLL_LIMIT||o->last_b>=until){fail(o,"poll_limit_or_deadline");return -1;}
    int64_t remain=until-o->last_b;int ms=remain<INT64_C(100000000)?(int)(remain/1000000):100;
    if(ms<1)ms=1;struct pollfd p={o->fd,want==MBEDTLS_ERR_SSL_WANT_WRITE?POLLOUT:POLLIN,0};
    o->polls++;errno=0;int rc=poll(&p,1,ms);int e=errno;
    if(!guard(o))return -1;
    if(rc<0&&e!=EINTR){fail(o,"poll_failed");return -1;}return rc>0?1:0;
}
static int step(struct operation *o,int action,const unsigned char *w,size_t n,unsigned char *r){
    if(!guard(o)||o->tls_calls>=TLS_LIMIT){fail(o,"tls_entry_limit");return EXP_FAIL;}
    o->tls_calls++;o->in_library=1;int rc;
    if(action==0)rc=mbedtls_ssl_handshake(&o->ssl);
    else if(action==2)rc=mbedtls_ssl_read(&o->ssl,r,n);
    else if(action==3)rc=mbedtls_ssl_close_notify(&o->ssl);
    else rc=EXP_FAIL;
    o->in_library=0;o->last_tls_rc=rc;
    if(!guard(o))return EXP_FAIL;return rc;
}
static int connect_once(struct operation *o,const char *ip,unsigned port){
    o->phase="connect";if(!guard(o))return -1;
    int fd=socket(AF_INET,SOCK_STREAM,0);if(fd<0){fail(o,"socket_failed");return -1;}o->fd=fd;
#ifdef SO_NOSIGPIPE
    int one=1;if(setsockopt(fd,SOL_SOCKET,SO_NOSIGPIPE,&one,sizeof(one))<0){fail(o,"socket_option_failed");return -1;}
#endif
    int flags=fcntl(fd,F_GETFL,0);if(flags<0||fcntl(fd,F_SETFL,flags|O_NONBLOCK)<0){fail(o,"nonblocking_failed");return -1;}
    struct sockaddr_in a;memset(&a,0,sizeof(a));a.sin_family=AF_INET;a.sin_port=htons((uint16_t)port);if(inet_pton(AF_INET,ip,&a.sin_addr)!=1){fail(o,"numeric_ipv4_required");return -1;}
    errno=0;int rc=connect(fd,(struct sockaddr *)&a,sizeof(a));int e=errno;
    if(!guard(o))return -1;
    if(rc<0&&e!=EINPROGRESS){fail(o,"connect_failed");return -1;}
    if(rc<0){int64_t until=o->last_b+INT64_C(3000000000);
        for(;;){int ready=wait_ready(o,MBEDTLS_ERR_SSL_WANT_WRITE,until);if(ready<0)return -1;if(!ready)continue;int err=0;socklen_t len=sizeof(err);
            if(getsockopt(fd,SOL_SOCKET,SO_ERROR,&err,&len)||err){fail(o,"connect_result_failed");return -1;}
            break;
        }
    }return 0;
}
static void nullable(int v){if(v==INT_MAX)printf("null");else printf("%d",v);}

int main(int argc,char **argv){
    if(argc!=5){fprintf(stderr,"usage: tls_client hostname numeric_ipv4 port public|fixture\n");return 2;}
    const char *name=argv[1];int fixture=!strcmp(argv[4],"fixture");
    if(fixture){if(strcmp(argv[2],"127.0.0.1")||(strcmp(name,"nexus-mbed.fixture.test")&&strcmp(name,"wrong.nexus-mbed.fixture.test")))return 2;}
    else if(strcmp(argv[4],"public")||(strcmp(name,"users.nexusmods.com")&&strcmp(name,"api.nexusmods.com"))||strcmp(argv[3],"443"))return 2;
    char *end=NULL;long port=strtol(argv[3],&end,10);if(!end||*end||port<1||port>65535)return 2;
    struct operation o;memset(&o,0,sizeof(o));o.fd=-1;o.phase="clock_precheck";
    o.handshake_rc=o.seed_rc=o.add_rc=o.close_rc=o.setup_rc=o.descriptor_close_rc=INT_MAX;
    if(owner)return 2;owner=&o;
    mbedtls_entropy_init(&o.entropy);mbedtls_ctr_drbg_init(&o.drbg);mbedtls_ssl_config_init(&o.conf);mbedtls_ssl_init(&o.ssl);mbedtls_x509_crt_init(&o.root);o.initialized=1;
    if(!utc_guard(&o))goto cleanup;
    o.phase="root_parse";
    const unsigned char *ca=fixture?fixture_root_der:official_root_der;
    size_t ca_len=fixture?sizeof(fixture_root_der):sizeof(official_root_der);
    int rc=mbedtls_x509_crt_parse_der(&o.root,ca,ca_len);
    if(rc||o.root.next||o.root.raw.len!=ca_len){fail(&o,"root_parse_failed");goto cleanup;}
    o.phase="seed";o.add_rc=mbedtls_entropy_add_source(&o.entropy,source,&o,64,MBEDTLS_ENTROPY_SOURCE_STRONG);
    if(o.add_rc){fail(&o,"add_source_failed");goto cleanup;}
    static const unsigned char personal[]="owned-host-nexus-mbed-profile-v1";
    o.seed_rc=mbedtls_ctr_drbg_seed(&o.drbg,mbedtls_entropy_func,&o.entropy,personal,sizeof(personal)-1);o.source_closed=1;
    if(o.seed_rc||o.source_calls!=2){fail(&o,"seed_failed");goto cleanup;}
    mbedtls_ctr_drbg_set_prediction_resistance(&o.drbg,MBEDTLS_CTR_DRBG_PR_OFF);
    mbedtls_ctr_drbg_set_reseed_interval(&o.drbg,RNG_LIMIT);
    o.phase="setup";o.setup_rc=mbedtls_ssl_config_defaults(&o.conf,MBEDTLS_SSL_IS_CLIENT,MBEDTLS_SSL_TRANSPORT_STREAM,MBEDTLS_SSL_PRESET_DEFAULT);
    if(o.setup_rc){fail(&o,"config_defaults_failed");goto cleanup;}
    mbedtls_ssl_conf_authmode(&o.conf,MBEDTLS_SSL_VERIFY_REQUIRED);mbedtls_ssl_conf_rng(&o.conf,rng,&o);
    mbedtls_ssl_conf_ca_chain(&o.conf,&o.root,NULL);mbedtls_ssl_conf_cert_profile(&o.conf,&profile);
    mbedtls_ssl_conf_min_tls_version(&o.conf,MBEDTLS_SSL_VERSION_TLS1_2);mbedtls_ssl_conf_max_tls_version(&o.conf,MBEDTLS_SSL_VERSION_TLS1_2);
    mbedtls_ssl_conf_ciphersuites(&o.conf,suites);mbedtls_ssl_conf_groups(&o.conf,groups);mbedtls_ssl_conf_sig_algs(&o.conf,sigs);
    o.setup_rc=mbedtls_ssl_setup(&o.ssl,&o.conf);if(o.setup_rc){fail(&o,"ssl_setup_failed");goto cleanup;}
    o.setup_rc=mbedtls_ssl_set_hostname(&o.ssl,name);if(o.setup_rc){fail(&o,"hostname_setup_failed");goto cleanup;}
    if(connect_once(&o,argv[2],(unsigned)port))goto cleanup;
    mbedtls_ssl_set_bio(&o.ssl,&o,bio_send,bio_recv,NULL);o.phase="handshake";
    for(;;){rc=step(&o,0,NULL,0,NULL);o.handshake_rc=o.last_tls_rc;
        if(rc==0)break;
        if(rc!=MBEDTLS_ERR_SSL_WANT_READ&&rc!=MBEDTLS_ERR_SSL_WANT_WRITE){fail(&o,"handshake_failed");goto cleanup;}
        if(wait_ready(&o,rc,o.anchor_b+TOTAL_NS)<0)goto cleanup;
    }
    o.phase="verify_gate";
    o.version_number=mbedtls_ssl_get_version_number(&o.ssl);o.ciphersuite_id=mbedtls_ssl_get_ciphersuite_id_from_ssl(&o.ssl);
    if(!utc_guard(&o)||mbedtls_ssl_get_verify_result(&o.ssl)!=0||o.version_number!=MBEDTLS_SSL_VERSION_TLS1_2||o.ciphersuite_id!=suites[0]){fail(&o,"verify_gate_failed");goto cleanup;}
    const mbedtls_x509_crt *peer=mbedtls_ssl_get_peer_cert(&o.ssl);
    while(peer){if(o.peer_cert_count>=8){fail(&o,"peer_chain_count_limit");goto cleanup;}o.peer_cert_count++;peer=peer->next;}
    if(!o.peer_cert_count||!o.cert_utc_calls){fail(&o,"peer_dates_not_checked");goto cleanup;}
    o.body_accepted=1;o.phase="close";
    int64_t until=o.last_b+INT64_C(2000000000);if(until>o.anchor_b+TOTAL_NS)until=o.anchor_b+TOTAL_NS;
    for(;;){rc=step(&o,3,NULL,0,NULL);o.close_rc=o.last_tls_rc;if(rc==0)break;
        if(rc!=MBEDTLS_ERR_SSL_WANT_READ&&rc!=MBEDTLS_ERR_SSL_WANT_WRITE){fail(&o,"close_notify_failed");goto cleanup;}
        if(wait_ready(&o,rc,until)<0)goto cleanup;
    }
    for(;;){rc=step(&o,2,NULL,sizeof(o.output),o.output);
        if(rc==MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY){o.clean_peer=1;break;}
        if(rc>0){o.response_bytes+=(unsigned)rc;fail(&o,"unexpected_application_data");goto cleanup;}
        if(rc!=MBEDTLS_ERR_SSL_WANT_READ&&rc!=MBEDTLS_ERR_SSL_WANT_WRITE){fail(&o,"peer_close_notify_missing");goto cleanup;}
        if(wait_ready(&o,rc,until)<0)goto cleanup;
    }
    if(!utc_guard(&o))goto cleanup;o.phase="complete";
cleanup:
    ;uint32_t verify=mbedtls_ssl_get_verify_result(&o.ssl);
    struct operation result;memset(&result,0,sizeof(result));
    /* Only scalar receipts leave the owner; no contexts, random data or TLS bytes. */
#define COPY(x) result.x=o.x
    COPY(handshake_rc);COPY(seed_rc);COPY(add_rc);COPY(close_rc);COPY(setup_rc);COPY(reason);COPY(phase);COPY(clock_reason);COPY(clock_failed);COPY(clock_errno);
    COPY(source_calls);COPY(source_closed);COPY(rng_calls);COPY(rng_bytes);COPY(tls_calls);COPY(bio_calls);COPY(polls);COPY(wire_send);COPY(wire_recv);COPY(response_bytes);COPY(clean_peer);COPY(body_accepted);
    COPY(clock_attempts);COPY(library_reads);COPY(cert_utc_calls);COPY(version_number);COPY(ciphersuite_id);COPY(peer_cert_count);COPY(descriptor_close_rc);
#undef COPY
    mbedtls_ssl_free(&o.ssl);mbedtls_ssl_config_free(&o.conf);mbedtls_x509_crt_free(&o.root);mbedtls_ctr_drbg_free(&o.drbg);mbedtls_entropy_free(&o.entropy);
    if(o.fd>=0){result.closes++;result.descriptor_close_rc=close(o.fd);o.fd=-1;if(result.descriptor_close_rc)fail(&result,"descriptor_close_failed");}
    owner=NULL;wipe(&o,sizeof(o));int zero=1;for(size_t i=0;i<sizeof(o);i++)if(((const unsigned char *)&o)[i])zero=0;
    int ok=result.body_accepted&&result.clean_peer&&!result.reason&&result.close_rc==0&&result.descriptor_close_rc==0&&zero;
    printf("{\"status\":\"%s\",\"config_id\":\"%s\",\"trust_mode\":\"%s\",\"hostname\":\"%s\",\"numeric_ip\":\"%s\",\"reason\":",ok?"ok":"rejected",EXP_CONFIG_ID,fixture?"owned_fixture":"official_x1",name,argv[2]);
    if(result.reason)printf("\"%s\"",result.reason);else printf("null");
    printf(",\"phase\":\"%s\",\"add_source_rc\":",result.phase);nullable(result.add_rc);printf(",\"seed_rc\":");nullable(result.seed_rc);printf(",\"setup_rc\":");nullable(result.setup_rc);printf(",\"handshake_rc\":");nullable(result.handshake_rc);printf(",\"close_rc\":");nullable(result.close_rc);
    printf(",\"verify_flags\":%" PRIu32 ",\"handshake_accepted\":%s,\"clean_peer_close\":%s,\"version_number\":%d,\"ciphersuite_id\":%d,\"peer_supplied_certificate_count\":%u,\"source_calls\":%u,\"source_closed\":%s,\"rng_calls\":%u,\"rng_requested_bytes\":%u,\"tls_entries\":%u,\"bio_entries\":%u,\"poll_calls\":%u,\"wire_sent_bytes\":%u,\"wire_received_bytes\":%u,\"application_sent_bytes\":0,\"application_received_bytes\":%u,\"clock_attempts\":%u,\"library_time_reads\":%u,\"certificate_utc_calls\":%u,\"clock_failure\":",verify,result.body_accepted?"true":"false",result.clean_peer?"true":"false",result.version_number,result.ciphersuite_id,result.peer_cert_count,result.source_calls,result.source_closed?"true":"false",result.rng_calls,result.rng_bytes,result.tls_calls,result.bio_calls,result.polls,result.wire_send,result.wire_recv,result.response_bytes,result.clock_attempts,result.library_reads,result.cert_utc_calls);
    if(result.clock_reason)printf("\"%s\"",result.clock_reason);else printf("null");
    printf(",\"descriptor_close_attempts\":%u,\"descriptor_close_rc\":",result.closes);nullable(result.descriptor_close_rc);
    printf(",\"owned_workspace_wiped\":%s,\"entropy_source\":\"checked_host_getentropy\",\"native_executed\":false}\n",zero?"true":"false");
    wipe(&result,sizeof(result));return ok?0:1;
}
