def encode(msg_type: str, data: str) -> str:
    """Construit le texte d'un message de broadcast : "TYPE:donnees".

    On préfixe nos messages par un TYPE (ex. "NEED") suivi de ":" et des données,
    afin de pouvoir trier nos propres messages à la réception et ignorer le reste.
    Exemple : encode("NEED", "team1:4:8123") -> "NEED:team1:4:8123".
    """
    return f"{msg_type}:{data}"


def decode(raw: str):
    """Décode un son reçu du serveur en (direction, type, donnees).

    Le serveur transmet un son sous la forme :
        "message K, TYPE:donnees"
    où K est la direction d'où vient le son (0 = même case, 1..8 = case relative à
    notre orientation, voir DIRECTION_MOVES dans brain.py).

    Renvoie :
        - un triplet (direction:int, type:str, donnees:str) si le message est
          valide ;
        - None si ce n'est pas un message de son ou s'il est mal formé (on
          l'ignore alors sans planter).
    """
    if not raw.startswith("message "):
        return None

    try:
        # On enlève le préfixe "message ", puis on sépare la direction du contenu
        # sur la PREMIÈRE virgule seulement (le contenu peut contenir des ":").
        after = raw[len("message "):]
        k_part, content = after.split(",", 1)
        direction = int(k_part.strip())

        # Le contenu est "TYPE:donnees". S'il n'y a pas de ":", il n'y a pas de
        # données (message sans charge utile).
        content = content.strip()
        if ":" in content:
            msg_type, data = content.split(":", 1)
        else:
            msg_type, data = content, ""

        return (direction, msg_type.strip(), data.strip())

    except (ValueError, IndexError):
        return None
