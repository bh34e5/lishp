SRC=src
TARGET=lishp
BUILD=build

CC=clang++
DEPFLAGS=-MD -MP
CFLAGS=-Wall -Wextra -g $(DEPFLAGS)

FILES=$(wildcard $(SRC)/*.cpp)
INCLUDES=$(wildcard $(SRC)/*.hpp)
OBJECTS=$(patsubst $(SRC)/%.cpp,$(BUILD)/%.o,$(FILES))
DEPFILES=$(patsubst $(SRC)/%.cpp,$(BUILD)/%.d,$(FILES))

.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^

$(BUILD)/%.o: $(SRC)/%.cpp | $(BUILD)
	$(CC) $(CFLAGS) $(foreach d,$(INCLUDES),-I$(d)) -o $@ -c $<

$(BUILD):
	mkdir -p $(BUILD)

.PHONY: clean
clean:
	rm -r $(TARGET) $(BUILD)

-include $(DEPFILES)
