# GUI (GRAPHIC) protocol — server ⇄ graphic client

Reference for the messages exchanged with a **GRAPHIC** client (the GUI). This
documents what the server in this repo **actually implements** (see
`server/src/World.cpp` / `server/src/Server.cpp`).

Resource indices everywhere are: `0 food · 1 linemate · 2 deraumere · 3 sibur ·
4 mendiane · 5 phiras · 6 thystame`. `#id` is a numeric id with a leading `#`.

## Handshake

```
server → WELCOME\n
client → GRAPHIC\n
server → (bootstrap, see below)
```

On `GRAPHIC` the server immediately sends the world bootstrap:

| order | message | meaning |
|---|---|---|
| 1 | `msz X Y\n` | map size |
| 2 | `tna N\n` (one per team) | team names |
| 3 | `bct X Y q0 q1 q2 q3 q4 q5 q6\n` (every tile) | tile contents |
| 4 | `pnw #id X Y O L team\n` (every player) | current players |
| 5 | `sgt T\n` | time unit / frequency |
| 6 | `sst S\n` | seconds until next resource reset *(custom, see below)* |

## Server → GUI events

| message | format | when |
|---|---|---|
| `msz` | `msz X Y` | map dimensions (bootstrap) |
| `sgt` | `sgt T` | server time unit (frequency `-f`) |
| `tna` | `tna N` | one per team name |
| `bct` | `bct X Y q0..q6` | a tile's content (bootstrap + whenever it changes) |
| `pnw` | `pnw #id X Y O L team` | new player connected (O=orientation 1‑4, L=level) |
| `ppo` | `ppo #id X Y O` | a player moved / turned |
| `plv` | `plv #id L` | a player's level changed (elevation) |
| `pin` | `pin #id X Y q0..q6` | a player's inventory *(reply to a GUI request, see below)* |
| `pgt` | `pgt #id i` | player took resource `i` off its tile |
| `pdr` | `pdr #id i` | player dropped resource `i` on its tile |
| `pic` | `pic X Y L #id #id …` | incantation begins on a tile (L=level, ids=participants) |
| `pie` | `pie X Y R` | incantation ends (R=1 success / 0 fail) |
| `pbc` | `pbc #id message` | player broadcast `message` |
| `pfk` | `pfk #id` | player forked (lays an egg) |
| `enw` | `enw #egg #player X Y` | new egg laid |
| `edi` | `edi #egg` | egg removed |
| `pex` | `pex #id` | player was ejected |
| `pdi` | `pdi #id` | player died / disconnected |
| `seg` | `seg team` | end of game — `team` won |

> Tile/inventory updates are authoritative: after any `pgt`/`pdr`/elevation the
> server also re-sends the affected `bct`, so the GUI can rebuild tile contents
> from `bct` alone.

## GUI → server requests

| request | reply | meaning |
|---|---|---|
| `pin #id` | `pin #id X Y q0..q6` | ask for a player's current inventory |

## Custom extension: `sst`

`sst S\n` is a **non-standard** message added in this project. The server runs a
resource-reset cycle (default every 5 min); `sst` carries the number of seconds
until the next reset. It is sent on GUI connect and again at each reset. The GUI
uses it for the on-screen countdown and the tree-of-life pulse. AI clients never
receive it, so standard protocol behaviour is unchanged.
