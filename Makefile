SRC     = src
INCLUDE = include
TARGET  = lishp
BUILD   = build

SRC_DIRS   = $(shell find $(SRC) -type d)
BUILD_DIRS = $(BUILD) $(patsubst $(SRC)/%,$(BUILD)/%,$(SRC_DIRS))

DEPFLAGS = -MD -MP

CC = clang++
CFLAGS += -Wall
CFLAGS += -Wextra
CFLAGS += -ggdb
CFLAGS += -std=c++17
#CFLAGS += -DDEBUG_MEMORY
CFLAGS += $(DEPFLAGS)

FILES    = $(shell find $(SRC) -type f -name '*.cpp')
OBJECTS  = $(patsubst $(SRC)/%.cpp,$(BUILD)/%.o,$(FILES))
DEPFILES = $(patsubst $(SRC)/%.cpp,$(BUILD)/%.d,$(FILES))

.PHONY: all
all: $(TARGET)

run: all
	@echo
	@./lishp

debug: all
	@echo
	@gdb ./lishp

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^

$(BUILD)/%.o: $(SRC)/%.cpp | $(BUILD_DIRS)
	$(CC) -I$(INCLUDE) $(CFLAGS) -o $@ -c $<

$(BUILD_DIRS):
	mkdir -p $@

.PHONY: clean
clean:
	if [ -f $(TARGET) ] ; then rm $(TARGET) ; fi
	if [ -d $(BUILD) ] ; then rm -r $(BUILD) ; fi

-include $(DEPFILES)
