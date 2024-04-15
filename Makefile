# gcc is preferred but I do not explicitly set it
CC      := gcc
CFLAGS  := -Wall -Wextra -pedantic -Wconversion -Wunreachable-code -Wswitch-enum

# support from c23 -> c89
STD     := -std=c11
BIN_DIR := ./build
SVE_DIR := ./saves
OBJ     := $(BIN_DIR)/host.o
EXE     := $(BIN_DIR)/run

# COLOR ALIASES
RED     = \033[31m
GREEN   = \033[32m
YELLOW  = \033[33m
BLUE    = \033[34m
MAGENTA = \033[35m
CYAN    = \033[36m
RESET   = \033[0m

# colored output function
define print_in_color
	@printf "$1"
	@printf "$2"
	@printf "\033[0m"
endef

.PHONY: clean all

all: clean $(OBJ) main

main: $(OBJ) main.c
	$(call print_in_color, $(BLUE), \nCOMPILING main.c to EXE $(BIN_DIR)/run\n)
	$(CC) $(CFLAGS) main.c -o $(EXE) $(OBJ)

client: $(BIN_DIR) client.c
	$(call print_in_color, $(BLUE), \nCOMPILING client.c to $(BIN_DIR)/$@\n)
	$(CC) $(CFLAGS) client.c -o $(BIN_DIR)/$@

$(BIN_DIR)/%.o: %.c
	$(call print_in_color, $(BLUE), \nCOMPILING $< to OBJ $@\n)
	$(CC) $(CFLAGS) -c $< -o $@

# create bin directory if doesn't exist
$(EXE): | $(BIN_DIR)
$(OBJ): | $(BIN_DIR)
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	$(call print_in_color, $(GREEN), \nCleaning...\n)
	rm -rf $(BIN_DIR) $(SVE_DIR)
