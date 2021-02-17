NAME = gbmu
SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)
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
