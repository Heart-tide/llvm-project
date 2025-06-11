add_llvm_library(LLVMBATReader SHARED
        source/bat-reader.cpp
        DISABLE_LLVM_LINK_LLVM_DYLIB
)

target_include_directories(LLVMBATReader
        PUBLIC header
)

# 使得动态库包含LLVM项目内的所有依赖
# 效果如：ldd libLLVMBATReader.so.19.1
#        linux-vdso.so.1 (0x00007e738fe74000)
#        libstdc++.so.6 => /usr/lib/libstdc++.so.6 (0x00007e738b800000)
#        libm.so.6 => /usr/lib/libm.so.6 (0x00007e738bb08000)
#        libgcc_s.so.1 => /usr/lib/libgcc_s.so.1 (0x00007e738fe0d000)
#        libc.so.6 => /usr/lib/libc.so.6 (0x00007e738b610000)
#        /usr/lib64/ld-linux-x86-64.so.2 (0x00007e738fe76000)
target_link_libraries(LLVMBATReader
        PRIVATE -Wl,--whole-archive
        PRIVATE LLVMBOLTProfile
        PRIVATE -Wl,--no-whole-archive
)

add_subdirectory(testcase)
