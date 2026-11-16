# AI protocol — server ⇄ AI client

Reference for an **AI** client (a `zappy_ai` bot). Documents what this repo's
server (`server/src/World.cpp`) and AI (`ai/`) actually implement.

## Handshake

```
server → WELCOME\n
client → TEAM-NAME\n          (e.g. "team1")
server → CLIENT-NUM\n         (remaining free slots for that team)
server → X Y\n                (map width and height)
```

If the team is full or unknown the server replies `ko\n` and closes the socket.

## Commands

One command per line. Each gets exactly one reply line (except `Incantation`,
which sends two — see below). Resources are: `food linemate deraumere sibur
mendiane phiras thystame`.

| command | reply | effect |
|---|---|---|
| `Forward` | `ok` | move one tile forward (map wraps — torus) |
| `Right` | `ok` | turn 90° right |
| `Left` | `ok` | turn 90° left |
| `Look` | `[t0,t1,t2,…]` | objects on each visible tile (vision grows with level) |
| `Inventory` | `[food n, linemate n, …]` | current inventory + remaining life |
| `Take object` | `ok` / `ko` | pick `object` up from the current tile |
| `Set object` | `ok` / `ko` | drop `object` on the current tile |
| `Broadcast text` | `ok` | send `text` to every player (heard as `message K, text`) |
| `Fork` | `ok` | lay an egg on the current tile (spawns a future slot) |
| `Eject` | `ok` / `ko` | push players sharing this tile one tile back |
| `Connect_nbr` | `N` | number of free team slots (eggs not yet hatched) |
| `Incantation` | see below | start an elevation ritual |

`Look` / `Inventory` formats match the AI parser in `ai/network/parser.py`.

## Incantation (elevation)

```
client → Incantation\n
server → Elevation underway\n          (immediately, if requirements are met)
server → Current level: K\n            (when it completes)
```

Requirements (players of the right level on the same tile + the right stones on
the tile) are in `server` / mirrored in `ai/logic/player.py` (`ELEVATION_REQ`).
If unmet, the server replies `ko`.

**Important behaviour:** `Current level: K` is sent to **every** participant on
the tile — including ones that did **not** send `Incantation`. Those receive it
*unsolicited*, so the AI treats `Current level` as an **out-of-band** message
(buffered like a broadcast in `ai/network/client.py`, applied in
`ai/logic/brain.py`) instead of as the reply to its in-flight command. Reading it
as a normal reply would shift every later reply by one line and desync the bot.

## Broadcasts (received)

A broadcast arrives at any time as `message K, text\n`, where `K` is the
direction the sound came from (0 = same tile, 1‑8 around the player). The AI
buffers these between command replies (`Client._broadcasts`).

## Time / speed

In standard Zappy each action costs `time-units / f` seconds (Forward 7/f,
Incantation 300/f, …) and `f` is the `-f` server flag. **This server does not
implement per-action time costs** — commands execute immediately. Watchable
pacing is instead done client-side via the `ZAPPY_AI_DELAY` env var (seconds
between movement commands; `0.5` ≈ 2 tiles/s). See `ai/network/client.py`.
