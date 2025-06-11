#ifndef BAT_READER_H

#include <string>
#include <cstdint>

namespace bat_reader {

int initBATReader(std::string InputFilename);
uint64_t dumpBATFor(uint64_t Address);

}

#define BAT_READER_H

#endif //BAT_READER_H
