import os
import sys
import time
import subprocess

from network.client import Client

# Plafond DUR de joueurs par équipe : la reine ne dépasse jamais ce nombre.
TEAM_CAP = 9

# Seuil de réapprovisionnement (hystérésis). La reine ne reforke QUE lorsque
# l'effectif tombe SOUS ce seuil, puis rebâtit jusqu'au plafond. On évite ainsi
# de forker pour chaque mort isolée. 7 est choisi exprès : 1 reine + 6 ouvriers,
# soit le quorum exact des niveaux 6 et 7 (les plus exigeants).
REFORK_BELOW = 7

# Cooldown court entre deux créations de joueur (le temps que l'œuf forké éclose
# et que l'enfant se connecte, pour ne pas spammer pendant la latence du Fork).
BUILD_FORK_COOLDOWN = 2.0


def _spawn(client: Client) -> subprocess.Popen:
    """Lance un nouveau process zappy_ai et renvoie son handle (pour supervision).
    L'enfant reçoit la variable d'env ZAPPY_CHILD=1 : c'est un simple RÔLE de
    lancement (renforcement de l'équipe), pas une donnée de jeu. Il NE recrute pas (seul le
    recruteur gère l'effectif) ; il joue normalement. Aucune info de jeu (taille
    d'équipe, etc.) n'est transmise → l'interdiction de communiquer hors-jeu est
    respectée. À la connexion, le serveur lui attribue un slot initial libre, sinon
    fait éclore un œuf (Fork) de l'équipe.
    """
    entry = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "zappy_ai")
    env = dict(os.environ, ZAPPY_CHILD="1")
    return subprocess.Popen(
        [
            sys.executable, entry,
            "-p", str(client.port),
            "-n", client.team,
            "-h", client.host,
        ],
        env=env,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )


class TeamManager:
    """La REINE : construit et MAINTIENT l'effectif de l'équipe (par équipe).
    La reine est le premier joueur de l'équipe à se connecter (le recruteur). Elle
    ne joue pas, elle supervise. Elle crée des enfants (ouvriers) pour atteindre le
    plafond d'effectif, puis les supervise et les remplace si nécessaire."""

    def __init__(self, client: Client):
        self.client = client
        # Recruteur = process lancé directement (pas un enfant _spawn).
        self.is_recruiter = os.environ.get("ZAPPY_CHILD") is None
        # Handles des coéquipiers créés par CE recruteur.
        self.children = []
        # Slots initiaux (-c) encore libres au moment de notre connexion : autant
        # de coéquipiers qu'on peut créer par simple connexion (sans Fork).
        self.free_slots = client.free_slots if self.is_recruiter else 0
        self.slots_used = 0
        self.next_build = 0.0
        # Hystérésis : True = on bâtit/rebâtit jusqu'au plafond. Démarre à True
        # (équipe à construire) ; repasse à True dès qu'on retombe sous le seuil.
        self.rebuilding = True

    def tick(self):
        if not self.is_recruiter:
            return
        # On retire les enfants morts (process terminé = joueur mort).
        self.children = [c for c in self.children if c.poll() is None]
        # Effectif courant = la reine (nous) + les ouvriers vivants qu'on a créés.
        members = 1 + len(self.children)

        # Hystérésis de réapprovisionnement : on (re)bâtit dès que l'effectif passe
        # sous REFORK_BELOW, et on s'arrête une fois le plafond TEAM_CAP atteint.
        # Entre les deux on conserve l'état → pas de fork pour chaque mort isolée,
        # mais un vrai réapprovisionnement quand l'équipe est trop entamée.
        if members < REFORK_BELOW:
            self.rebuilding = True
        elif members >= TEAM_CAP:
            self.rebuilding = False

        # Rien à faire si on ne rebâtit pas, ou si le plafond est déjà atteint.
        if not self.rebuilding or members >= TEAM_CAP:
            return
        if time.time() < self.next_build:
            return

        if self.slots_used < self.free_slots:
            # Il reste un slot initial : l'enfant peut s'y connecter tout de suite.
            self.slots_used += 1
            self.children.append(_spawn(self.client))
        elif self.client.connect_nbr() > 0:
            # Un œuf (forké au tour précédent) est prêt : on lance l'enfant qui va
            # l'éclore. CRUCIAL : on n'envoie l'enfant QUE quand un slot/œuf existe,
            # sinon le serveur répond « ko » et le process reste bloqué (fuite).
            self.children.append(_spawn(self.client))
        else:
            # Ni slot ni œuf → on crée un œuf maintenant ; l'enfant viendra l'éclore
            # au prochain tick, une fois l'œuf matérialisé (Fork coûte 42 ticks).
            self.client.send("Fork")
        self.next_build = time.time() + BUILD_FORK_COOLDOWN
