cmake_minimum_required(VERSION 3.18 FATAL_ERROR)

function(llvm_embed_bitcode_configure target)
    include(FetchContent)    
    include(CheckCompilerFlag)    
    include(CheckLinkerFlag)

    # Fetch plugin
    FetchContent_Declare(llvm-ebc-lddump
        GIT_REPOSITORY https://github.com/ergrelet/llvm-ebc-lddump
        GIT_TAG a79b35d9a745484292ab50736a1f761af9fec63f
    )    
    FetchContent_MakeAvailable(llvm-ebc-lddump)

    # Build plugin
    message(STATUS "[LLVMEmbedBitcode] Building plugin...")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -B "${llvm-ebc-lddump_SOURCE_DIR}/_build" -DCMAKE_BUILD_TYPE=Release
        WORKING_DIRECTORY "${llvm-ebc-lddump_SOURCE_DIR}"
    )
    execute_process(
        COMMAND ${CMAKE_COMMAND} --build "${llvm-ebc-lddump_SOURCE_DIR}/_build"
        WORKING_DIRECTORY "${llvm-ebc-lddump_SOURCE_DIR}"
    )

    get_filename_component(PLUGIN_DIR_PATH "${llvm-ebc-lddump_SOURCE_DIR}/_build/" REALPATH)
    message(STATUS "[LLVMEmbedBitcode] Plugin has been built in: ${PLUGIN_DIR_PATH}")

    check_compiler_flag(CXX "-fembed-bitcode" COMPILER_SUPPORTS_EMBED_BITCODE)
    if (COMPILER_SUPPORTS_EMBED_BITCODE)
        if (WIN32)
            # Note: we only support lld
            check_linker_flag(CXX "LINKER:/mllvm:-load=${PLUGIN_DIR_PATH}/plugin.dll" LINKER_SUPPORTS_MLLVM)
            if (LINKER_SUPPORTS_MLLVM)
                # Looks like lld
                target_compile_options(${target} PRIVATE "-fembed-bitcode")
                target_link_options(${target} PRIVATE "LINKER:/mllvm:-load=${PLUGIN_DIR_PATH}/plugin.dll")
            else()
                message(WARNING "[LLVMEmbedBitcode] Linker not supported!")
            endif()
        else()
            check_linker_flag(CXX "LINKER:-mllvm=-load=${PLUGIN_DIR_PATH}/libplugin.so" LINKER_SUPPORTS_MLLVM)
            check_linker_flag(CXX "LINKER:-plugin=${PLUGIN_DIR_PATH}/libplugin.so" LINKER_SUPPORTS_PLUGIN)
            if (LINKER_SUPPORTS_MLLVM)
                # Looks like lld
                target_compile_options(${target} PRIVATE "-fembed-bitcode")
                target_link_options(${target} PRIVATE "LINKER:-mllvm=-load=${PLUGIN_DIR_PATH}/libplugin.so")
            elseif (LINKER_SUPPORTS_PLUGIN)
                # Looks like GNU ld
                target_compile_options(${target} PRIVATE "-fembed-bitcode")
                target_link_options(${target} PRIVATE "LINKER:-plugin=${PLUGIN_DIR_PATH}/libplugin.so")
            else()
                message(WARNING "[LLVMEmbedBitcode] Linker not supported!")
            endif()
        endif()
    else()
        message(WARNING "[LLVMEmbedBitcode] Compiler doesn't support bitcode embedding!")
    endif()
endfunction()
