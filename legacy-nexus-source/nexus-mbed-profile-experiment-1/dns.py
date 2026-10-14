"""DNS only, executed in a separately timeout-bounded process."""
import json
import socket
import sys

if len(sys.argv) != 2 or sys.argv[1] not in ("users.nexusmods.com", "api.nexusmods.com"):
    raise SystemExit(2)
addresses = list(dict.fromkeys(item[4][0] for item in socket.getaddrinfo(sys.argv[1], 443, socket.AF_INET, socket.SOCK_STREAM)))
if not addresses:
    raise SystemExit("no IPv4 address")
print(json.dumps({"hostname": sys.argv[1], "ipv4_addresses": addresses, "selected_ipv4": addresses[0]}))
