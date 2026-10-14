"""Read-only reuse of the completed fixture's clock/BIO lifecycle into fresh scratch."""
import datetime
import pathlib
from cryptography import x509
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec, rsa
from cryptography.x509.oid import NameOID, ExtendedKeyUsageOID

HERE = pathlib.Path(__file__).resolve().parent
OLD = HERE.parent / "native-https-experiment-1" / "tls_client.c"


def private_file(path, data):
    path.write_bytes(data)
    path.chmod(0o600)


def fixtures():
    target = HERE / "fixture-private"
    target.mkdir(mode=0o700)
    now = datetime.datetime.now(datetime.timezone.utc)
    root_key = rsa.generate_private_key(65537, 4096)
    issuer_key = ec.generate_private_key(ec.SECP384R1())
    leaf_key = ec.generate_private_key(ec.SECP256R1())
    names = [x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, s)]) for s in
             ("Owned RSA4096 experiment root", "Owned P384 experiment issuer", "nexus-mbed.fixture.test")]

    def certificate(key, subject, issuer, signer, serial, ca, digest):
        builder = (x509.CertificateBuilder().subject_name(subject).issuer_name(issuer)
                   .public_key(key.public_key()).serial_number(serial)
                   .not_valid_before(now - datetime.timedelta(days=1))
                   .not_valid_after(now + datetime.timedelta(days=7))
                   .add_extension(x509.BasicConstraints(ca=ca, path_length=None if ca else None), True)
                   .add_extension(x509.KeyUsage(True, False, False, False, False, ca, ca, False, False), True))
        if not ca:
            builder = builder.add_extension(x509.ExtendedKeyUsage([ExtendedKeyUsageOID.SERVER_AUTH]), False)
            builder = builder.add_extension(x509.SubjectAlternativeName([x509.DNSName("nexus-mbed.fixture.test")]), False)
        return builder.sign(signer, digest)

    root = certificate(root_key, names[0], names[0], root_key, 101, True, hashes.SHA256())
    issuer = certificate(issuer_key, names[1], names[0], root_key, 102, True, hashes.SHA256())
    leaf = certificate(leaf_key, names[2], names[1], issuer_key, 103, False, hashes.SHA384())
    private_file(target / "leaf.key", leaf_key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8, serialization.NoEncryption()))
    # CA private keys stay only in this preparation process, then leave scope.
    (HERE / "fixture-chain.pem").write_bytes(leaf.public_bytes(serialization.Encoding.PEM) + issuer.public_bytes(serialization.Encoding.PEM))
    der = root.public_bytes(serialization.Encoding.DER)
    (HERE / "fixture-root.der").write_bytes(der)
    (HERE / "fixture_root.h").write_text("/* Development-only owned fixture CA, never a public-domain trust input. */\nstatic const unsigned char fixture_root_der[]={" + ",".join(str(b) for b in der) + "};\n")


def client():
    text = OLD.read_text()
    text = text[:text.index("int main(int argc,char **argv){")]
    text = text.replace("/* Throwaway loopback fixture: deterministic development entropy, never deploy. */", "/* Throwaway host TLS-only experiment; real checked host getentropy, never deploy. */")
    text = text.replace('#include "fixture_root.h"', '#include "fixture_root.h"\n#include "official_root.h"\n#include "experiment_identity.h"\n#include <arpa/inet.h>\n#include <sys/random.h>')
    text = text.replace("#define RNG_BYTES_LIMIT 3072u", "#define RNG_BYTES_LIMIT 6144u")
    for line in [s for s in text.splitlines() if s.startswith("static const char name[]=") or s.startswith("static const unsigned char request[]=") or s.startswith("static const unsigned char response[]=")]:
        text = text.replace(line + "\n", "")
    text = text.replace("groups[]={23,0}", "groups[]={23,24,0}")
    text = text.replace("sigs[]={0x0403,0}", "sigs[]={0x0403,0x0503,0x0401,0}")
    text = text.replace("MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA256),", "MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA256)|MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA384),")
    text = text.replace("MBEDTLS_X509_ID_FLAG(MBEDTLS_PK_ECKEY)|MBEDTLS_X509_ID_FLAG(MBEDTLS_PK_ECDSA),", "MBEDTLS_X509_ID_FLAG(MBEDTLS_PK_ECKEY)|MBEDTLS_X509_ID_FLAG(MBEDTLS_PK_ECDSA)|MBEDTLS_X509_ID_FLAG(MBEDTLS_PK_RSA),")
    text = text.replace("MBEDTLS_X509_ID_FLAG(MBEDTLS_ECP_DP_SECP256R1),0", "MBEDTLS_X509_ID_FLAG(MBEDTLS_ECP_DP_SECP256R1)|MBEDTLS_X509_ID_FLAG(MBEDTLS_ECP_DP_SECP384R1),2048")
    text = text.replace("int clock_errno; int clean_peer,body_accepted;", "int clock_errno; int clean_peer,body_accepted;\n    int version_number,ciphersuite_id,descriptor_close_rc; unsigned peer_cert_count;")
    text = text.replace("    unsigned index=o->source_calls++;\n    for(unsigned i=0;i<32;i++)o->staging[i]=(unsigned char)(0x40+index*0x40+i);", "    o->source_calls++;errno=0;int rc=getentropy(o->staging,32);int e=errno;\n    if(rc||e){wipe(o->staging,sizeof(o->staging));wipe(out,cap);fail(o,\"host_getentropy_failed\");return MBEDTLS_ERR_ENTROPY_SOURCE_FAILED;}")
    text = text.replace("if(!out||!n||n>32)", "if(!out||!n||n>64)")
    text = text.replace("    else if(action==1)rc=mbedtls_ssl_write(&o->ssl,w,n);\n", "")
    text = text.replace("    else rc=mbedtls_ssl_close_notify(&o->ssl);", "    else if(action==3)rc=mbedtls_ssl_close_notify(&o->ssl);\n    else rc=EXP_FAIL;")
    text = text.replace("connect_once(struct operation *o,unsigned port)", "connect_once(struct operation *o,const char *ip,unsigned port)")
    text = text.replace("a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);", "if(inet_pton(AF_INET,ip,&a.sin_addr)!=1){fail(o,\"numeric_ipv4_required\");return -1;}")
    text += r'''
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
'''
    if "mbedtls_ssl_write" in text or "0x40+index" in text:
        raise RuntimeError("application write or deterministic source survived")
    (HERE / "tls_client.c").write_text(text)


if __name__ == "__main__":
    if not (HERE / "fixture_root.h").exists():
        fixtures()
    client()
    print("Fresh TLS-only client and owned mixed-algorithm fixture prepared.")
