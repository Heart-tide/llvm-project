// bat-reader.cpp: reader mapping from bat section

#include "bolt/Profile/BoltAddressTranslation.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Object/ELFObjectFile.h"
#include "llvm/Object/Error.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Object/SymbolicFile.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Errc.h"
#include "llvm/Support/FileSystem.h"

using namespace llvm;
using namespace bolt;

static BoltAddressTranslation BAT;
static std::map<uint64_t, std::string> FunctionsMap;

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

uint64_t dumpBATFor(uint64_t Address) {
  // dbgs() << "Translating addresses according to parsed BAT tables:\n";
  auto FI = FunctionsMap.upper_bound(Address);
  if (FI == FunctionsMap.begin()) {
    errs() << "No function symbol found for 0x" << Twine::utohexstr(Address)
           << "\n";
  }
  --FI;

  uint64_t prevOffset = BAT.reverseBranchTranslate(FI->first, Address - FI->first);
  // dbgs() << "0x" << Twine::utohexstr(Address) << " -> "
  //        << "0x" << Twine::utohexstr(FI->first + prevOffset)
  //        << " (aka. " << FI->second << " + 0x" << Twine::utohexstr(prevOffset) << ")"
  //        << "\n";
  return FI->first + prevOffset;
}

static void initBAT(llvm::object::ELFObjectFileBase *InputFile) {
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
  for (const llvm::object::ELFSymbolRef &Symbol : InputFile->symbols()) {
    Expected<StringRef> NameOrError = Symbol.getName();
    if (NameOrError.takeError())
      continue;
    if (cantFail(Symbol.getType()) != llvm::object::SymbolRef::ST_Function)
      continue;
    const StringRef Name = *NameOrError;
    const uint64_t FuncAddress = cantFail(Symbol.getAddress());
    FunctionsMap[FuncAddress] = Name.str();
  }
}

int initBATReader(std::string InputFilename) {
  if (!sys::fs::exists(InputFilename))
    report_error(InputFilename, errc::no_such_file_or_directory);

  Expected<llvm::object::OwningBinary<llvm::object::Binary>> BinaryOrErr =
      llvm::object::createBinary(InputFilename);
  if (Error E = BinaryOrErr.takeError())
    report_error(InputFilename, std::move(E));
  llvm::object::Binary &Binary = *BinaryOrErr.get().getBinary();

  if (auto *InputFile = dyn_cast<llvm::object::ELFObjectFileBase>(&Binary))
    initBAT(InputFile);
  else
    report_error(InputFilename,
                 llvm::object::object_error::invalid_file_type);

  return EXIT_SUCCESS;
}
