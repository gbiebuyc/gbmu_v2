NAME = gbmu
# SRC = $(wildcard src_emu/*.c) src_frontend/frontend_sdl.c
SRC = $(wildcard src_emu/*.c) src_frontend/frontend_gtk.c
OBJ = $(SRC:.c=.o)
# LIB = sdl2
LIB = gtk+-3.0
CFLAGS = `pkg-config $(LIB) --cflags`
LDFLAGS = `pkg-config $(LIB) --libs`
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
