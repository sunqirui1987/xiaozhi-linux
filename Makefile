PACKAGE_NAME := linx_cc_on_macos
PROJECT_DIR := ${shell pwd}
BUILD_DIR   := ${PROJECT_DIR}/build
INSTALL_DIR := ${BUILD_DIR}/install
NUM_JOB := 4
CMAKE := cmake

BUILD_TYPE := Debug

CMAKE_ARGS := \
        -DBUILD_SHARED_LIBS=OFF \
	    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
        -DCMAKE_BUILD_TYPE=${BUILD_TYPE}

all:
	@echo hello Linx


prepare:
	# For macOS, use Homebrew to install dependencies
	brew install websocketpp portaudio
.PHONY: prepare

build:
	cd ${BUILD_DIR} && \
	${CMAKE} ${CMAKE_ARGS} -DCMAKE_INSTALL_PREFIX=${INSTALL_DIR} .. && \
	make -j ${NUM_JOB} && make install
.PHONY: build

clean:
	rm -rf build/*
.PHONY: clean

run:
	cd ${INSTALL_DIR}/bin && \
	./linx_app
.PHONY: run

