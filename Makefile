NAME = gbmu
SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
ifeq ($(OS),Windows_NT)
	NAME := $(NAME).exe
	CFLAGS = -DGBMU_USE_SDL
	LDFLAGS = -lSDL2main -lSDL2
else
	LIBS = gtk+-3.0 libpulse-simple
	CFLAGS = `pkg-config $(LIBS) --cflags` -DGBMU_USE_GTK
	LDFLAGS = `pkg-config $(LIBS) --libs` -lm -pthread
endif
.PHONY: all clean fclean re

all: $(NAME)

$(NAME): $(OBJ)
	cc -o $@ $^ $(LDFLAGS)

%.o: %.c
	cc -c -o $@ $< $(CFLAGS)

clean:
	rm -rf $(OBJ)

fclean:
	rm -rf $(OBJ) $(NAME)

re: fclean all
