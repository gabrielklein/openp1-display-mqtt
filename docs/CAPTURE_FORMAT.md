# Diagnostic capture v1 (planned contract)

M1 supports strict hex and MQTT lines only, with an explicitly supplied layer.
JSONL and permissive skipped-line reporting below remain M5 work; M1 rejects
unsupported lines clearly. See [M1](M1.md).

UTF-8 .txt, one JSON object per line (JSONL). First record type=meta, schema=openp1.capture, version=1. Input records type=rx carry monotonic t_ms, layer=wire_hdlc or llc_apdu, and hex. Subsequent type=decoded or error records refer to rx sequence numbers. Chunks may be incomplete frames. Decoder must combine chunks; timestamps preserve input timing.

The replay reader consumes rx records only. Saved decoded/error records are expected results for debugging, never a substitute for decoding the original bytes. Metadata includes firmware and UART settings, never credentials. Limits and truncation are explicit records, never silent data loss.

Also accept standalone hex lines and MQTT lines ending in a hex payload. Parse the log wrapper before strict hex validation. MQTT /raw and /hdlc from the user's previous project identify different layers; do not assume /hdlc includes the outer HDLC envelope. Ignore irrelevant log lines with visible skipped-line counts. Reject malformed candidate records with line-numbered errors.

Examples in captures are deliberately synthetic and are NOT valid meter telegrams. Add complete user captures as regression fixtures only when available; never present truncated conversation snippets as valid checksummed frames.
