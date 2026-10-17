---
title: Sending a payload
parent: Payloads
nav_order: 9
---

# Sending a payload

A built payload is a single `.elf`. Getting it to the console is one TCP connection to a listening
loader. This page covers the `payload` command, the wire format, the loader's expectations, and the
common ways things fail.

## The command

```
dotnet run --project tools/SharpProspero.Bindings.Generator -- payload --send \
  --host ps5-host-not-configured.invalid \
  --file samples/prospero-payload-unjail/out/SampleApp.elf
```

| Flag | Meaning | Default |
|---|---|---|
| `--host` | The console's IP address. | Required. |
| `--file` | The `.elf` to send. | Required. |
| `--port` | The loader's TCP port. | 9021. |
| `--timeout` | Connect timeout in milliseconds. | 5000. |

A successful send prints one line with the byte count sent and closes the connection. The console
side maps the payload and jumps to its entry immediately; the send program does not read a reply.

## The wire

The loader speaks a small protocol:

1. It listens on a TCP port (9021 by default).
2. When a connection comes in, it reads the raw `.elf` bytes to EOF.
3. It closes the connection.
4. It maps the payload into the host process and jumps to `_start`.

Nothing else on the wire: no length prefix, no magic, no framing. The send program writes the file
bytes and closes.

## The listening loader

The listener is a separate program the console runs — the payload SDK does not provide the loader
side. Setting up the loader is the console side of the workflow. The `payload --send` command speaks
whichever wire format the loader expects at the given port; the 9021 default matches the standard
loader shape (`ET_DYN` raw ELF bytes, close to signal EOF).

For a loader that expects a different wire format (a length prefix, a handshake byte, a scoped file
name), wrap the send in a small shell script that writes those bytes before the `.elf`.

## The mapping

The loader maps the payload's LOAD segments at a base of its choice, applies the
`R_X86_64_RELATIVE` fix-ups, and jumps to the entry recorded in the ELF header (`e_entry`). The
payload's CRT takes it from there — see [Runtime bring-up](payload-runtime.md).

## Failures the send program surfaces

| Failure | What to check |
|---|---|
| "Could not connect to `<host>:<port>`" | The loader is not listening, the firewall on the host is blocking the port, or `--host` names the wrong machine. |
| "Broken pipe" mid-send | The loader closed the connection because the payload's initial bytes did not match the ELF magic it expected. Rebuild and re-send. |
| The send succeeds but the console shows no sign of the payload | The loader mapped the payload and jumped to `_start`, but the CRT failed silently. Rebuild with `-DiagnosticBreadcrumbs` and read the device log — the first missing `sp:*` breadcrumb names the failed step. |

## Restarting the daemon

A daemon-shaped payload (accept loop) keeps running after the send closes. Sending a new build over
the same port replaces the CRT's bring-up sequence in a new thread of the host process; the old
daemon thread does not stop. Two daemons on the same port fail one bind — the newer one exits.

To restart cleanly:

1. Stop the host process from the console side (or reboot).
2. Start the host process again.
3. Send the new payload.

For iterative development, the fastest cycle is: edit, rebuild, restart the host, resend.

## Sending to a firewalled network

If the console is behind a router the host machine cannot reach, either:

- Move the host and the console to the same subnet.
- Add a port-forward on the router so `--host <router-ip> --port <forwarded>` reaches the loader.
- Run the send from a machine on the same subnet as the console.

The payload wire has no authentication — anyone on the same network can send an ELF the loader will
map. Reserve development machines for development networks.
