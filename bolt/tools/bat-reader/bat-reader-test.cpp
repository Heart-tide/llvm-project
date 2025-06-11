// 本文件为bat-reader的示例调用文件。
// 使用方法：
// 1. 编译bat-reader后，编译结果位于 <build_dir>/lib/llvm-bat-reader.so
// 2. 编译本文件并链接共享库文件llvm-bat-reader.so

#include <string>
#include <cstdint>
#include <iostream>

int initBATReader(std::string InputFilename);
uint64_t dumpBATFor(uint64_t Address);

int main() {
    initBATReader("oat.bolt");
    constexpr uint AddrSize = 3;
    uint64_t PrevAddr[AddrSize] = { 0x134482cUL, 0x1344830UL, 0x13447f8UL };
    for (uint64_t i = 0; i < AddrSize; i++) {
        printf("Address translation: 0x%lx -> 0x%lx\n", PrevAddr[i], dumpBATFor(PrevAddr[i]));
    }
    return 0;
}
