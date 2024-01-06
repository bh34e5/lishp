SRC=src
SRC_DIRS=$(shell find $(SRC) -type d)
TARGET=lishp
BUILD=build
BUILD_DIRS=$(BUILD) $(patsubst $(SRC)/%,$(BUILD)/%,$(SRC_DIRS))

CC=clang++
DEPFLAGS=-MD -MP
CFLAGS=-Wall -Wextra -g $(DEPFLAGS) -std=c++17

FILES=$(foreach D,$(SRC_DIRS),$(wildcard $(D)/*.cpp))
OBJECTS=$(patsubst $(SRC)/%.cpp,$(BUILD)/%.o,$(FILES))
DEPFILES=$(patsubst $(SRC)/%.cpp,$(BUILD)/%.d,$(FILES))

.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^

$(BUILD)/%.o: $(SRC)/%.cpp | $(BUILD_DIRS)
	$(CC) $(CFLAGS) $(foreach D,$(SRC_DIRS),-I$(D)) -o $@ -c $<

$(BUILD_DIRS):
	mkdir -p $@

.PHONY: clean
clean:
	if [ -f $(TARGET) ] ; then rm $(TARGET) ; fi
	if [ -d $(BUILD) ] ; then rm -r $(BUILD) ; fi

-include $(DEPFILES)
