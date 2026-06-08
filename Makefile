NAME        = webserv

CXX         = clang++
CXXFLAGS    = -Wall -Wextra -Werror -std=c++98 -Iinclude -fsanitize=address -g -O1

SRC_DIR     = src
OBJ_DIR     = obj

SRC         = $(wildcard $(SRC_DIR)/*.cpp)
OBJ         = $(SRC:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

all: $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) -o $(NAME)
	@echo "Build complete."

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR)
	@echo "Object files removed."

fclean: clean
	rm -f $(NAME)
	@echo "Executable removed."

re: fclean all

.PHONY: all clean fclean re