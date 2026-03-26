NAME = gzip

CC = c++
CFLAGS = -Wall -Wextra -Werror -Iincludes

SRCS = src/main.cpp src/Deflate.cpp src/Gzip.cpp src/Huffman.cpp src/LZ77.cpp

OBJ_DIR = obj

OBJS = $(SRCS:%.cpp=$(OBJ_DIR)/%.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(NAME)

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re