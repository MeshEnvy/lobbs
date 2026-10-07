# LoLog host tests

Run from repo root:

```bash
cd ci/host && pio test -e host -f test_lolog_host
```

Coverage (Unity):

| Area | Tests |
|------|--------|
| Boot / mount | Empty format, erased chip, pure DATA replay, reboot |
| File ops | Nested mkdir, overwrite, rename, rmdir, prefix names, 2.5 KB EXTENT file, writeAt, stat |
| Crash / ACID | Fault on every prog byte (large commit), every prog call, torn tail + append, CRC poke, partial mkdir group |
| Index / clean | Index flush remount, bulk flush, tomb shadow, delete when full, cleaner churn, headroom |
| Fuzz | 30 rounds random ops + crash injection vs ref model |

Fault injection uses `LoLogRamDevice` (`setFaultAfterProgBytes`, `setFaultOnProgCall`, `poke`).
