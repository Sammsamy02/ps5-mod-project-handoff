"""Focused real-library owned localhost operation/cache/authority/shutdown proof."""
import importlib.util
import json
import os
from pathlib import Path
import socket
import ssl
import subprocess
import threading
import time

HERE=Path(__file__).resolve().parent
BORROWED=HERE.parent/'native-https-console-fixture-1'
spec=importlib.util.spec_from_file_location('owned_console_fixture',BORROWED/'fixture.py')
fixture=importlib.util.module_from_spec(spec);spec.loader.exec_module(fixture)
manifest=json.loads((HERE/'host/manifest.json').read_text())


class Server(threading.Thread):
    def __init__(self,case):
        super().__init__(daemon=True);self.case=case
        self.listener=socket.socket();self.listener.bind(('127.0.0.1',0));self.listener.listen(1)
        self.listener.settimeout(8);self.port=self.listener.getsockname()[1]
        self.result={'case':case,'http_request_bytes':0,'response_sent_bytes':0,'clean_peer_close':False}
    def run(self):
        raw=conn=None
        try:
            ctx=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            ctx.minimum_version=ctx.maximum_version=ssl.TLSVersion.TLSv1_2
            ctx.set_ciphers('ECDHE-ECDSA-AES128-GCM-SHA256');ctx.set_ecdh_curve('prime256v1')
            ctx.options|=ssl.OP_NO_TICKET;ctx.keylog_filename=None
            ctx.load_cert_chain(BORROWED/'chain.pem',BORROWED/'server-private/leaf.key')
            def sni(sock,name,context): self.result['sni_matches']=name==fixture.NAME
            ctx.sni_callback=sni
            raw,peer=self.listener.accept();assert peer[0]=='127.0.0.1';raw.settimeout(4)
            conn=ctx.wrap_socket(raw,server_side=True);raw=None
            self.result.update(handshake_ok=True,version=conn.version(),cipher=conn.cipher()[0])
            data=b''
            while b'\r\n\r\n' not in data and len(data)<256:
                part=conn.recv(256-len(data));data+=part
                if not part: break
            self.result['http_request_bytes']=len(data)
            self.result['request_matches']=data==fixture.REQUEST
            assert self.result['request_matches']
            response=fixture.RESPONSE[:-1] if self.case=='incomplete' else fixture.RESPONSE
            conn.sendall(response);self.result['response_sent_bytes']=len(response)
            if self.case!='no_peer_close':
                raw=conn.unwrap();conn=None;self.result['clean_peer_close']=True
        except (OSError,AssertionError) as e:
            self.result['failure_type']=type(e).__name__;self.result['failure_reason']=getattr(e,'reason',None)
        finally:
            if conn: conn.close()
            if raw: raw.close()
            self.listener.close();self.result['listener_closed']=True


def exchange(port,raw):
    with socket.create_connection(('127.0.0.1',port),2) as peer:
        peer.settimeout(4);peer.sendall(raw);data=b''
        while True:
            part=peer.recv(16384)
            if not part: break
            data+=part
    header,body=data.split(b'\r\n\r\n',1)
    lines=header.decode('ascii').split('\r\n')
    fields=dict(v.split(': ',1) for v in lines[1:])
    assert int(fields['Content-Length'])==len(body) and fields['Connection']=='close'
    assert fields['Cache-Control']=='no-store' and fields['X-Content-Type-Options']=='nosniff'
    assert 'Access-Control-Allow-Origin' not in fields
    return int(lines[0].split()[1]),body,fields


