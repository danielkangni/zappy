CXX      = g++

CXXFLAGS = -std=c++17 -O2 -Wall -Iinclude -I$(HOME)/.local/include -pthread -DGLM_ENABLE_EXPERIMENTAL
LDFLAGS  = -lGL -lGLEW -lsfml-graphics -lsfml-window -lsfml-system -lsfml-audio -lX11 -pthread

NAME = zappy_gui

SRCS = src/render/Shader.cpp \
       src/render/Mesh.cpp \
       src/render/ObjLoader.cpp \
       src/render/SkinnedMesh.cpp \
       src/render/Camera.cpp \
       src/render/TexturedMesh.cpp \
       src/net/GuiClient.cpp \
       src/menu/MenuScene.cpp \
       src/audio/AudioManager.cpp \
       src/hud/HudOverlay.cpp \
       src/world/World.cpp \
       src/app/SkyDayNight.cpp \
       src/app/Constellation.cpp \
       src/app/TreeMotes.cpp \
       src/app/IncantationFx.cpp \
       src/app/WorldRender.cpp \
       src/app/TileDebug.cpp \
       src/app/Islands.cpp \
       src/app/CharacterRender.cpp \
       src/app/Wildlife.cpp \
       src/app/PlayerRender.cpp \
       src/app/SettingsOverlay.cpp \
       src/app/OutroScene.cpp \
       src/app/Fullscreen.cpp \
       src/app/GuiApp.cpp \
       src/main.cpp

OBJS = $(SRCS:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@


DEPS_STAMP := $(HOME)/.local/include/tiny_gltf.h

deps:
	@./scripts/setup_libs.sh

$(DEPS_STAMP):
	@./scripts/setup_libs.sh


$(OBJS): | $(DEPS_STAMP)

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all deps clean fclean re
