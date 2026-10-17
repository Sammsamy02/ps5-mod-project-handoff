# DLL diagnostic with retrievable result path

Run `modbridge-owned-dll-data-test.elf` once in Payload Manager, then leave FTP on and report the loader message. Townfall is not needed for this standalone DLL execution test.

The original test used `/tmp/modbridge-owned-dll-candidate.jsonl`; FTP reported that `/tmp` does not exist. This replacement uses `/data/modbridge-owned-dll-candidate.jsonl`. Its original DLL inputs, Windows bridges, native memory adapter and startup code are preserved; only the report path and build identity were changed. The rebuild reproduces the old driver object exactly before applying that profile change. Other 15 objects are reused unchanged.

Build identity: `db1c40e6b35e4f16b4f78afcb38ed15c`.

A successful result must contain that identity, all three DLL calls returned, values `[86,93]`, and final cleanup retirement. A launcher status alone does not establish these outcomes.

This does not install a gameplay mod. UE4SS engine calls and arbitrary DLL mod compatibility remain untested.

ELF: 185,608 bytes, SHA-256 `bec47619a5738e599bd80a7d2a252deb3220001e865d8fe651e2a28c8c1577f9`.
