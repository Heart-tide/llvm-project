llvm_add_library(llvm-bat-reader
        bat-reader.cpp
        MODULE
        DISABLE_LLVM_LINK_LLVM_DYLIB
  )

target_link_libraries(llvm-bat-reader
        PRIVATE
        LLVMBOLTProfile
  )
