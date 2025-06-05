set(LLVM_LINK_COMPONENTS
  Object
  Support
  )

add_bolt_executable(llvm-bat-reader
        bat-reader.cpp

  DISABLE_LLVM_LINK_LLVM_DYLIB
  )

target_link_libraries(llvm-bat-reader
  PRIVATE
  LLVMBOLTProfile
  )
