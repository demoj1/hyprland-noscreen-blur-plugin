PLUGIN  = blurcap
SRC     = src/main.cpp
OUT     = $(PLUGIN).so

PKGS    = hyprland pixman-1 libdrm
CXX    ?= g++
CXXFLAGS += -shared -fPIC --no-gnu-unique -g -std=c++26 -Wall \
            $(shell pkg-config --cflags $(PKGS))

all: $(OUT)

$(OUT): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(OUT)

clean:
	rm -f $(OUT)

.PHONY: all clean
