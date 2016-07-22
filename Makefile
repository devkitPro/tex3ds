TARGET := $(notdir $(CURDIR))

CFILES   := $(wildcard source/*.c)
CXXFILES := $(wildcard source/*.cpp)
OFILES   := $(subst source/,build/,$(CFILES:.c=.c.o)) \
            $(subst source/,build/,$(CXXFILES:.cpp=.cpp.o))

override COMMON_FLAGS := -g -Wall -Iinclude -pthread -D_GNU_SOURCE $(COMMON_FLAGS)
override CFLAGS       := $(COMMON_FLAGS) -std=c99 $(CFLAGS)
override CXXFLAGS     := $(COMMON_FLAGS) -std=c++11 \
                         `Magick++-config --cxxflags --cppflags` \
                         $(CXXFLAGS)
override LDFLAGS      := -pthread \
                          `Magick++-config --ldflags --libs` \
                         $(LDFLAGS)

.PHONY: all debug release clean

all: release

debug: $(TARGET)

release:
	@$(MAKE) COMMON_FLAGS="-O3 -flto -DNDEBUG" LDFLAGS="-flto" $(TARGET)

$(TARGET): $(OFILES)
	@echo "Linking $@"
	@$(CXX) -o $@ $^ $(LDFLAGS)

$(OFILES): | build

build:
	@echo "Creating $@ directory"
	@mkdir -p $@

build/%.c.o : source/%.c
	@echo "Compiling $@"
	@$(CC) -MMD -MP -MF build/$*.c.d -o $@ -c $< $(CFLAGS)

build/%.cpp.o : source/%.cpp
	@echo "Compiling $@"
	@$(CXX) -MMD -MP -MF build/$*.cpp.d -o $@ -c $< $(CXXFLAGS)

clean:
	@$(RM) -r build $(TARGET)

-include build/*.d
