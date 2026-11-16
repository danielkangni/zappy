import os
import socket
import time


class Client:
    # Le serveur applique DÉJÀ un coût en temps par action (Forward/Look=7 ticks,
    # Inventory=1, Fork=42, Incantation=300), cadencé par la fréquence -f : l'IA est
    # donc naturellement rythmée par le serveur (chaque send bloque jusqu'à la
    # réponse, qui n'arrive qu'une fois l'action terminée). ZAPPY_AI_DELAY est un
    # throttle CLIENT optionnel EN PLUS, uniquement pour ralentir la démo visuelle
    # (durée d'un déplacement en s ; 0.5 -> ~2 tuiles/s). 0 (défaut) = pas de
    # throttle client, on suit le rythme du serveur.
    MOVE_INTERVAL = float(os.getenv("ZAPPY_AI_DELAY", "0") or 0)
    # On ne cadence que l'avancée d'une tuile ; les rotations restent rapides.
    _MOVE_CMDS = ("Forward",)

    def __init__(self, host: str, port: int, team: str):
        self.host = host
        self.port = port
        self.team = team
        self.sock = None
        self.map_x = 0
        self.map_y = 0
        self.free_slots = 0
        self._broadcasts = []
        self._pending_level = None   # niveau reçu via "Current level" (incantation)
        self._next_move = 0.0

    def connect(self):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.connect((self.host, self.port))

        welcome = self._read_line()
        if not welcome.startswith("WELCOME"):
            raise ConnectionError(f"Handshake raté : attendu WELCOME, reçu '{welcome}'")

        self._write(self.team)
        # CLIENT-NUM = nombre de slots initiaux encore libres dans la team.
        # 0 → on a pris le dernier slot (cas c=1 : on est seul, il faut forker).
        # >0 → d'autres connexions sont encore possibles (cas c=N, test à N instances).
        try:
            self.free_slots = int(self._read_line())
        except ValueError:
            self.free_slots = 0

        dims = self._read_line().split()
        self.map_x = int(dims[0])
        self.map_y = int(dims[1])

    def send(self, command: str, is_incantation: bool = False) -> str:
        self._write(command)
        return self._read_response(is_incantation)

    def receive(self, is_incantation: bool = False) -> str:
        return self._read_response(is_incantation)

    def send_batch(self, commands: list) -> list:
        # envoie les commandes par chunks de 10 (limite du serveur) sans attendre
        # les réponses entre chaque, puis lit toutes les réponses dans l'ordre.
        # On passe par _write, qui cadence quand même les Forward (ZAPPY_AI_DELAY) :
        # la démo reste regardable, et à plein régime (delay=0) c'est rapide.
        results = []
        for i in range(0, len(commands), 10):
            chunk = commands[i:i + 10]
            for cmd in chunk:
                self._write(cmd)
            for _ in chunk:
                results.append(self._read_response())
        return results

    def get_broadcasts(self) -> list:
        pending = self._broadcasts[:]
        self._broadcasts = []
        return pending

    def get_level_update(self):
        # renvoie le nouveau niveau reçu via "Current level" (ou None), puis l'efface
        lvl = self._pending_level
        self._pending_level = None
        return lvl

    def connect_nbr(self) -> int:
        # Nombre d'œufs (issus de Fork) de l'équipe encore en attente d'éclosion,
        # tel que rapporté PAR LE SERVEUR. C'est notre seule source d'effectif
        # légitime (le sujet interdit toute communication hors-jeu). Sert de
        # sémaphore : si > 0, un œuf est déjà en vol → inutile d'en reforker un.
        raw = self.send("Connect_nbr")
        try:
            return int(raw)
        except (ValueError, TypeError):
            return 0

    _DEBUG = os.getenv("ZAPPY_DEBUG") == "1"

    def _log(self, direction: str, text: str):
        if self._DEBUG:
            print(f"[{direction}] {text}", flush=True)

    def _write(self, message: str):
        # cadençage des déplacements uniquement : un Forward/Left/Right ne part
        # qu'après MOVE_INTERVAL, ce qui borne la vitesse visible (2 tuiles/s à
        # 0.5). Les autres commandes (Look, Inventory, Take...) restent rapides.
        if self.MOVE_INTERVAL > 0 and message.split(" ", 1)[0] in self._MOVE_CMDS:
            wait = self._next_move - time.monotonic()
            if wait > 0:
                time.sleep(wait)
            self._next_move = time.monotonic() + self.MOVE_INTERVAL
        self._log(">>", message)
        self.sock.sendall((message + "\n").encode())

    def _read_line(self) -> str:
        buf = b""
        while not buf.endswith(b"\n"):
            chunk = self.sock.recv(1)
            if not chunk:
                raise ConnectionError("Connexion fermée par le serveur")
            buf += chunk
        line = buf.decode().strip()
        self._log("<<", line)
        return line

    def _read_response(self, is_incantation: bool = False) -> str:
        while True:
            line = self._read_line()
            if line.startswith("message "):
                self._broadcasts.append(line)
            elif line.startswith("Current level"):
                if is_incantation:
                    return line
                # incantation result — peut arriver non sollicité (joueur participant
                # qui n'a pas envoyé Incantation) : on le met de côté pour le brain
                try:
                    self._pending_level = int(line.split(":")[1].strip())
                except (IndexError, ValueError):
                    pass
            elif line.startswith("Elevation underway"):
                if is_incantation:
                    return line
                pass  # Async message from incantation, ignore as command response
            elif line.startswith("eject:"):
                pass  # Async message from being pushed, ignore as command response
            elif line == "dead":
                raise ConnectionError("Mort de faim ou fin de partie")
            else:
                return line