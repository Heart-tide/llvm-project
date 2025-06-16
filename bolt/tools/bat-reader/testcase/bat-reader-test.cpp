// bat-reader-test: LLVMBATReader的示例调用文件

#include <string>
#include <cstdint>
#include <iostream>
#include "../header/bat-reader.h"

int main() {
    bat_reader::initBATReader("/home/zcc/Desktop/odex/base.odex");
    constexpr uint AddrSize = 1;
    uint64_t PrevAddr[AddrSize] = { 0x1c038UL };
    for (uint64_t i = 0; i < AddrSize; i++) {
        printf("Address translation: 0x%lx -> 0x%lx\n", PrevAddr[i], bat_reader::dumpBATFor(PrevAddr[i]));
    }
    return 0;
}
