"""Persistent account lifecycle against owned offline provider responses."""
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from urllib.parse import parse_qs, urlencode, urlsplit
from unittest import mock
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'companion'))
import modbridge_nexus as nexus


def response(value, status=200):
    return nexus.Response(status, {}, json.dumps(value).encode())


def token():
    return response({'access_token':'rotated-access', 'refresh_token':'rotated-refresh', 'token_type':'Bearer', 'expires_in':300})


def identity(oauth=True):
    return response({'sub':'42', 'name':'Owned user', 'membership_roles':['premium'], 'preferences':{'adult':False}} if oauth else {'user_id':42,'name':'Owned user','is_premium':True,'is_supporter':False})


class Wire:
    def __init__(self, values, inspect=None):
        self.values, self.requests, self.inspect = list(values), [], inspect
    def request(self, method, url, **kwargs):
        self.requests.append((method, url, kwargs))
        if self.inspect:
            self.inspect(method, url, kwargs)
        value = self.values.pop(0)
        if isinstance(value, Exception):
            raise value
        return value


class Accounts(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.store = nexus.CredentialStore(Path(self.temp.name).resolve()/'account.json')
        self.now = 1000
        self.config = nexus.OAuthConfig('owned-client','http://127.0.0.1:8181/callback')
        self.old = nexus.Credentials(access_token='old-access',refresh_token='old-refresh',expires_at=1010)
    def manager(self, wire):
        self.assertTrue(hasattr(nexus, 'AccountManager'), 'Persistent live account lifecycle is missing')
        return nexus.AccountManager(self.store, transport=wire, clock=lambda:self.now)
    def save(self, credentials=None):
        self.assertTrue(hasattr(self.store, 'save_account'), 'OAuth configuration is not persisted with credentials')
        self.store.save_account(credentials or self.old, self.config)
    def test_config_roundtrip_and_legacy_key(self):
        self.save()
        self.assertEqual(self.store.load_account(), (self.old,self.config))
        self.assertEqual(self.store.load(), self.old)
        self.store.save(nexus.Credentials(api_key='owned-key'))
        self.assertEqual(self.store.load_account(), (nexus.Credentials(api_key='owned-key'),None))
    def test_refresh_rotation_persisted_before_metadata_request_and_restart(self):
        self.save()
        def inspect(method, url, kwargs):
            if method == 'GET':
                self.assertEqual(self.store.load().refresh_token, 'rotated-refresh')
                self.assertEqual(kwargs['headers']['Authorization'],'Bearer rotated-access')
        wire = Wire([token(),response([])],inspect)
        self.assertEqual(self.manager(wire).client().games(), [])
        self.assertEqual(self.manager(Wire([])).credentials().access_token,'rotated-access')
        self.assertEqual(parse_qs(wire.requests[0][2]['body'].decode())['refresh_token'],['old-refresh'])
    def test_revoked_refresh_preserves_record_and_blocks_expired_get(self):
        self.save(); self.now=1100
        wire=Wire([response({},400)])
        with self.assertRaises(nexus.NexusError) as caught:
            self.manager(wire).client().games()
        self.assertEqual(caught.exception.code,'reconnect_required')
        self.assertEqual(self.store.load(),self.old)
        self.assertEqual([r[0] for r in wire.requests],['POST'])
    def test_network_and_save_failure_do_not_send_old_token(self):
        self.save(); self.now=1100
        for wire in (Wire([nexus.NexusError('network')]), Wire([token()])):
            with self.subTest(wire=wire), mock.patch.object(self.store,'save_account',side_effect=nexus.NexusError('storage')):
                with self.assertRaises(nexus.NexusError): self.manager(wire).client().games()
                self.assertEqual(self.store.load(),self.old)
                self.assertEqual([r[0] for r in wire.requests],['POST'])
    def test_legacy_oauth_without_config_requires_reconnect(self):
        self.store.save(self.old)
        with self.assertRaises(nexus.NexusError) as caught: self.manager(Wire([])).credentials()
        self.assertEqual(caught.exception.code,'reconnect_required')
    def test_one_401_refresh_and_retry_then_stop(self):
        self.store.save_account(nexus.Credentials(access_token='old-access',refresh_token='old-refresh',expires_at=1400),self.config)
        wire=Wire([response({},401),token(),response({},401)])
        with self.assertRaises(nexus.NexusError): self.manager(wire).client().games()
        self.assertEqual([r[0] for r in wire.requests],['GET','POST','GET'])
    def test_reused_client_switches_user_endpoint_when_store_mode_changes(self):
        self.store.save(nexus.Credentials(api_key='owned-key'))
        wire=Wire([identity(False),identity()]); client=self.manager(wire).client()
        client.user()
        self.store.save_account(nexus.Credentials(access_token='oauth',refresh_token='refresh',expires_at=1400),self.config)
        client.user()
        self.assertTrue(wire.requests[0][1].endswith('/v1/users/validate'))
        self.assertEqual(wire.requests[1][1],'https://users.nexusmods.com/oauth/userinfo')
        self.assertNotIn('apikey',wire.requests[1][2]['headers'])
    def test_failed_new_login_identity_preserves_old_account(self):
        self.store.save(nexus.Credentials(api_key='owned-key'))
        session=nexus.OAuthSession(self.config,clock=lambda:self.now)
        state=parse_qs(urlsplit(session.authorize_url).query)['state'][0]
        with self.assertRaises(nexus.NexusError):
            self.manager(Wire([token(),response({},401)])).complete_sign_in(session,self.config.redirect_uri+'?'+urlencode({'state':state,'code':'owned-code'}))
        self.assertEqual(self.store.load().api_key,'owned-key')
    def test_status_verifies_identity_and_contains_only_safe_account(self):
        self.store.save(nexus.Credentials(api_key='owned-key'))
        status=self.manager(Wire([identity(False)])).status()
        self.assertTrue(status['connected']); self.assertEqual(status['live_service'],'verified')
        self.assertEqual(status['account']['user_id'],42)
        self.assertNotIn('owned-key',json.dumps(status))
        status=self.manager(Wire([response({},401)])).status()
        self.assertFalse(status['connected']); self.assertNotIn('account',status)
    def test_unmanaged_known_expired_access_never_sent(self):
        wire=Wire([response([])])
        with self.assertRaises(nexus.NexusError): nexus.NexusClient(self.old,wire).games()
        self.assertEqual(wire.requests,[])
    def test_matching_denial_consumes_session_but_forgery_does_not(self):
        session=nexus.OAuthSession(self.config,clock=lambda:self.now)
        state=parse_qs(urlsplit(session.authorize_url).query)['state'][0]
        with self.assertRaises(nexus.NexusError): session.consume_callback(self.config.redirect_uri+'?error=access_denied&state=forged')
        with self.assertRaises(nexus.NexusError) as caught: session.consume_callback(self.config.redirect_uri+'?'+urlencode({'error':'access_denied','state':state}))
        self.assertEqual(caught.exception.code,'authorization_cancelled')
        with self.assertRaises(nexus.NexusError): session.consume_callback(self.config.redirect_uri+'?'+urlencode({'code':'owned-code','state':state}))

    # ---- Sign in with a personal API key: validated live first, saved only on success ----
    KEY = 'owned-personal-key-0123456789abcdef'
    def stored_bytes(self):
        return self.store.path.read_bytes() if self.store.path.exists() else None
    def test_key_sign_in_validates_with_the_real_client_then_saves_the_key_privately(self):
        wire=Wire([identity(False)])
        profile=self.manager(wire).sign_in_with_key(self.KEY)
        self.assertEqual((profile.user_id,profile.display_name,profile.is_premium),(42,'Owned user',True))
        self.assertEqual(len(wire.requests),1)
        method,url,kwargs=wire.requests[0]
        self.assertEqual((method,url),('GET','https://api.nexusmods.com/v1/users/validate'))
        self.assertEqual(kwargs['headers']['apikey'],self.KEY)
        self.assertNotIn('Authorization',kwargs['headers'])
        self.assertEqual(self.store.load_account(),(nexus.Credentials(api_key=self.KEY),None))
        self.assertEqual(self.store.path.stat().st_mode & 0o777,0o600)
        self.assertEqual(self.store.path.parent.stat().st_mode & 0o077,0)
        # Restart: a new manager with the same store is signed in without being told the key again.
        again=Wire([identity(False)])
        self.assertTrue(self.manager(again).status()['connected'])
        self.assertEqual(again.requests[0][2]['headers']['apikey'],self.KEY)
    def test_a_refused_key_writes_nothing_and_leaves_the_earlier_sign_in_alone(self):
        for refusal in (response({'message':'Please provide a valid API Key'},401),response({},403),response({},500),nexus.NexusError('network')):
            with self.subTest(refusal=repr(refusal)[:40]):
                self.store.disconnect()
                with self.assertRaises(nexus.NexusError) as caught: self.manager(Wire([refusal])).sign_in_with_key(self.KEY)
                self.assertFalse(self.store.path.exists())
                self.assertNotIn(self.KEY,repr(caught.exception)+str(caught.exception))
        self.store.save(nexus.Credentials(api_key='older-saved-key'))
        before=self.stored_bytes()
        with self.assertRaises(nexus.NexusError): self.manager(Wire([response({},401)])).sign_in_with_key(self.KEY)
        self.assertEqual(self.stored_bytes(),before)
        self.assertEqual(self.store.load().api_key,'older-saved-key')
        # A reply that is not a valid identity is a refusal too.
        with self.assertRaises(nexus.NexusError) as caught: self.manager(Wire([response({'user_id':42,'name':'x'})])).sign_in_with_key(self.KEY)
        self.assertEqual(caught.exception.code,'payload')
        self.assertEqual(self.stored_bytes(),before)
    def test_a_key_that_cannot_be_a_credential_is_refused_before_any_request(self):
        for bad in ('',None,5,b'key','has space','tab\tkey','new\nline','café','x'*16385,'\x7f'):
            with self.subTest(bad=repr(bad)[:20]):
                wire=Wire([])
                with self.assertRaises(nexus.NexusError) as caught: self.manager(wire).sign_in_with_key(bad)
                self.assertEqual(caught.exception.code,'credentials')
                self.assertEqual(wire.requests,[])
                self.assertFalse(self.store.path.exists())
    def test_key_sign_in_replaces_an_oauth_sign_in_and_its_configuration(self):
        self.save()
        self.manager(Wire([identity(False)])).sign_in_with_key(self.KEY)
        self.assertEqual(self.store.load_account(),(nexus.Credentials(api_key=self.KEY),None))
        # The key never appears in a status record, and disconnecting removes it.
        status=self.manager(Wire([identity(False)])).status()
        self.assertTrue(status['connected']); self.assertNotIn(self.KEY,json.dumps(status))
        self.store.disconnect()
        self.assertFalse(self.store.path.exists())
        self.assertFalse(self.manager(Wire([])).status()['connected'])

if __name__ == '__main__': unittest.main(verbosity=2)
