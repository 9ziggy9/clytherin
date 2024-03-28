CC=gcc
CFLAGS=-Wall -Wextra -pedantic -Wconversion -Wunreachable-code -Wswitch-enum
BIN_DIR=./bin
TEST_TXT="hello world" "goodbye moon" "milksteak" "little green ghouls buddy"

# COLOR ALIASES
RED=\033[31m
GREEN=\033[32m
YELLOW=\033[33m
BLUE=\033[34m
MAGENTA=\033[35m
CYAN=\033[36m
RESET=\033[0m

# Colored output function
define print_in_color
	@printf "$1"
	@printf "$2"
	@printf "\033[0m"
endef

all: bin_dir interface test

test:
	$(BIN_DIR)/interface $(TEST_TXT)

bin_dir:
	$(call print_in_color, $(GREEN), \nCreating bin dir: $(BIN_DIR)\n)
	mkdir -p $(BIN_DIR)

interface: ui.o main.c
	$(call print_in_color, $(BLUE), \nCOMPILING main.c\n)
	$(CC) $(CFLAGS) main.c -o $(BIN_DIR)/$@ -lncurses

ui.o: ui.c
	$(call print_in_color, $(BLUE), \nGENERATING OBJ $@\n)
	$(CC) $(CFLAGS) -c ui.c -o $(BIN_DIR)/$@ -lncurses

clean:
	$(call print_in_color, $(GREEN), \nCleaning...\n)
	rm -rf $(BIN_DIR)
