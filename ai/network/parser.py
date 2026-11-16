def parse_look(raw: str) -> list:
    """Transforme la réponse de la commande "Look" en liste de cases.

    Le serveur renvoie le champ de vision sous la forme d'une liste entre
    crochets, les cases séparées par des virgules, et le contenu d'une case par
    des espaces. Exemple :

        [ player, food, , linemate food, deraumere ]
          ^case0  ^case1 ^case2 ^case3        ^case4

    On veut en faire une liste de listes (une sous-liste par case, contenant les
    noms des objets présents) :

        [["player"], ["food"], [], ["linemate", "food"], ["deraumere"]]

    La case d'indice 0 est toujours celle où se trouve le joueur lui-même. Une
    case vide donne une sous-liste vide.
    """
    # On retire les crochets et les espaces/retours autour de la chaîne.
    raw = raw.strip("[] \n")

    tiles = []
    for tile in raw.split(","):
        tile = tile.strip()
        # tile.split() découpe sur les espaces ; si la case est vide on met [].
        items = tile.split() if tile else []
        tiles.append(items)
    return tiles
