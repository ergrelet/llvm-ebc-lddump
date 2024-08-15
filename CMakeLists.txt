cmake_minimum_required(VERSION 3.25)

project(llvm-ebc-lddump
	VERSION 0.1.0
	LANGUAGES C
)

add_library(plugin SHARED "src/plugin.c")

