NAME = gzip

CC = c++
CFLAGS = -Wall -Wextra -Werror -Iincludes

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
SDKROOT := $(shell xcrun --show-sdk-path 2>/dev/null)
LIBCXX_INCLUDE := $(SDKROOT)/usr/include/c++/v1
ifneq ($(wildcard $(LIBCXX_INCLUDE)/iostream),)
CFLAGS += -isystem $(LIBCXX_INCLUDE)
endif
endif

SRCS = src/main.cpp src/DeflateCompress.cpp src/DeflateDecompress.cpp src/Gzip.cpp src/Huffman.cpp src/LZ77.cpp

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
