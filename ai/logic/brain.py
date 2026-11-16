from network.client import Client
from logic.player import Player
from logic.states.survive import survive
from logic.states.collect import collect
from logic.states.elevate import elevate
from network.broadcast import decode, encode
from logic.fork import TeamManager
from logic.player import ELEVATION_REQ

"""Boucle de décision d'un joueur: à chaque tour, observer puis agir."""
DIRECTION_MOVES = {
    0: [], # déjà sur la case de l'émetteur
    1: ["Forward"], # devant
    2: ["Right", "Forward"], # devant-droite
    3: ["Right", "Forward"], # droite
    4: ["Right", "Right", "Forward"], # derrière-droite
    5: ["Right", "Right", "Forward"], # derrière
    6: ["Left", "Left", "Forward"], # derrière-gauche
    7: ["Left", "Forward"], # gauche
    8: ["Left", "Forward"], # devant-gauche
}

"""Seuil de food pour la reine """
QUEEN_FOOD_MIN = 30


class Brain:
    """Boucle de décision d'un joueur : à chaque tour, observer puis agir.
    Le déroulé d'un tour est toujours le même :
    1. la reine (s'il s'agit du process recruteur) maintient l'effectif ;
    2. on écoute les sons (broadcasts) pour décider du regroupement ;
    3. on relit niveau + inventaire depuis le serveur ;
    4. on choisit UNE action : manger, collecter, se regrouper, ou incanter."""
    
    def __init__(self, client: Client):
        self.client = client
        self.player = Player()

    def _handle_broadcasts(self):
        """Écoute les sons de l'équipe et décide du regroupement (ancre/suiveur).
        Principe du regroupement par ANCRE ÉLUE et chaque joueur diffuse en continu "NEED:equipe:niveau:poids"
        et en fonction du poids on sait vers qui se diriger. Comme le poids est un tirage aléatoire FIXE,
        l'ancre d'un niveau est unique et stable : tout le monde converge vers le même joueur, sans osciller."""

        # best_weight = le plus grand poids entendu jusqu'ici (on part du nôtre).
        # best_dir = direction du joueur qui porte ce plus grand poids :
        #   None -> personne de plus fort entendu (c'est donc NOUS l'ancre)
        #   0    -> ce joueur est sur NOTRE case (on est déjà au rendez-vous)
        #   1..8 -> direction vers laquelle se déplacer pour le rejoindre
        best_dir = None
        best_weight = self.player.rally_weight

        for raw in self.client.get_broadcasts():
            parsed = decode(raw)
            if parsed is None:
                continue
            direction, msg_type, data = parsed
            if msg_type != "NEED":
                continue

            # data = "equipe:niveau:poids"
            parts = data.split(":")
            try:
                sender_level = int(parts[1])
                sender_weight = int(parts[2])
            except (ValueError, IndexError):
                continue
            # On n'écoute que les joueurs de NOTRE niveau: ce sont les seuls avecqui on peut incanter.
            if sender_level != self.player.level:
                continue

            # On retient le plus haut poids, MÊME s'il vient de notre propre case
            # (direction 0). C'est essentiel : un suiveur déjà arrivé au rendez-vous
            # entend l'ancre en direction 0 et doit RESTER là. Sans ça, il
            # repartirait vers le 2e plus haut poids resté au loin.
            if sender_weight > best_weight:
                best_weight = sender_weight
                best_dir = direction
        # Si aucun poids supérieur au nôtre n'a été entendu, c'est nous l'ancre.
        self.player.is_anchor = (best_dir is None)

        """On ne se déplace vers l'ancre que si on est prêt (pierres + food) et PAS encore verrouillé."""
        ready = (
            not self.player.missing_for_elevation()
            and not self.player.need_fuel
            and ELEVATION_REQ.get(self.player.level, {}).get("players", 1) > 1
        )

        """on verouille le joueur sur la case de l'ancre mais on déverouille si on attend une meilleur ancre"""
        if ready and (self.player.is_anchor or best_dir == 0):
            self.player.locked = True
        elif ready and best_dir:
            self.player.locked = False
        # On ne se déplace vers l'ancre que si on est prêt et PAS encore verrouillé.
        if best_dir and ready and not self.player.locked:
            for move in DIRECTION_MOVES.get(best_dir, []):
                self.client.send(move)

    def run(self):
        """Boucle principale : un tour de jeu par itération, jusqu'au niveau 8."""

        """On crée un TeamManager pour gérer l'effectif de l'équipe (la reine) et on le tick à chaque tour."""
        team = TeamManager(self.client)
        prev_level = self.player.level

        while True:
            team.tick()

            """la reine ne se déplace jamais, elle reste sur sa case et ne mange que si la réserve est basse
            et elle ne collecte jamais de pierres (elle ne joue pas, elle supervise)."""
            if team.is_recruiter:
                raw = self.client.send("Inventory")
                self.player.update_inventory(raw)
                if self.player.inventory["food"] < QUEEN_FOOD_MIN:
                    survive(self.client, self.player)
                continue

            #Écouter les sons et, le cas échéant, avancer vers l'ancre.
            self._handle_broadcasts()

            #verifier si le niveau a changé (incantation réussie) et mettre à jour le niveau du joueur
            lvl = self.client.get_level_update()
            if lvl is not None:
                self.player.level = lvl

            # Relire l'inventaire (source de vérité pour food et pierres).
            raw = self.client.send("Inventory")
            self.player.update_inventory(raw)

            """lever le verrou si le niveau a changé et remettre à zéro le compteur de tours passés verrouillé"""
            if self.player.level != prev_level:
                self.player.locked = False
                self.player.locked_ticks = 0
                self.player.rally_seen = 0
                prev_level = self.player.level

            """On ne quitte pas la partie au niveau 8, on reste connecté pour compter pour la victoire."""
            if self.player.level >= 8:
                if self.player.inventory["food"] < QUEEN_FOOD_MIN:
                    survive(self.client, self.player)
                continue

            missing = self.player.missing_for_elevation()
            req = ELEVATION_REQ.get(self.player.level, {})
            multi = req.get("players", 1) > 1  # verifie si le niveau est un niveau de groupe ou solo
            food = self.player.inventory["food"]

            """le cri que chaque joueur envoie à chaque tour pour se faire trouver."""
            if multi:
                self.client.send(
                    f"Broadcast {encode('NEED', f'{self.client.team}:{self.player.level}:{self.player.rally_weight}')}"
                )

            """On ne se déplace jamais vers l'ancre si on est en famine critique, 
            même si on est l'ancre, on se déplace vers la food"""
            if food < self.player.CRITICAL_FOOD:
                self.player.need_fuel = True
            elif food >= self.player.FOOD_RALLY_TARGET:
                self.player.need_fuel = False

            """quand on est verrouillé, on regarde si le nbre de joueurs sur la case a augmenté depuis le verrouillage
            si oui on remet le compteur de tours passés à zéro, sinon on incrémente"""
            if self.player.locked:
                if self.player.here_count > self.player.rally_seen:
                    self.player.rally_seen = self.player.here_count
                    self.player.locked_ticks = 0
                else:
                    self.player.locked_ticks += 1
                    if self.player.locked_ticks >= self.player.LOCK_TIMEOUT:
                        self.player.locked = False
                        self.player.locked_ticks = 0
                        self.player.rally_seen = 0
                        self.player.need_fuel = True
            else:
                self.player.locked_ticks = 0
                self.player.rally_seen = 0

            if multi and self.player.locked:
                """On est sur la case de l'ancre, on est prêt, on attend le quorum et on incante dès qu'il est atteint."""
                elevate(self.client, self.player)
            elif multi and self.player.is_anchor:
                """l'ancre une fois figée ne se déplace jamais sauf si elle est en famine critique,
                dans ce cas elle va chercher de la food"""
                if food < self.player.CRITICAL_FOOD:
                    survive(self.client, self.player)
                else:
                    elevate(self.client, self.player)
            else:
                # Suiveur, ou joueur d'un niveau "solo" (niveau 1).
                if (not missing) and multi:
                    # Pierres déjà réunies : on est en attente au rendez-vous. On ne
                    # repart manger que si la réserve est vraiment basse (hystérésis)
                    # pour ne pas casser le regroupement.
                    must_eat = self.player.need_fuel
                else:
                    # En collecte (ou niveau solo) : on garde le coussin normal.
                    must_eat = self.player.needs_food()

                if must_eat:
                    survive(self.client, self.player)
                elif missing:
                    collect(self.client, self.player)
                else:
                    elevate(self.client, self.player)
