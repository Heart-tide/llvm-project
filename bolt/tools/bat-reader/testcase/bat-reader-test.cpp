// bat-reader-test: LLVMBATReader的示例调用文件

#include <string>
#include <cstdint>
#include <iostream>
#include "../header/bat-reader.h"

int main() {
    bat_reader::initBATReader("/home/zcc/Desktop/odex/base.odex");
    uint64_t PrevAddr[] = { 0x1c038UL, 0x1c044UL, 0x0001c4ac, 0x0001c4e4, 0x0001c50c, 0x0001c540 };
    for (uint64_t i = 0; i < std::size(PrevAddr); i++) {
        printf("Address translation: 0x%lx -> 0x%lx\n", PrevAddr[i], bat_reader::dumpBATFor(PrevAddr[i]));
    }
    return 0;
}
