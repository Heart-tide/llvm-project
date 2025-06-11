add_bolt_executable(bat-reader-test
    bat-reader-test.cpp
    DISABLE_LLVM_LINK_LLVM_DYLIB
)

target_link_libraries(bat-reader-test
    PRIVATE LLVMBATReader
)
