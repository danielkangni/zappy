from network.client import Client
from logic.player import Player
from network.parser import parse_look
from logic.states.survive import _go_to_tile
from logic.explore import explore


def collect(client: Client, player: Player):
    """Va chercher les pierres qui manquent pour l'élévation du niveau courant.
    Stratégie simple et efficace :
      1. on calcule les pierres manquantes (niveau courant uniquement) ;
      2. on regarde autour de soi (Look) ;
      3. on va sur la PREMIÈRE case en vue qui contient une pierre utile, on la
         ramasse (et au passage la food qui s'y trouve) ;
      4. si rien d'utile n'est visible, on explore pour découvrir d'autres cases.
    On vise le niveau courant seulement (et non un gros stock d'avance) : la
    collecte reste courte, ce qui permet de vite repartir au regroupement plutôt
    que de passer tout son temps à amasser des pierres."""
    missing = player.missing_for_elevation()

    # Champ de vision actuel : liste de cases (voir parse_look).
    raw = client.send("Look")
    tiles = parse_look(raw)

    # On cherche la première case qui contient au moins une pierre manquante.
    for index, tile in enumerate(tiles):
        resources_here = [r for r in missing if r in tile]
        if resources_here:
            # Un seul déplacement vers cette case.
            _go_to_tile(client, index)
            # On ramasse toutes les pierres utiles présentes sur la case.
            for resource in resources_here:
                client.send(f"Take {resource}")
            # Bonus opportuniste : tant qu'on est là, on prend aussi la food.
            # Sans ça la réserve de food ne dépasse jamais le strict seuil de survie.
            for _ in range(tile.count("food")):
                client.send("Take food")
            return
    # Aucune pierre utile en vue : on explore pour couvrir d'autres zones.
    explore(client, player)
