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

all: db

test: client
	$(BIN_DIR)/client $(TEST_TXT)

db: bin_dir db.c
	$(call print_in_color, $(BLUE), \nCOMPILING db.c\n)
	$(CC) $(CFLAGS) db.c -o $(BIN_DIR)/$@ -lsqlite3

host: bin_dir host.c
	$(call print_in_color, $(BLUE), \nCOMPILING host.c\n)
	$(CC) $(CFLAGS) host.c -o $(BIN_DIR)/$@

client: ui.o client.c
	$(call print_in_color, $(BLUE), \nCOMPILING client.c\n)
	$(CC) $(CFLAGS) client.c $(BIN_DIR)/ui.o -o $(BIN_DIR)/$@ -lncurses

ui.o: bin_dir ui.c
	$(call print_in_color, $(BLUE), \nGENERATING OBJ $@\n)
	$(CC) $(CFLAGS) -c ui.c -o $(BIN_DIR)/$@ -lncurses

.PHONY: bin_dir
bin_dir:
	$(call print_in_color, $(GREEN), \nCreating bin dir: $(BIN_DIR)\n)
	mkdir -p $(BIN_DIR)

.PHONY: clean
clean:
	$(call print_in_color, $(GREEN), \nCleaning...\n)
	rm -rf $(BIN_DIR)
