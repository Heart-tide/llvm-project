set(LLVM_LINK_COMPONENTS
        Object
        Support
)

add_llvm_library(LLVMBATReader STATIC
        source/bat-reader.cpp
        DISABLE_LLVM_LINK_LLVM_DYLIB
  )

target_include_directories(LLVMBATReader
        PUBLIC header
)

target_link_libraries(LLVMBATReader
        PRIVATE LLVMBOLTProfile
        --static
)

add_subdirectory(testcase)
