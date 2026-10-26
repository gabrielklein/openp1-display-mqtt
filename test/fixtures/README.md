# Anonymized regression fixtures

These inputs derive from historical meter captures. Both meter identifiers have
been replaced with same-length examples: 12345678 and 1234567. MQTT wrappers use
example/p1/raw. Modified HDLC frame FCS checksums have been recalculated; frame
headers, segmentation and lengths are unchanged. These are anonymized test
fixtures, not verbatim original wire captures. Measurement values and timestamps
remain regression test data and must not be used to identify a real installation.
The first LLC/APDU fixture matches the built-in anonymized simulation. Tests
validate frame checksums and byte-exact reassembly against that fixture.
