# Documentation de l'IA Zappy (`zappy_ai`)

Cette documentation explique **comment l'IA fonctionne** et **ce que fait chaque fonction**, fichier par fichier.

---

## 1. Vue d'ensemble

`zappy_ai` est le client autonome qui pilote **un seul joueur** (un « drone ») sur le serveur Zappy. Pour faire vivre une équipe entière, on lance **plusieurs process** `zappy_ai` (le premier en construit d'autres via la commande `Fork` du jeu).

**Objectif du jeu :** faire monter les joueurs en niveau (1 → 8) via des **incantations**. Une incantation exige, sur **une même case**, un certain nombre de **joueurs du même niveau** + un certain nombre de **pierres** (voir la table `ELEVATION_REQ`).

**Principe de fonctionnement d'un joueur** — une boucle infinie qui, à chaque tour :
1. gère l'effectif de l'équipe (créer/remplacer des coéquipiers) ;
2. écoute les messages (sons) et se dirige vers le point de ralliement ;
3. décide quoi faire : **manger** (survie), **collecter** des pierres, ou **incanter**.

**Contrainte du sujet respectée :** les clients **ne communiquent jamais hors du jeu**. Toute coordination passe par le **serveur** (broadcasts sonores + commande `Connect_nbr`). La seule information transmise à un sous-process est son rôle de lancement (variable d'env `ZAPPY_CHILD`), pas une donnée de jeu.

---

## 2. Arborescence

```
ai/
├── zappy_ai                 # point d'entrée (exécutable Python)
├── network/
│   ├── client.py            # connexion TCP + envoi/réception des commandes
│   ├── parser.py            # parsing de la réponse "Look"
│   └── broadcast.py         # encodage/décodage des messages broadcast
└── logic/
    ├── brain.py             # boucle principale + ralliement (homing)
    ├── player.py            # état du joueur + table d'élévation
    ├── fork.py              # gestion de l'effectif d'équipe (TeamManager)
    ├── explore.py           # déplacement d'exploration
    └── states/
        ├── survive.py       # aller manger (+ helper de déplacement)
        ├── collect.py       # aller chercher les pierres manquantes
        └── elevate.py       # rassemblement + incantation
```

---

## 3. Le protocole serveur (rappel)

**Connexion (handshake) :**
```
serveur → WELCOME
client  → <nom_equipe>
serveur → <CLIENT-NUM>      (nb de slots encore libres pour l'équipe)
serveur → <X> <Y>          (dimensions de la carte)
```

**Commandes utilisées** (réponse entre parenthèses) :
`Forward`, `Right`, `Left` (ok) · `Look` (`[tuile0, tuile1, ...]`) · `Inventory` (`[food n, linemate n, ...]`) · `Take <objet>` / `Set <objet>` (ok/ko) · `Broadcast <texte>` (ok) · `Fork` (ok) · `Connect_nbr` (un nombre) · `Incantation` (`Elevation underway` puis `Current level: K`, ou `ko`).

**Messages asynchrones** (peuvent arriver à tout moment) : `message K, <texte>` (broadcast reçu, `K` = direction du son), `eject: K`, `dead`.

---

## 4. La boucle principale (résumé)

Dans `Brain.run()` (voir [logic/brain.py](logic/brain.py)), à chaque tour :

```
team.tick()                # 1. effectif : créer/remplacer les coéquipiers
_handle_broadcasts()       # 2. lire les sons → se diriger vers l'ancre (homing)
get_level_update()         # 3. récupérer un éventuel passage de niveau
Inventory                  # 4. mettre à jour l'inventaire
si niveau >= 8 : STOP
                           # 5. décision :
   doit manger ?  → survive()      (food trop basse)
   sinon manque des pierres ? → collect()
   sinon          → elevate()      (rassemblement + incantation)
```

---

## 5. Détail des fichiers et fonctions

### `zappy_ai` — point d'entrée

| Fonction | Rôle |
|---|---|
| `parse_args()` | Lit les arguments **`-p` port (obligatoire)**, **`-n` nom d'équipe (obligatoire)**, **`-h` hôte (défaut `localhost`)**. Conforme à l'usage du sujet. |
| `main()` | Crée le `Client`, se connecte, crée le `Brain` et lance `brain.run()`. Capture `ConnectionError`/`BrokenPipeError`/`KeyboardInterrupt` pour sortir proprement quand le serveur ferme, que le joueur meurt, ou sur Ctrl-C. |

---

### `network/client.py` — couche réseau (`class Client`)

Gère la socket TCP et l'envoi/réception des commandes. Bufferise les broadcasts et les passages de niveau reçus en asynchrone.

**Attributs clés :** `host`, `port`, `team`, `map_x/map_y` (taille carte), `free_slots` (slots libres au handshake), `_broadcasts` (file des sons reçus), `_pending_level` (niveau reçu en asynchrone).

**Variables d'environnement :**
- `ZAPPY_AI_DELAY` : durée d'un déplacement en secondes (cadence la vitesse **visible** dans le GUI ; `0` = plein régime, `0.5` ≈ 2 cases/s).
- `ZAPPY_DEBUG=1` : affiche tout le trafic (`>>` envoyé, `<<` reçu).

| Fonction | Rôle |
|---|---|
| `connect()` | Ouvre la socket, fait le handshake (WELCOME → nom → CLIENT-NUM → dimensions). Remplit `free_slots`, `map_x`, `map_y`. |
| `send(cmd, is_incantation=False)` | Envoie une commande **et** lit sa réponse (renvoie la réponse). |
| `receive(is_incantation=False)` | Lit une réponse **sans** envoyer (utilisé pour la 2ᵉ phase de l'incantation). |
| `send_batch(cmds)` | Envoie une liste de commandes par **paquets de 10** (limite du serveur : 10 requêtes sans réponse), puis lit toutes les réponses. Sert au déplacement « pipeline ». |
| `get_broadcasts()` | Renvoie et **vide** la file des sons reçus (consommée par le brain). |
| `get_level_update()` | Renvoie le niveau reçu en asynchrone (`Current level` non sollicité, ex. on est participant d'une incantation lancée par un autre), puis l'efface. |
| `connect_nbr()` | Envoie `Connect_nbr` et renvoie le **nombre d'œufs** de l'équipe en attente d'éclosion. Sert de **sémaphore** au `TeamManager` (un seul œuf en vol à la fois). |
| `_write(msg)` | Écrit sur la socket. **Cadence uniquement les `Forward`** selon `ZAPPY_AI_DELAY` (les `Look`/`Inventory`/`Take`… restent rapides). |
| `_read_line()` | Lit une ligne (jusqu'au `\n`) ; lève `ConnectionError` si le serveur ferme. |
| `_read_response(is_incantation)` | Lit la réponse d'une commande en **filtrant les messages asynchrones** : `message …` → mis dans `_broadcasts` ; `Current level`/`Elevation underway` → renvoyés si on attend une incantation, sinon stockés/ignorés ; `eject:` → ignoré ; `dead` → `ConnectionError`. |

---

### `network/parser.py`

| Fonction | Rôle |
|---|---|
| `parse_look(raw)` | Transforme la réponse `Look` (`"[player food, , linemate, ...]"`) en **liste de tuiles**, chaque tuile étant une **liste d'objets** (ex. `[["player","food"], [], ["linemate"], ...]`). La tuile 0 = la case du joueur. |

---

### `network/broadcast.py`

Nos messages d'équipe ont le format `TYPE:donnees` (ex. `NEED:team1:4:837421`).

| Fonction | Rôle |
|---|---|
| `encode(msg_type, data)` | Construit `"TYPE:data"` à diffuser via `Broadcast`. |
| `decode(raw)` | Parse un message reçu `"message K, TYPE:data"` → renvoie le triplet **`(direction K, TYPE, data)`** (ou `None` si ce n'est pas un broadcast). `K` est la direction du son (0 = même case, 1-8 autour). |

---

### `logic/player.py` — état du joueur

**`RESOURCES`** : la liste des 7 ressources (food + 6 pierres).

**`ELEVATION_REQ`** : la **table d'élévation** (du sujet) — pour chaque niveau courant, le nombre de **joueurs** requis et la quantité de chaque **pierre** :

| Niveau | joueurs | linemate | deraumere | sibur | mendiane | phiras | thystame |
|---|---|---|---|---|---|---|---|
| 1→2 | 1 | 1 | 0 | 0 | 0 | 0 | 0 |
| 2→3 | 2 | 1 | 1 | 1 | 0 | 0 | 0 |
| 3→4 | 2 | 2 | 0 | 1 | 0 | 2 | 0 |
| 4→5 | 4 | 1 | 1 | 2 | 0 | 1 | 0 |
| 5→6 | 4 | 1 | 2 | 1 | 3 | 0 | 0 |
| 6→7 | 6 | 1 | 2 | 3 | 0 | 1 | 0 |
| 7→8 | 6 | 2 | 2 | 2 | 2 | 2 | 1 |

**Constantes de food (`class Player`) :**
- `FOOD_THRESHOLD = 18` : seuil normal en dessous duquel un joueur **en collecte** va manger.
- `CRITICAL_FOOD = 12` : seuil critique (un joueur au rendez-vous ne repart manger qu'en dessous).
- `FOOD_RALLY_TARGET = 25` : niveau de food jusqu'auquel on fait le **plein** (hystérésis) avant de se figer au rendez-vous.
- `LOCK_TIMEOUT = 60` : patience max (en tours) d'un joueur verrouillé sans progrès avant d'abandonner le rendez-vous.

**Attributs clés :** `level`, `inventory`, `need_fuel` (hystérésis food), `is_anchor` (suis-je l'ancre de mon niveau ?), `rally_weight` (poids aléatoire fixe pour l'élection d'ancre), `explore_steps` (compteur pour l'exploration en zig-zag), `locked` (verrouillé au rendez-vous ?), `locked_ticks` (tours sans progrès depuis le verrou), `here_count` (joueurs sur ma case actuellement), `rally_seen` (max de joueurs vus depuis le verrou).

| Méthode | Rôle |
|---|---|
| `update_inventory(raw)` | Parse la réponse `Inventory` et met à jour `self.inventory`. |
| `missing_for_elevation()` | Renvoie les pierres manquantes pour le **niveau courant uniquement** (ignore la food). `{}` si niveau ≥ 8. C'est la seule cible de collecte : on collecte court, on monte, on recommence. |
| `needs_food()` | `True` si `food < FOOD_THRESHOLD`. |

---

### `logic/brain.py` — cerveau (`class Brain`)

**`DIRECTION_MOVES`** : table qui traduit la **direction d'un son** (numéro de tuile K, 1-8, renvoyé par le serveur) en **séquence de déplacements** pour se rapprocher de la source :

| K (direction du son) | mouvements |
|---|---|
| 0 (même case) | — |
| 1 (devant) | `Forward` |
| 2, 3 (droite) | `Right, Forward` |
| 4, 5 (arrière) | `Right, Right, Forward` |
| 6 (arrière-gauche) | `Left, Left, Forward` |
| 7, 8 (gauche) | `Left, Forward` |

| Méthode | Rôle |
|---|---|
| `__init__(client)` | Crée le `Player`. |
| `_handle_broadcasts()` | **Ralliement (homing).** Lit tous les sons reçus. Pour les `NEED` **de notre niveau** (toutes équipes confondues, car une incantation réunit les joueurs de même niveau quelle que soit l'équipe), repère le **plus haut `weight`** entendu (élection d'ancre). Si quelqu'un a un poids supérieur au nôtre, on est un **suiveur** → on se dirige vers lui. Sinon **on est l'ancre** (`is_anchor = True`). On retient le plus haut poids **même en direction 0** (sur notre case) → un suiveur déjà arrivé **reste** au lieu d'être réaspiré ailleurs. On ne se déplace que si on est **`ready`** (plus rien à collecter pour le niveau courant, food OK, niveau ≥ 2 joueurs). Dès qu'on est prêt et au rendez-vous (ancre ou direction 0), on se **verrouille** (`locked = True`). |
| `run()` | **Boucle principale** (voir §4). Crée le `TeamManager`. Si ce process est la **reine** (`is_recruiter`) : il se contente de survivre (manger sous `QUEEN_FOOD_MIN=30`) + `team.tick()`. Sinon (ouvrier), à chaque tour : `team.tick()` → `_handle_broadcasts()` → maj niveau → `Inventory` → arrêt si niveau 8 → diffusion du **beacon `NEED`** (si niveau de groupe) → maj hystérésis food → **patience progressive du verrou** → **décision** : verrouillé → `elevate` ; ancre → `elevate` (ou `survive` si famine critique) ; sinon `survive`/`collect`/`elevate`. |

**Logique de décision (dans `run`)** :
- **Verrouillé** au rendez-vous (`locked`) : on ne bouge plus et on n'part jamais manger → `elevate()` (incante dès quorum).
- **Ancre** non verrouillée : se **fige** comme point de rendez-vous (`elevate`), et ne part manger (`survive`) que sous `CRITICAL_FOOD`.
- `need_fuel` passe à `True` sous `CRITICAL_FOOD`, à `False` à `FOOD_RALLY_TARGET`, et **garde son état entre les deux** (hystérésis) → un suiveur engagé au rendez-vous n'en repart pas dès qu'il perd 1 food.
- Suiveur **prêt** (pierres OK, niveau multi-joueurs) : `must_eat = need_fuel` ; sinon `must_eat = needs_food()` (seuil normal). Puis `survive` / `collect` / `elevate`.

**Patience progressive du verrou** : tant que `locked`, si `here_count > rally_seen` (un joueur de plus vient d'arriver) → `locked_ticks = 0` (le groupe grossit, on attend). Sinon `locked_ticks += 1` ; au-delà de `LOCK_TIMEOUT (60)` tours sans progrès → abandon : `locked = False`, `need_fuel = True`, on repart se ravitailler et on retentera. Un changement de niveau lève automatiquement le verrou.

---

### `logic/fork.py` — gestion de l'effectif (`class TeamManager`)

**`TEAM_CAP = 9`** : plafond **dur** de joueurs par équipe (jamais dépassé). **`REFORK_BELOW = 7`** : seuil de réapprovisionnement (hystérésis) — la reine ne reforke que lorsque l'effectif tombe **sous 7**, puis rebâtit jusqu'à 9 (7 = 1 reine + 6 ouvriers = quorum des niveaux 6-7). **`BUILD_FORK_COOLDOWN = 2.0`** : délai entre deux créations (le temps qu'un œuf éclose).

| Élément | Rôle |
|---|---|
| `_spawn(client)` | Lance un nouveau process `zappy_ai` (avec `-p/-n/-h`) et **renvoie son handle** (pour le surveiller). L'enfant reçoit `ZAPPY_CHILD=1` (un simple **rôle de lancement**, pas une donnée de jeu) → il ne recrute pas, il joue. À la connexion, le serveur lui donne un slot libre, sinon fait éclore un œuf. |
| `TeamManager.__init__` | Détermine si ce process est le **recruteur** (`is_recruiter` = lancé à la main, sans `ZAPPY_CHILD`). Mémorise les `free_slots` initiaux. |
| `TeamManager.tick()` | **Construit et maintient l'effectif (hystérésis 7→9).** Seul le **recruteur** (la reine) agit. Il : (1) **retire** de sa liste les enfants dont le **process est mort** (= joueur mort) ; (2) calcule l'effectif `1 (reine) + enfants vivants` ; (3) passe en mode **rebuild** quand l'effectif tombe sous `REFORK_BELOW` (7) et en sort à `TEAM_CAP` (9) ; (4) en mode rebuild et cooldown écoulé : crée un coéquipier — via un **slot initial** restant, sinon un **œuf** (`Fork`). **Point clé :** on ne lance l'enfant que **quand un slot/œuf existe** (slot dispo, ou `connect_nbr() > 0` = œuf prêt) ; sinon on `Fork` d'abord et l'enfant viendra au tour suivant. Ça évite que des enfants lancés trop tôt reçoivent « ko » et restent **bloqués** (fuite de process). |

> **Comment l'équipe est réapprovisionnée :** le recruteur surveille les **handles de process** de ses enfants. Quand un joueur meurt, son process se termine ; `tick()` le détecte (`poll()`) et le retire. Dès que l'effectif tombe **sous 7**, la reine rebâtit jusqu'à **9** (Fork + nouveaux process). Plafonné à 9 (jamais plus).

---

### `logic/states/survive.py` — manger

| Fonction | Rôle |
|---|---|
| `_go_to_tile(client, index)` | **Calcule et exécute le trajet** vers la tuile `index` de la vision (`Look`). Géométrie : la ligne `r = floor(√index)`, le décalage latéral `col = index − r(r+1)` (négatif = gauche). Envoie `Forward × r` + un virage + `Forward × |col|` en une salve (`send_batch`). Helper réutilisé par `collect` et `elevate`. |
| `survive(client, player)` | `Look`, va sur la **case de food la plus proche** et ramasse **toute** la food présente. Si aucune food en vue → `explore()`. |

---

### `logic/states/collect.py` — collecter les pierres

| Fonction | Rôle |
|---|---|
| `collect(client, player)` | Calcule les pierres manquantes du **niveau courant** (`missing_for_elevation`). `Look`, va sur la **première case** contenant une pierre manquante, la ramasse (+ la food présente au passage). Si rien en vue → `explore()`. |

---

### `logic/states/elevate.py` — rassemblement + incantation

`STONES` : les 6 pierres (sans la food).

| Fonction | Rôle |
|---|---|
| `_topup_stones(client, player, first_tile)` | **Dépôt mutualisé.** Pose sur la case **seulement le déficit** pour atteindre les prérequis du niveau : on compte ce qui est **déjà** sur la case (déposé par les coéquipiers) et on n'ajoute que ce qui manque, dans la limite de ce qu'on porte. → chacun contribue, pas de gaspillage. |
| `_do_incantation(client, player)` | `Look` + `_topup_stones` (s'assurer que la case a les pierres) + envoie `Incantation` (2 phases : `Elevation underway`, puis 300 ticks plus tard `Current level: K`). Si succès, met à jour `player.level`. |
| `elevate(client, player)` | Si le niveau exige **1 joueur** → incante directement. Sinon : `Look`, compte les joueurs présents (`here`), **mange la food de la case** (auto-alimentation pendant l'attente). Si **quorum atteint** (`here ≥ needed`) → incante. Sinon → patiente sur place (le beacon `NEED` est désormais diffusé par `run()` à chaque tour, plus ici). |

---

### `logic/explore.py`

| Fonction | Rôle |
|---|---|
| `explore(client, player)` | Déplacement d'exploration : avance, et **tourne aléatoirement toutes les 3 avancées** pour ne pas traverser la carte en ligne droite (et couvrir les côtés). |

---

## 6. Concepts clés (stratégies)

- **Ralliement par élection d'ancre.** Chaque joueur a un `rally_weight` aléatoire fixe. Les joueurs prêts diffusent `NEED:team:level:weight`. Celui du **plus haut poids** reste immobile (ancre) ; les autres se dirigent vers lui grâce à la **direction du son** (`DIRECTION_MOVES`). Un seul attracteur stable → convergence sans oscillation.

- **Verrouillage au rendez-vous (lock).** Dès qu'un joueur est prêt et sur la case de l'ancre, il se verrouille : il ne bouge plus et ne repart jamais manger. La patience est **progressive** : si le groupe grossit (`here_count` augmente), le compteur repart à zéro. Si personne n'arrive depuis `LOCK_TIMEOUT (60)` tours → abandon + ravitaillement + réessai. Évite l'orbite autour de l'ancre et les départs prématurés pour manger qui empêchaient le quorum de se former.

- **Beacon continu.** Le `NEED` est diffusé **à chaque tour** : la direction d'un son (numéro de tuile) dépend de la position/orientation **courante** du récepteur ; une vieille direction est inutilisable. Émettre en continu donne au suiveur une direction fraîche à chaque pas.

- **Collecte au niveau courant seulement.** On ne collecte que les pierres du niveau courant (`missing_for_elevation()`). La collecte reste courte → le joueur rejoint rapidement le rendez-vous, incante, puis recommence pour le niveau suivant.

- **Hystérésis de food.** Deux seuils (`CRITICAL_FOOD=12`, `FOOD_RALLY_TARGET=25`) : on fait le plein, puis on **reste** au rendez-vous tant qu'on n'est pas retombé au seuil bas → évite l'oscillation « manger ↔ rallier ».

- **Dépôt mutualisé des pierres.** À l'incantation, chaque joueur présent ne pose que le **déficit** → pas de gaspillage, ce ne sont pas les ressources d'un seul qui sont prises.

- **Gestion d'effectif (TeamManager).** Un **recruteur** par équipe construit l'effectif à **9** et le **maintient** (remplace les morts en surveillant les process), plafonné à 9. Régulé par le serveur (`Connect_nbr` = sémaphore d'œuf).

- **Conformité au sujet.** Aucune donnée de jeu n'est échangée hors du serveur. L'effectif est piloté par le serveur (slots du handshake + `Connect_nbr`). La seule info passée à un sous-process est `ZAPPY_CHILD` (un rôle de lancement, pas une donnée de jeu).

---

## 7. Résultats et limites

- **Niveau 8 atteint.** Le système de verrouillage au rendez-vous + patience progressive a permis d'atteindre le niveau 8 (9 joueurs ont atteint le niveau 8 lors d'un run live avec 4 équipes, carte 10×10, f=100).
- **Correctif serveur.** La food n'est plus consommée pendant les 300 ticks de gel de l'incantation (`isIncanting` dans World.cpp) → évite les morts de faim pendant le rituel 7→8.
- **Point dur résiduel.** Réunir 6 joueurs de même niveau sur une case exacte via le son (direction approximative à 1 parmi 8) reste aléatoire. Le succès dépend de la taille de la carte et de la variance des runs.

---

## 8. Lancer l'IA

**Un joueur :**
```bash
cd ai && python3 zappy_ai -p 4242 -n team1 -h localhost
```
*(Le premier process lancé est le « recruteur » : il construit le reste de l'équipe via `Fork`.)*

**Démos fournies** (depuis la racine du dépôt) :
```bash
./run_demo.sh        # serveur + plusieurs équipes + GUI
./test_fork.sh       # test de la reproduction (Fork) : l'effectif grandit
```
