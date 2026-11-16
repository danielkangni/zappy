"""ici c'est la mémoire du joueur."""

import random

"""Les 7 ressources du jeu."""
RESOURCES = ["food", "linemate", "deraumere", "sibur", "mendiane", "phiras", "thystame"]

"""Table des prérequis d'élévation  Pour passer du niveau N au niveau N+1, il faut réunir sur UNE MÊME case :
joueurs du niveau N et les quantités de pierres indiquées."""
ELEVATION_REQ = {
    1: {"players": 1, "linemate": 1, "deraumere": 0, "sibur": 0, "mendiane": 0, "phiras": 0, "thystame": 0},
    2: {"players": 2, "linemate": 1, "deraumere": 1, "sibur": 1, "mendiane": 0, "phiras": 0, "thystame": 0},
    3: {"players": 2, "linemate": 2, "deraumere": 0, "sibur": 1, "mendiane": 0, "phiras": 2, "thystame": 0},
    4: {"players": 4, "linemate": 1, "deraumere": 1, "sibur": 2, "mendiane": 0, "phiras": 1, "thystame": 0},
    5: {"players": 4, "linemate": 1, "deraumere": 2, "sibur": 1, "mendiane": 3, "phiras": 0, "thystame": 0},
    6: {"players": 6, "linemate": 1, "deraumere": 2, "sibur": 3, "mendiane": 0, "phiras": 1, "thystame": 0},
    7: {"players": 6, "linemate": 2, "deraumere": 2, "sibur": 2, "mendiane": 2, "phiras": 2, "thystame": 1},
}


class Player:
    """Représente l'état local d'UN joueur, tel que l'IA le connaît.
    Cette classe ne stocke que ce dont l'IA a besoin pour décider quoi faire au tour
    suivant : son niveau, son inventaire, et quelques drapeaux de coordination.
    """

    """seuil normal: pour un joueur qui collecte des pierres.
    En dessous, il part se ravitailler."""
    FOOD_THRESHOLD = 18
    """ seuil d'urgence: le joueur part se ravitailler immédiatement,
    peut importe ce qu'il est en train de faire."""
    CRITICAL_FOOD = 12
    """ seuil de ralliement: aumoin atteindre ce niveau avant
    de se rassembler pour l'élévation."""
    FOOD_RALLY_TARGET = 25
    """la patience max d'un joueur verrouillé au rendez-vous
    avant d'abandonner et de repartir se ravitailler. """
    LOCK_TIMEOUT = 60

    def __init__(self):
        # Niveau courant: démarre à 1.
        self.level = 1
        # Inventaire local (toutes les ressources à 0 au départ).
        self.inventory = {resource: 0 for resource in RESOURCES}

        # Compteur de pas effectués afin de faire changer de direction au joueur
        self.explore_steps = 0

        """Booléen qui passe à True sous CRITICAL_FOOD=12,
        repasse à False seulement quand on atteint FOOD_RALLY_TARGET=25.
        Ça évite qu'un joueur parte manger dès qu'il perd 1 food."""
        self.need_fuel = True
        
        """Booléen qui indique si le joueur est l'ancre du niveau courant.
        L'ancre est le joueur du niveau courant ayant le plus grand poids de ralliement"""
        self.is_anchor = False

        """Booléen qui indique si le joueur est verrouillé sur la case de l'ancre.
        Le verrouillage est levé à chaque changement de niveau"""
        self.locked = False

        """compteur de tours passés verrouillé sans voir de nouveaux joueurs arriver.
        Le compteur se remet à zéro dès qu'un joueur de plus arrive"""
        self.locked_ticks = 0
    
        """Compte le nbre de joueurs vus sur la case au dernier Look"""
        self.here_count = 0

        """compte le nbre de joueurs vus sur la case au dernier Look
        qui sont du même niveau que le joueur courant"""
        self.rally_seen = 0

        """Permet de tirer de manière aléatoire un joueur qui sera l'ancre du niveau courant."""
        self.rally_weight = random.randint(1, 1_000_000_000)

    def update_inventory(self, raw: str):
        """Met à jour l'inventaire local depuis la réponse du serveur.
        Le serveur renvoie [food 12, linemate 3, deraumere 0, ...]
        On enlève les crochets, on découpe sur les virgules, puis chaque morceau
        est un couple "nom-quantité" qu'on range dans self.inventory."""
        raw = raw.strip("[] \n")
        for item in raw.split(","):
            parts = item.strip().split()
            if len(parts) == 2:
                name, qty = parts
                if name in self.inventory:
                    self.inventory[name] = int(qty)

    def missing_for_elevation(self) -> dict:
        """compare l'inventaire aux prérequis du niveau courant (table ELEVATION_REQ).
        Renvoie un dict vide si tout est prêt."""
        if self.level >= 8:
            return {}
        req = ELEVATION_REQ[self.level]
        return {
            resource: req[resource] - self.inventory[resource]
            for resource in RESOURCES
            if resource != "food" and self.inventory[resource] < req.get(resource, 0)
        }

    def needs_food(self) -> bool:
        """True si la réserve de food est passée sous le seuil de survie normal."""
        return self.inventory["food"] < self.FOOD_THRESHOLD
