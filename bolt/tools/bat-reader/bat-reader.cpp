// bat-reader.cpp: reader mapping from bat section

#include "bolt/Profile/BoltAddressTranslation.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Object/Binary.h"
#include "llvm/Object/ELFObjectFile.h"
#include "llvm/Object/Error.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Object/SymbolicFile.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Errc.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/ErrorOr.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Program.h"
#include "llvm/Support/raw_ostream.h"
#include <assert.h>
#include <cstdint>
#include <map>
#include <stdlib.h>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>

using namespace llvm;
using namespace bolt;

namespace opts {

static cl::OptionCategory BatReaderCategory("BAT reader options");

static cl::OptionCategory *BatReaderCategories[] = {&BatReaderCategory};

static cl::opt<std::string> InputFilename(cl::Positional,
                                          cl::desc("<executable>"),
                                          cl::Required,
                                          cl::cat(BatReaderCategory));

static cl::opt<std::string> TargetAddress("addr", cl::desc("address to be translated"),
                             cl::cat(BatReaderCategory));

} // namespace opts

static void report_error(StringRef Message, std::error_code EC) {
  assert(EC);
  errs() << "llvm-bat-reader: '" << Message << "': " << EC.message() << ".\n";
  exit(1);
}

static void report_error(StringRef Message, Error E) {
  assert(E);
  errs() << "llvm-bat-reader: '" << Message << "': " << toString(std::move(E))
         << ".\n";
  exit(1);
}

void dumpBATFor(llvm::object::ELFObjectFileBase *InputFile, uint64_t Address) {
  BoltAddressTranslation BAT;
  if (!BAT.enabledFor(InputFile)) {
    errs() << "error: no BAT table found.\n";
    exit(1);
  }

  // Look for BAT section
  bool Found = false;
  StringRef SectionContents;
  for (const llvm::object::SectionRef &Section : InputFile->sections()) {
    Expected<StringRef> SectionNameOrErr = Section.getName();
    if (Error E = SectionNameOrErr.takeError())
      continue;

    if (SectionNameOrErr.get() != BoltAddressTranslation::SECTION_NAME)
      continue;

    Found = true;
    Expected<StringRef> ContentsOrErr = Section.getContents();
    if (Error E = ContentsOrErr.takeError())
      continue;
    SectionContents = ContentsOrErr.get();
  }

  if (!Found) {
    errs() << "BOLT-ERROR: failed to parse BOLT address translation "
              "table. No BAT section found\n";
    exit(1);
  }

  if (std::error_code EC = BAT.parse(outs(), SectionContents)) {
    errs() << "BOLT-ERROR: failed to parse BOLT address translation "
              "table. Malformed BAT section\n";
    exit(1);
  }

  // Build map of <Address, SymbolName> for InputFile
  // 在不开启函数重排的情况下，可以用优化后的函数入口表来代替优化前的表
  std::map<uint64_t, StringRef> FunctionsMap;
  for (const llvm::object::ELFSymbolRef &Symbol : InputFile->symbols()) {
    Expected<StringRef> NameOrError = Symbol.getName();
    if (NameOrError.takeError())
      continue;
    if (cantFail(Symbol.getType()) != llvm::object::SymbolRef::ST_Function)
      continue;
    const StringRef Name = *NameOrError;
    const uint64_t FuncAddress = cantFail(Symbol.getAddress());
    FunctionsMap[FuncAddress] = Name;
  }

  outs() << "Translating addresses according to parsed BAT tables:\n";
  auto FI = FunctionsMap.upper_bound(Address);
  if (FI == FunctionsMap.begin()) {
    outs() << "No function symbol found for 0x" << Twine::utohexstr(Address)
           << "\n";
  }
  --FI;

  uint64_t prevOffset = BAT.reverseBranchTranslate(FI->first, Address - FI->first);
  outs() << "0x" << Twine::utohexstr(Address) << " -> "
         << "0x" << Twine::utohexstr(FI->first + prevOffset)
         << " (aka. " << FI->second << " + 0x" << Twine::utohexstr(prevOffset) << ")"
         << "\n";
}

int main(int argc, char **argv) {
  cl::HideUnrelatedOptions(ArrayRef(opts::BatReaderCategories));
  cl::ParseCommandLineOptions(argc, argv, "");

  if (!sys::fs::exists(opts::InputFilename))
    report_error(opts::InputFilename, errc::no_such_file_or_directory);

  Expected<llvm::object::OwningBinary<llvm::object::Binary>> BinaryOrErr =
      llvm::object::createBinary(opts::InputFilename);
  if (Error E = BinaryOrErr.takeError())
    report_error(opts::InputFilename, std::move(E));
  llvm::object::Binary &Binary = *BinaryOrErr.get().getBinary();

  uint64_t TargetAddressInteger = strtoul(opts::TargetAddress.c_str(), NULL, 16);

  if (auto *InputFile = dyn_cast<llvm::object::ELFObjectFileBase>(&Binary))
    dumpBATFor(InputFile, TargetAddressInteger);
  else
    report_error(opts::InputFilename,
                 llvm::object::object_error::invalid_file_type);

  return EXIT_SUCCESS;
}
