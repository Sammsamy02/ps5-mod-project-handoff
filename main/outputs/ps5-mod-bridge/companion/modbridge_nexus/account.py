"""Private persistent account lifecycle; saved secrets never constitute live identity."""
from dataclasses import asdict
import threading
import time

from .auth import exchange_code, refresh_credentials
from .client import NexusClient
from .errors import NexusError
from .transport import HTTPSTransport


class AccountManager:
    def __init__(self, store, transport=None, *, clock=time.time, refresh_margin=60):
        if type(refresh_margin) not in (int, float) or not 0 <= refresh_margin <= 3600:
            raise NexusError("configuration")
        self.store, self.transport, self.clock = store, transport or HTTPSTransport(), clock
        self.refresh_margin = refresh_margin
        self._lock = threading.RLock()

    def credentials(self, force=False):
        with self._lock:
            credentials, config = self.store.load_account()
            if credentials is None:
                raise NexusError("credentials")
            if credentials.api_key:
                if force:
                    raise NexusError("reconnect_required")
                return credentials
            if config is None or credentials.expires_at is None or not credentials.refresh_token:
                raise NexusError("reconnect_required")
            if force or credentials.expires_at <= self.clock() + self.refresh_margin:
                try:
                    updated = refresh_credentials(config, credentials, self.transport, clock=self.clock)
                except NexusError as error:
                    if error.status in (400, 401):
                        raise NexusError("reconnect_required") from None
                    raise
                # Persist rotated refresh credentials before any subsequent provider GET.
                self.store.save_account(updated, config)
                return updated
            return credentials

    def client(self):
        return NexusClient(account=self, transport=self.transport, clock=self.clock)

    def complete_sign_in(self, session, callback_url):
        credentials = exchange_code(session, callback_url, self.transport)
        profile = NexusClient(credentials, self.transport, clock=self.clock).user()
        with self._lock:
            self.store.save_account(credentials, session.config)
        return profile

    def status(self):
        credentials, _ = self.store.load_account()
        result = {"connected":False, "credentials_present":credentials is not None,
                  "live_service":"not_checked", "console_translation":"unverified"}
        if credentials is None:
            return result
        try:
            profile = self.client().user()
        except NexusError as error:
            result.update(live_service="unavailable", error={"code":error.code})
        else:
            result.update(connected=True, live_service="verified", account=asdict(profile))
        return result

    def disconnect(self):
        with self._lock:
            self.store.disconnect()
