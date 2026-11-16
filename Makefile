# Makefile racine — délègue à chaque composant.
#
# Règles disponibles :
#   all         : compile server + gui
#   server      : compile uniquement le serveur
#   gui         : compile uniquement la GUI
#   ai          : vérifie que zappy_ai est présent (script Python, rien à compiler)
#   clean       : nettoie les objets de tous les composants
#   fclean      : supprime les binaires de tous les composants
#   re          : fclean + all

NAME_SERVER = zappy_server
NAME_GUI    = zappy_gui
NAME_AI     = zappy_ai

all: server gui ai

server:
	$(MAKE) -C server

gui:
	$(MAKE) -C gui

ai:
	@test -f ai/$(NAME_AI) || (echo "ERROR: ai/$(NAME_AI) not found" && exit 1)
	@echo "ai/$(NAME_AI) OK"

clean:
	$(MAKE) -C server clean
	$(MAKE) -C gui clean

fclean: clean
	$(MAKE) -C server fclean
	$(MAKE) -C gui fclean

re: fclean all

.PHONY: all server gui ai clean fclean re
