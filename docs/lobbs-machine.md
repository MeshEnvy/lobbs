# LoBBS Machine API

[https://discord.gg/DMrGcGQfMN](https://discord.gg/DMrGcGQfMN)

For rich clients and agents over DM. Command names and player behavior are in the [daily user guide](lobbs-daily.md). Field numbers and hook behavior are in [LoBBS internals](lobbs-internals.md).

## Why a machine mode

The plain-text reply is written for a person on a phone. A mail row reads `[2]*@ben: see you at the trailhead (3h ago)`. The index, the `*` unread mark, the truncated preview, the relative time, and the `{p n/t}` footer exist for eyes, and plugins can rewrite them through `display_human`. A client that scrapes that line breaks whenever the wording changes.

A machine reply is the same result set before rendering. That mail row arrives as separate fields: the row id, the read flag, the created time in Unix seconds, and the preview, next to the title line a person would see. A client reads the fields it knows and ignores the rest.

The request id exists because the mesh is not a socket. DMs arrive seconds or minutes late, some never arrive, and they can arrive out of order. A client with several requests in flight matches each reply to its request by the echoed id. A person typing by hand does not need that, so the id is optional, and its presence is what selects machine mode.

There is no second API. Every command a person can type, a client can send with an id, and the same handler answers both. A new command works from a client the day it ships.

Paging uses the same cached reply for both. Each page is its own request with its own id, so a client that loses page 2 asks for page 2 again instead of re-running the command. `[n:max]` tells it how many DMs to collect before it parses.

## Requests

Put a number and a space after `/` on a command to get a machine reply. The number only works when a space follows it (`/42 login`, not `/42login`). Without a leading id, replies use plain text and `{p i/n}` footers on the same cached result set.

Run the command first, then request further pages without re-running it.

```
/42 mail list
<42>ok [1:2]
…payload…

/43 p2
<43>ok [2:2]
…payload…
```

## Reply shape

Each DM is 200 bytes or less, header included.

| Case                    | Header                   | Body                  |
| ----------------------- | ------------------------ | --------------------- |
| Success, one page       | `<id>ok` newline         | LoScalar record lines |
| Success, multiple pages | `<id>ok [n:max]` newline | Slice of the document |
| Error                   | `<id>`                   | One sentence, no `ok` |

The `[n:max]` marker is separate from the request id. Use a fresh id on each page request (`/43 p2`, `/44 p3`). The reply echoes the id from that page request. Stop when `n == max`.

Success bodies are the cached result set encoded as LoScalar lines (`number:value|number:value`), one record per line. Concatenate page bodies in order, then split on newlines. Pages may cut through the middle of a line. Reassemble the full document before parsing records.

## Paging

Human paging: `/p2` after `/mail list`.

Machine paging: `/43 p2` with a new id each page.

The node caches your last successful reply for about five minutes (reset on each page read). A new successful command replaces the cache. Errors do not change it. Payloads larger than 8 KiB are not cached. Page 1 is sent and `/p2` replies `No cached reply.`. Past the end: `No such page.`
