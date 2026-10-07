# examples/Makefile - 链接 libncm 并运行 C / C++ 冒烟测试
#
# 前置：已执行 scripts/build.sh 生成 out/libncm.*
#
#   make            构建并运行两个 smoke
#   make asan       带 AddressSanitizer 运行（C 与 C++ 都查泄漏）
#   make clean
#
# -o 指定了目标目录时需同时指定 -f，例如:
#   make -f examples/Makefile OUT_DIR=out/build

OUT_DIR   ?= out
ROOT      := $(abspath $(CURDIR)/..)
LIB_DIR   := $(abspath $(ROOT)/$(OUT_DIR))
INC_DIR   := $(LIB_DIR)/include

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
  LIB_NAME := libncm.dylib
  RPATH    := -Wl,-rpath,$(LIB_DIR)
  # 未安装到系统目录时允许未解析符号
  EXTRA_LD := -Wl,-undefined,dynamic_lookup
else
  LIB_NAME := libncm.so
  RPATH    := -Wl,-rpath,$(LIB_DIR)
  EXTRA_LD :=
endif

CC       ?= cc
CXX      ?= c++
CFLAGS   += -I$(INC_DIR) -Wall -Wextra -Wpedantic
CXXFLAGS += -I$(INC_DIR) -std=c++17 -Wall -Wextra
LDLIBS   += -L$(LIB_DIR) -lncm

BIN_C    := smoke_c
BIN_CXX  := smoke_cxx

.PHONY: all smoke asan clean

all: smoke

smoke: $(BIN_C) $(BIN_CXX)
	@echo "== C smoke =="
	@LD_LIBRARY_PATH=$(LIB_DIR):$$LD_LIBRARY_PATH ./$(BIN_C)
	@echo
	@echo "== C++ smoke =="
	@LD_LIBRARY_PATH=$(LIB_DIR):$$LD_LIBRARY_PATH ./$(BIN_CXX)

$(BIN_C): smoke.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) $(RPATH) $(EXTRA_LD) $(LDLIBS)

$(BIN_CXX): smoke.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS) $(RPATH) $(EXTRA_LD) $(LDLIBS)

asan:
	$(MAKE) clean
	$(MAKE) smoke \
	  CFLAGS="$(CFLAGS) -fsanitize=address -g" \
	  CXXFLAGS="$(CXXFLAGS) -fsanitize=address -g" \
	  LDFLAGS="-fsanitize=address -g"
	@echo "（若在 macOS 上报 LeakSanitizer 不支持，可加 ASAN_OPTIONS=detect_leaks=0）"

clean:
	rm -f $(BIN_C) $(BIN_CXX)