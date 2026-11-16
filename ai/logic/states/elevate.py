from network.client import Client
from network.parser import parse_look
from logic.player import Player, ELEVATION_REQ

STONES = ["linemate", "deraumere", "sibur", "mendiane", "phiras", "thystame"]


def _topup_stones(client: Client, player: Player, first_tile: list):
    """Dépose sur la case SEULEMENT ce qui manque pour atteindre les prérequis.
    Mise en commun des pierres : avant d'incanter, on regarde ce qui est DÉJÀ sur
    la case et on ne complète que le déficit, dans la limite de ce qu'on porte.
    Conséquences: chaque joueur présent contribue à tour de rôle et on ne dépose jamais en trop, donc rien n'est gaspillé."""
    req = ELEVATION_REQ.get(player.level, {})
    for stone in STONES:
        deficit = req.get(stone, 0) - first_tile.count(stone)
        held = player.inventory.get(stone, 0)
        # On dépose une à une les pierres manquantes qu'on possède réellement.
        for _ in range(min(deficit, held)):
            client.send(f"Set {stone}")
            player.inventory[stone] -= 1  # on garde l'inventaire local cohérent


def _do_incantation(client: Client, player: Player):
    """Complète les pierres manquantes sur la case puis lance l'incantation.
    Déroulé côté serveur: "Incantation" gèle les participants pendant 300 ticks,
    puis renvoie "Current level: N" en cas de succès ou "ko" si les conditions ne
    sont plus réunies, par exemple s'il manque un joueur ou une pierre."""
    # On relit la case et on complète le déficit de pierres avant d'incanter.
    raw = client.send("Look")
    tiles = parse_look(raw)
    _topup_stones(client, player, tiles[0] if tiles else [])

    # Phase 1: démarrage. "ko" => les conditions ne sont pas réunies, on abandonne.
    start_msg = client.send("Incantation", is_incantation=True)
    if start_msg == "ko":
        return

    # Phase 2: on attend le résultat final (300 ticks plus tard).
    end_msg = client.receive(is_incantation=True)
    if end_msg.startswith("Current level"):
        try:
            player.level = int(end_msg.split(":")[1].strip())
        except (IndexError, ValueError):
            pass


def elevate(client: Client, player: Player):
    """Tente de faire monter le joueur d'un niveau.
    Deux cas: le niveau "solo" où on incante immédiatement et
    niveau de groupe où on attend d'avoir le quorum de joueurs sur la case, en
    mangeant la food présente pour tenir, puis on incante dès qu'il est atteint.
    """
    req = ELEVATION_REQ.get(player.level, {})
    needed = req.get("players", 1)

    # Niveau 1 -> 2 : un seul joueur suffit, on incante sans attendre personne.
    if needed == 1:
        _do_incantation(client, player)
        return

    """on regarde autour de soit pour savoir combien de joueurs sont présents sur la case.+"""
    raw = client.send("Look")
    tiles = parse_look(raw)
    """on récupère le nbre de joueurs sur la case puis le nbre de joueurs total"""
    players_here = max(0, tiles[0].count("player") - 1) if tiles else 0
    here = players_here + 1

    """On mémorise combien de joueurs sont présents sur la case
    et le brain s'en sert comme signal de PROGRÈS pour sa patience"""
    player.here_count = here

    """on consomme la food et on envoie le signal au server"""
    if tiles:
        for _ in range(tiles[0].count("food")):
            client.send("Take food")

    """on monte le niveau de tous les joueurs du même niveau sur la case"""
    if here >= needed:
        _do_incantation(client, player)
        return
    