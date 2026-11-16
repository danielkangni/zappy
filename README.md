# Zappy

Jeu multijoueur en réseau réalisé en équipe à EPITECH Bénin (2026). Des équipes de joueurs pilotés par une IA évoluent sur une carte torique : ils récoltent de la nourriture et des pierres, puis se regroupent pour lancer des incantations qui les font monter de niveau, jusqu'au niveau 8.

Le projet se compose de trois programmes qui communiquent par TCP :

| Dossier | Programme | Rôle |
|---|---|---|
| [`server/`](server/) | `zappy_server` (C++) | Serveur temps réel multi-clients non bloquant : simulation du monde, file de commandes par joueur, gestion du temps |
| [`ai/`](ai/) | `zappy_ai` (Python) | Client autonome : survie, collecte, coordination par broadcast et incantations entre équipes |
| [`gui/`](gui/) | `zappy_gui` (C++, OpenGL, SFML) | Visualisation 3D en direct de la partie : îles, joueurs, incantations, cycle jour/nuit, panneau d'événements |

Les protocoles sont décrits dans [`protocol/`](protocol/) et le fonctionnement de l'IA dans [`ai/DOCUMENTATION.md`](ai/DOCUMENTATION.md).

## Lancer une démo

Dépendances (Ubuntu) : `g++`, `make`, `python3`, `libsfml-dev`, `libglew-dev`, `libglm-dev`, `curl`.

```bash
make            # compile le serveur et la GUI
./run_demo.sh   # serveur + 2 équipes d'IA + GUI
```

Les modèles 3D et textures de la GUI sont téléchargés depuis un CDN au premier lancement.

## Équipe

Projet d'équipe EPITECH. Contributeurs, par nombre de commits sur le dépôt d'origine :

- Knight-coder-19
- KANGNI Jean-Daniel (Black_nexus06) : cœur du serveur et protocole serveur / IA / interface
- chabda01
- Huziane TONAWA
- Ridwane Nico
- Randy
- Espoir Biaou

Ce dépôt est une copie publique du code final, publiée avec l'accord de l'équipe. Les sujets officiels EPITECH n'y figurent pas.
