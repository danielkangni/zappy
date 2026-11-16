import random

"""Module d'exploration du joueur."""

from network.client import Client
from logic.player import Player


def explore(client: Client, player: Player):
    """Déplacement d'exploration quand il n'y a rien d'utile en vue.
    But: couvrir le terrain pour finir par tomber sur de la food ou des pierres,
    sans traverser la carte en ligne droite.
    Méthode: On avance tout le temps, et tous les 3 pas on tourne à gauche ou à droite au hasard.
    Ça fait un zigzag qui couvre mieux la carte qu'une ligne droite.
    """
    player.explore_steps += 1
    commands = []
    # Tous les 3 pas, on change de cap (gauche ou droite au hasard).
    if player.explore_steps % 3 == 0:
        commands.append(random.choice(["Left", "Right"]))
    commands.append("Forward")
    # Virage éventuel + avance, envoyés en une seule salve.
    client.send_batch(commands)
