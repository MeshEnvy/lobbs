# LoBBS Daily User Guide

[https://discord.gg/DMrGcGQfMN](https://discord.gg/DMrGcGQfMN)

If you found LoBBS somewhere on your mesh and are a daily user, this guide is for you.

DM the node a line that starts with `/`.

`/login username password` signs in or creates an account after the node is installed. If the node says it needs install, find the SysOp. See the [SysOp guide](lobbs-sysop.md).

Long answers end with `{p 1/3}`. Send `/p2`, then `/p3`. Paging works on your last answer for about 5 minutes. A new command replaces it.

## Public

| Command         | Notes                                    |
| --------------- | ---------------------------------------- |
| `/hi`           | Welcome screen and hints                 |
| `/status`       | Overall status (paged)                   |
| `/time`         | Current time                             |
| `/help [topic]` | Command catalog, or topic help           |
| `/pN`           | Next page of the last reply (e.g. `/p2`) |
| `/wall`         | Play the Wall game (shared ASCII art)    |
| `/yarn`         | Play the Yarn game (shared story)        |

## Session

| Command                | Notes                        |
| ---------------------- | ---------------------------- |
| `/login user password` | Sign in or create an account |
| `/logout`              | End session                  |
| `/whoami`              | Who you are                  |
| `/passwd new confirm`  | Change your password         |

A login ends when the node reboots or after a day without commands (the SysOp can change both limits). When every login slot is taken, a new login signs out whoever has been idle longest. Run `/login` again if `/whoami` says you are not logged in.

## Users

Login required.

| Command            | Notes            |
| ------------------ | ---------------- |
| `/users list`      | User list        |
| `/users find text` | Search usernames |

## Mail

Login required. SysOps reading someone else's inbox: [SysOp guide](lobbs-sysop.md).

| Command                    | Notes                  |
| -------------------------- | ---------------------- |
| `/mail list`               | Your inbox             |
| `/mail read N`             | Read message N         |
| `/mail N`                  | Same as `/mail read N` |
| `/mail unread N`           | Mark unread            |
| `/mail delete N`           | Delete from your inbox |
| `/mail send user message…` | Send mail              |

## News

Login required.

| Command               | Notes                  |
| --------------------- | ---------------------- |
| `/news list`          | News index             |
| `/news read N`        | Read item N            |
| `/news N`             | Same as `/news read N` |
| `/news unread N`      | Mark unread            |
| `/news post message…` | Post news              |

## Wall

`/wall` shows the grid (no login). Painting needs login.

Rows `a`–`l`, columns `1`–`12`. `a4x` paints `x` at row a, column 4. `-a4` clears that cell. Several tokens in one command: `/wall a1# a2# b2#`.

| Command        | Notes                                   |
| -------------- | --------------------------------------- |
| `/wall token…` | Paint. Default quota is 1 cell per hour |

## Yarn

`/yarn` shows the tail (no login). Adding words needs login.

| Command        | Notes                                                       |
| -------------- | ----------------------------------------------------------- |
| `/yarn word …` | Add words. Default quota is 1 word (32 characters) per hour |
