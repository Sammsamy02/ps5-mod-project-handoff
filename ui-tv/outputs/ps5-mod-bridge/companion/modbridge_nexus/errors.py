"""Public exceptions deliberately contain no provider bodies or URLs."""

_MESSAGES = {
    "configuration": "Nexus connection configuration is unavailable or invalid.",
    "credentials": "Nexus credentials are missing or invalid.",
    "storage": "Private local storage is unavailable or unsafe.",
    "destination": "The quarantine destination is unavailable or unsafe.",
    "invalid_input": "The requested Nexus identifier or URL is invalid.",
    "reconnect_required": "Reconnect your Nexus account to continue.",
    "authorization_cancelled": "Nexus sign-in was cancelled.",
    "authorization": "Nexus authorization is invalid, expired, or already used.",
    "download_authorization": "This download requires matching unexpired Nexus website authorization.",
    "content_hidden": "This mod is hidden by the current content filter.",
    "provider": "Nexus could not complete the request.",
    "payload": "Nexus returned an invalid or unsupported response.",
    "redirect": "Redirects are not permitted for this request.",
    "rate_limit": "Nexus has limited requests; wait before trying again.",
    "size_limit": "The response exceeded the configured byte limit.",
    "truncated": "The download ended before the advertised byte count.",
    "timeout": "The operation exceeded its configured time limit.",
    "network": "The secure connection could not complete.",
}


class NexusError(Exception):
    def __init__(self, code, *, status=None, retry_after=None):
        self.code = code if code in _MESSAGES else "provider"
        self.status = status if type(status) is int and 100 <= status <= 599 else None
        self.retry_after = retry_after if type(retry_after) in (int, float) and 0 <= retry_after <= 86400 else None
        super().__init__(_MESSAGES[self.code])

    def __repr__(self):
        return f"NexusError(code={self.code!r}, status={self.status!r}, retry_after={self.retry_after!r})"
