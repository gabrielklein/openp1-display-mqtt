# Simulation input format

Full Settings accepts strict hex lines or MQTT-style lines ending in one hex
payload, for example:

```text
example/p1/hdlc E6E700...
```

The ellipsis is illustrative and must be replaced with complete hex. Select the
input layer explicitly: **wire HDLC** includes frame headers/flags/checksums;
**LLC/APDU** is already reassembled application data, even if an old topic calls
it `hdlc`. Never wrap an application payload into a fabricated validated frame.

Input is bounded to 4096 text characters and 2048 decoded bytes. Malformed hex,
odd digits, unsupported wrappers and empty input are rejected. JSONL, arbitrary
log filtering and file import/export are not supported. Replay feeds 64-byte
chunks at 20 ms intervals and repeats after a 3.5-second pause.

Anonymized fixtures live in `test/fixtures/`; their README describes same-length
identifier replacement and recalculated FCS checksums. They are not verbatim
original captures. Runtime diagnostics preserve actual received bytes and can
therefore contain real meter identifiers; anonymize before sharing them.
