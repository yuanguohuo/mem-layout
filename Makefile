.PHONY: all build test clean

all: clean build test

build:
	cmake -S . -B ./Debug -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=YES -G "Unix Makefiles"
	cmake --build ./Debug -j8

clean:
	find ./Debug -mindepth 1 -not -name 'compile_commands.json' | xargs rm -fr

test:
	./Debug/basic
	./Debug/serialize
	./Debug/alignment
