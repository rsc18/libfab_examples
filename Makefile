# Compiler and Flags
CC = icx
CFLAGS = -O3 -g -mcmodel=large -funroll-loops -mavx512f -mprefer-vector-width=512 \
         -qopt-streaming-stores=always -qopt-report
INCLUDES = -I/opt/cray/libfabric/1.22.0/include
LDFLAGS = -L/opt/cray/libfabric/1.22.0/lib64 -lfabric

# Directories
SRC_DIR = src
BIN_DIR = bin

# Find all .c and .cpp files in src/
SRC = $(wildcard $(SRC_DIR)/*.[cC] $(SRC_DIR)/*.[cC][pP][pP])
# Replace src/filename.c → bin/filename
TARGETS = $(patsubst $(SRC_DIR)/%, $(BIN_DIR)/%, $(basename $(SRC)))

# Default rule
all: $(TARGETS)

# Build rule for both .c and .cpp files
$(BIN_DIR)/%: $(SRC_DIR)/%.c | $(BIN_DIR)
	@echo "Compiling $< → $@"
	$(CC) $(CFLAGS) $(INCLUDES) $< -o $@ $(LDFLAGS)

$(BIN_DIR)/%: $(SRC_DIR)/%.cpp | $(BIN_DIR)
	@echo "Compiling $< → $@"
	$(CC) $(CFLAGS) $(INCLUDES) $< -o $@ $(LDFLAGS)

# Ensure bin directory exists
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Clean rule
clean:
	rm -rf $(BIN_DIR) *.optrpt

# Show optimization reports
report:
	@echo "Optimization reports:"
	@ls -1 *.optrpt 2>/dev/null || echo "No .optrpt files found."

.PHONY: all clean report
