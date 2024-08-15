cmake_minimum_required(VERSION 3.18 FATAL_ERROR)

project(HelloWorld)

include(../../cmake/LLVMEmbedBitcode.cmake)

add_executable(hello_world "main.cpp")
llvm_embed_bitcode_configure(hello_world)