def run_case(case):
    server=Server(case);server.start()
    with socket.socket() as reserve: reserve.bind(('127.0.0.1',0));port=reserve.getsockname()[1]
    authority=f'127.0.0.1:{port}'
    def request(method='GET',target='/report',extras=''):
        return f'{method} {target} HTTP/1.1\r\nHost: {authority}\r\n{extras}\r\n'.encode()
    argv=[str(HERE/'host'/manifest['artifact']),'--port',str(port),'--lifetime','20','--tls-port',str(server.port)]
    if case=='wrong_hostname': argv+=['--tls-hostname','wrong.native-https.modbridge.test']
    proc=subprocess.Popen(argv,stdout=subprocess.PIPE,stderr=subprocess.PIPE,
                          env={**os.environ,'PYTHONDONTWRITEBYTECODE':'1'})
    try:
        until=time.monotonic()+8
        while time.monotonic()<until:
            assert proc.poll() is None,'host exited before readiness'
            try:
                with socket.create_connection(('127.0.0.1',port),.1): break
            except OSError: time.sleep(.01)
        else: raise AssertionError('host did not become ready')
        status,first,fields=exchange(port,request());assert status==200 and len(first)<8192
        assert first==exchange(port,request())[1]
        report=json.loads(first);https=report['observations']['https']
        assert report['build_id']==manifest['build_id'] and report['authority']==authority
        assert report['version']=='0.1.0' and report['origin']=='desktop_fixture'
        assert not report['installation_supported'] and report['observations']['getpid']['value']==proc.pid
        assert report['firmware']['reason']==report['game']['reason']=='not_queried'
        core=json.loads(exchange(port,request(target='/core'))[1]);assert core==report['core']
        assert core['status']=='passed' and not core['installation_supported']
        assert len(core['checks'])==7 and all(core['checks'].values())
        assert report['observations']['entropy']['call_count']==0
        assert report['observations']['drbg']['total_api_call_count']==0
        assert report['observations']['drbg']['seed_rc'] is None
        assert https['config_id']==manifest['tls_descriptor']['config_id']
        assert https['source_callback_count']==https['source_os_call_count']==https['total_api_call_count']==2
        assert https['source_closed'] and not https['source_failed'] and https['seed_rc']==0
        assert https['local_cleanup_complete'] and https['descriptor_close_attempts']==1
        assert not https['descriptor_close_failed'] and https['last_api']['returned_length_bytes']==32
        assert https['rng_calls']<=96 and https['rng_requested_bytes']<=6144
        assert https['clock']['authenticated_time'] is False and https['clock']['last_utc']
        server.join(6);assert not server.is_alive() and server.result['listener_closed']
        if case=='valid':
            assert https['status']=='passed' and https['certificate_verified'] and https['framed_response_complete']
            assert https['peer_close_confirmed'] and https['local_close_notify_confirmed'] and https['body_accepted']
            assert https['handshake_rc']==0 and https['verify_flags']==0
            assert https['http_request_bytes']==114 and https['response_bytes']==108
            assert server.result['clean_peer_close'] and server.result['request_matches']
            # Existing authority/framing/shutdown gates and exact responses.
            bad=[(request().replace(authority.encode(),b'example.org'),400),
                 (request(extras=f'Host: {authority}\r\n'),400),
                 (request(extras='Transfer-Encoding: chunked\r\n'),400),
                 (request(extras='Origin: http://evil.example\r\n'),403),
                 (request('POST','/shutdown','Content-Length: 0\r\nX-Modbridge-Shutdown: 0\r\n'),403),
                 (request(target='/shutdown'),405)]
            names={400:b'Bad Request',403:b'Forbidden',405:b'Method Not Allowed'}
            for raw,want in bad:
                code,body,_=exchange(port,raw)
                assert code==want and body==b'{"error":"'+names[want]+b'"}\n'
                assert proc.poll() is None and first==exchange(port,request())[1]
            version=json.loads(exchange(port,request(target='/version'))[1])
            assert all(version[k]==report[k] for k in ('version','build_id','profile','origin'))
            code,page,page_headers=exchange(port,request(target='/'));assert code==200
            assert b'X-Modbridge-Shutdown' in page and "default-src 'none'" in page_headers['Content-Security-Policy']
        elif case=='wrong_hostname':
            assert https['status']=='failed' and not https['certificate_verified'] and not https['framed_response_complete']
            assert https['verify_flags']==4 and https['http_request_bytes']==server.result['http_request_bytes']==0
        elif case=='incomplete':
            assert https['status']=='failed' and https['certificate_verified'] and not https['framed_response_complete']
            assert https['peer_close_confirmed'] and https['reason']=='transcript_incomplete' and https['response_bytes']==107
        else:
            assert https['status']=='failed' and https['certificate_verified'] and https['framed_response_complete']
            assert not https['peer_close_confirmed'] and not https['local_close_notify_confirmed']
            assert https['reason']=='tls_eof_or_read_failed' and https['response_bytes']==108
        code,body,_=exchange(port,request('POST','/shutdown',f'Content-Length: 0\r\nX-Modbridge-Shutdown: 1\r\nOrigin: http://{authority}\r\n'))
        assert code==200 and body==b'{"status":"shutdown_requested"}\n'
        assert proc.wait(timeout=3)==0
        stdout,stderr=proc.communicate();assert stdout==stderr==b''
        with socket.socket() as reuse:
            reuse.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1);reuse.bind(('127.0.0.1',port))
        row={'case':case,'argv':argv,'report':report,'server':server.result,'report_cached':True,
             'seven_core_checks_passed':True,'owned_shutdown_passed':True,'stdout_empty':True,
             'host_boundary_wipe_and_canaries_passed':True,'assertions_passed':True}
        print(json.dumps({'case':case,'status':https['status'],'certificate_verified':https['certificate_verified'],
            'framed_response_complete':https['framed_response_complete'],'peer_close_confirmed':https['peer_close_confirmed'],
            'local_cleanup_complete':https['local_cleanup_complete'],'source_calls':2,'checks_passed':True}),flush=True)
        return row
    finally:
        if proc.poll() is None: proc.terminate()
        proc.communicate(timeout=4);server.join(6)


if __name__=='__main__':
    rows=[]
    for case in ('valid','wrong_hostname','incomplete','no_peer_close'):
        rows.append(run_case(case))
        (HERE/'host-tests.json').write_text(json.dumps(rows,indent=2)+'\n')
