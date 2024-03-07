CPPSRC     := cppsrc
CPPINCLUDE := cppinclude
CPPTARGET  := cpplishp
CPPBUILD   := cppbuild

SRC     := src
TEST    := test
INCLUDE := include
TARGET  := lishp
BUILD   := build

CPPSRC_DIRS   := $(shell find $(CPPSRC) -type d)
CPPBUILD_DIRS := $(CPPBUILD) $(patsubst $(CPPSRC)/%,$(CPPBUILD)/%,$(CPPSRC_DIRS))

SRC_DIRS   := $(shell find $(SRC) -type d)
BUILD_DIRS := $(BUILD) $(patsubst $(SRC)/%,$(BUILD)/%,$(SRC_DIRS))

DEPFLAGS := -MD -MP

CC  := clang
CXX := clang++

COMMON_FLAGS += -Wall
COMMON_FLAGS += -Wextra
COMMON_FLAGS += -ggdb
#COMMON_FLAGS += -DDEBUG_MEMORY

CFLAGS   += -std=c2x
CXXFLAGS += -std=c++17

CPPFILES    := $(shell find $(CPPSRC) -type f -name '*.cpp')
CPPOBJECTS  := $(patsubst $(CPPSRC)/%.cpp,$(CPPBUILD)/%.o,$(CPPFILES))
CPPDEPFILES := $(patsubst $(CPPSRC)/%.cpp,$(CPPBUILD)/%.d,$(CPPFILES))

FILES    := $(shell find $(SRC) -type f -name '*.c')
OBJECTS  := $(patsubst $(SRC)/%.c,$(BUILD)/%.o,$(FILES))
DEPFILES := $(patsubst $(SRC)/%.c,$(BUILD)/%.d,$(FILES))

HEADERS  := $(shell find $(INCLUDE) -type f -name '*.h')
CHECKS   := $(foreach H,$(HEADERS),--check_also)

.PHONY: all test cppall clean
all: $(TARGET)

run: all
	@echo
	@./lishp

debug: all
	@echo
	@gdb ./lishp

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^

$(BUILD)/%.o: $(SRC)/%.c | $(BUILD_DIRS)
	$(CC) -I$(INCLUDE) $(CFLAGS) $(COMMON_FLAGS) $(DEPFLAGS) -o $@ -c $<

$(BUILD_DIRS):
	mkdir -p $@

test:
	$(CC) -I$(INCLUDE) $(CFLAGS) $(COMMON_FLAGS) -o test_bin \
	      $(filter-out $(SRC)/main.c,$(FILES)) \
	      $(shell find $(TEST) -type f -name '*.c')
	./test_bin


cppall: $(CPPTARGET)

cpprun: cppall
	@echo
	@./cpplishp

cppdebug: cppall
	@echo
	@gdb ./cpplishp

$(CPPTARGET): $(CPPOBJECTS)
	$(CXX) -o $@ $^

$(CPPBUILD)/%.o: $(CPPSRC)/%.cpp | $(CPPBUILD_DIRS)
	$(CXX) -I$(CPPINCLUDE) $(CXXFLAGS) $(COMMON_FLAGS) $(DEPFLAGS) -o $@ -c $<

$(CPPBUILD_DIRS):
	mkdir -p $@

both: $(TARGET) $(CPPTARGET)

clean:
	if [ -f $(TARGET) ] ; then rm $(TARGET) ; fi
	if [ -d $(BUILD) ] ; then rm -r $(BUILD) ; fi
	@echo
	if [ -f $(CPPTARGET) ] ; then rm $(CPPTARGET) ; fi
	if [ -d $(CPPBUILD) ] ; then rm -r $(CPPBUILD) ; fi
	@echo
	if [ -f test_bin ] ; then rm test_bin ; fi

-include $(DEPFILES)
