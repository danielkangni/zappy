# Protocol quick reference

One-page cheat sheet for both protocols. Full details: `ai_protocol.md`,
`gui_protocol.md`. Resource order everywhere: `0 food · 1 linemate · 2 deraumere
· 3 sibur · 4 mendiane · 5 phiras · 6 thystame`.

## AI client → server

| command | reply |
|---|---|
| `Forward` `Right` `Left` | `ok` |
| `Look` | `[t0,t1,…]` |
| `Inventory` | `[food n, linemate n, …]` |
| `Take object` / `Set object` | `ok` / `ko` |
| `Broadcast text` | `ok` |
| `Fork` | `ok` |
| `Eject` | `ok` / `ko` |
| `Connect_nbr` | `N` |
| `Incantation` | `Elevation underway` → `Current level: K` (or `ko`) |

Handshake: `WELCOME` → send team name → `CLIENT-NUM` → `X Y`.

## Server → GUI (GRAPHIC)

| msg | format | | msg | format |
|---|--Few or zero competing teams, long run, food-dense map (more food = more survivable waits).
-|---|---|---|
| `msz` | `msz X Y` | | `pgt` | `pgt #id i` (took) |
| `sgt` | `sgt T` | | `pdr` | `pdr #id i` (dropped) |
| `tna` | `tna N` | | `pic` | `pic X Y L #id…` |
| `bct` | `bct X Y q0..q6` | | `pie` | `pie X Y R` |
| `pnw` | `pnw #id X Y O L team` | | `pbc` | `pbc #id message` |
| `ppo` | `ppo #id X Y O` | | `pfk` | `pfk #id` |
| `plv` | `plv #id L` | | `enw` | `enw #egg #player X Y` |
| `pin` | `pin #id X Y q0..q6` | | `edi` | `edi #egg` |
| `pdi` | `pdi #id` | | `pex` | `pex #id` |
| `seg` | `seg team` | | | |

Handshake: `WELCOME` → send `GRAPHIC` → bootstrap (`msz`, `tna…`, `bct…`,
`pnw…`, `sgt`, `sst`).

GUI → server request: `pin #id` → server replies `pin #id X Y q0..q6`.

## Custom extensions (this project)

| msg | format | purpose |
|---|---|---|
| `sst` | `sst S` | seconds to next resource reset (countdown + tree pulse); GUI-only |

## Notes

- Map is a **torus** (Forward wraps at the edges).
- `Current level: K` goes to **all** incantation participants; the AI buffers it
  out-of-band to avoid desync.
- This server executes commands **instantly** (no `T/f` time cost); pacing is
  client-side via `ZAPPY_AI_DELAY`.
