# Project settings
LLVM_PROJECT_PATH := $(shell pwd)
LLVM_PATH         := $(LLVM_PROJECT_PATH)/llvm
BUILD_PATH		  := $(LLVM_PROJECT_PATH)/build

# Build directories
CLANG_DEFAULT_PATH    := $(BUILD_PATH)/build_clang_default
CLANG_PROBED_PATH     := $(BUILD_PATH)/build_clang_probed
CLANG_USED_PATH       := $(BUILD_PATH)/build_clang_used
CLANG_OPTIMIZED_PATH  := $(BUILD_PATH)/build_clang_optimized
PROFILE_PATH          := $(BUILD_PATH)/profile
CLANG_EVALUATION1_PATH := $(BUILD_PATH)/build_clang_evaluation1
CLANG_EVALUATION2_PATH := $(BUILD_PATH)/build_clang_evaluation2

BUILD_DIRS := \
    $(CLANG_DEFAULT_PATH) \
    $(CLANG_PROBED_PATH) \
    $(CLANG_USED_PATH) \
    $(CLANG_OPTIMIZED_PATH) \
    $(PROFILE_PATH) \
    $(CLANG_EVALUATION1_PATH) \
    $(CLANG_EVALUATION2_PATH)

# Compiler flags
PROBE_FLAGS    := -fno-omit-frame-pointer -fpseudo-probe-for-profiling -mllvm --use-selective-pseudo-probe=spanning-tree
OPTIMIZE_FLAGS := $(PROBE_FLAGS) -fprofile-sample-use=$(PROFILE_PATH)/clang.spgo.prof

# CMake common configuration
CMAKE_COMMON_FLAGS := \
    -DLLVM_ENABLE_PROJECTS=clang \
    -DCMAKE_BUILD_TYPE=Release \
    -G Ninja \

.PHONY: all default probed profile optimized eval1 eval2 clean

all: default

# Directory creation
$(BUILD_DIRS):
	mkdir -p $@

# Targets
default: $(CLANG_DEFAULT_PATH)
	cd $< && \
        cmake $(CMAKE_COMMON_FLAGS) $(LLVM_PATH) && \
        ninja clang

probed: $(CLANG_PROBED_PATH)
	cd $< && \
        cmake $(CMAKE_COMMON_FLAGS) \
            -DCMAKE_C_COMPILER=$(CLANG_DEFAULT_PATH)/bin/clang \
            -DCMAKE_CXX_COMPILER=$(CLANG_DEFAULT_PATH)/bin/clang++ \
            -DCMAKE_C_FLAGS="$(PROBE_FLAGS)" \
            -DCMAKE_CXX_FLAGS="$(PROBE_FLAGS)" \
            $(LLVM_PATH) && \
        ninja clang

profile: $(CLANG_USED_PATH) $(PROFILE_PATH)
	rm -r $</*
	cd $< && \
        cmake $(CMAKE_COMMON_FLAGS) \
            -DLLVM_BUILD_RUNTIME=No \
            -DCMAKE_C_COMPILER=$(CLANG_PROBED_PATH)/bin/clang \
            -DCMAKE_CXX_COMPILER=$(CLANG_PROBED_PATH)/bin/clang++ \
            $(LLVM_PATH)
	perf record \
        -g \
        --call-graph fp \
        -e br_inst_retired.near_taken:uppp \
        -c 160009 \
        -b \
        -o $(PROFILE_PATH)/clang.perf.data \
        -- ninja -C $(CLANG_USED_PATH) opt
	perf script \
        -F ip,brstack \
        -i $(PROFILE_PATH)/clang.perf.data \
        --show-mmap-event \
        > $(PROFILE_PATH)/clang.perf.script
	llvm-profgen \
        --perfscript=$(PROFILE_PATH)/clang.perf.script \
        --binary=$(CLANG_PROBED_PATH)/bin/clang-21 \
        --output=$(PROFILE_PATH)/clang.spgo.prof \
        --format=text

optimized: $(CLANG_OPTIMIZED_PATH)
	cd $< && \
        cmake $(CMAKE_COMMON_FLAGS) \
            -DCMAKE_C_COMPILER=$(CLANG_DEFAULT_PATH)/bin/clang \
            -DCMAKE_CXX_COMPILER=$(CLANG_DEFAULT_PATH)/bin/clang++ \
            -DCMAKE_C_FLAGS="$(OPTIMIZE_FLAGS)" \
            -DCMAKE_CXX_FLAGS="$(OPTIMIZE_FLAGS)" \
            $(LLVM_PATH) && \
        ninja clang

clean:
	rm -rf $(BUILD_DIRS)