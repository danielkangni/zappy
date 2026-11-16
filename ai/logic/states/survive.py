import math

from network.client import Client
from logic.player import Player
from network.parser import parse_look
from logic.explore import explore


def _go_to_tile(client: Client, index: int):
    """Look numérote les cases en cône (0=joueur, ligne r = r² à (r+1)²-1).
    r = int(√index) → lignes à avancer ; col = index - r*(r+1) → décalage gauche/droite."""

    if index == 0:
        return  # on est déjà sur la case visée
    r = int(math.sqrt(index)) # numéro de ligne = nombre de pas en avant
    col = index - r * (r + 1) # décalage latéral (signé) par rapport au centre
    commands = ["Forward"] * r
    if col < 0: # case à gauche du centre
        commands.append("Left")
        commands.extend(["Forward"] * abs(col))
    elif col > 0:  # case à droite du centre
        commands.append("Right")
        commands.extend(["Forward"] * col)

    # On envoie tout le trajet d'un coup (le serveur accepte jusqu'à 10 commandes
    # en file), c'est plus rapide que d'attendre la réponse après chaque pas.
    client.send_batch(commands)


def survive(client: Client, player: Player):
    """Trouve de la nourriture et la ramasse. On regarde autour de soi,
    on va sur la case de food la plus proche, et on ramasse TOUTE la food qui s'y trouve.
    Si aucune food n'est en vue, on explore pour en chercher."""
    raw = client.send("Look") # renvoie une chaîne de caractères décrivant les cases autour du joueur
    tiles = parse_look(raw)

    # On parcourt les cases du plus proche au plus loin et on s'arrête à la première qui contient de la food.
    for index, tile in enumerate(tiles):
        count = tile.count("food")
        if count:
            _go_to_tile(client, index)
            for _ in range(count):
                client.send("Take food")
            return
    # Rien à manger en vue : on explore pour découvrir d'autres cases.
    explore(client, player)
