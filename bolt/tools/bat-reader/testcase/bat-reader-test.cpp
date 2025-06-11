// bat-reader-test: LLVMBATReader的示例调用文件

#include <string>
#include <cstdint>
#include <iostream>
#include "../header/bat-reader.h"

int main() {
    bat_reader::initBATReader("oat.bolt");
    constexpr uint AddrSize = 3;
    uint64_t PrevAddr[AddrSize] = { 0x134482cUL, 0x1344830UL, 0x13447f8UL };
    for (uint64_t i = 0; i < AddrSize; i++) {
        printf("Address translation: 0x%lx -> 0x%lx\n", PrevAddr[i], bat_reader::dumpBATFor(PrevAddr[i]));
    }
    return 0;
}
