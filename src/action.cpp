/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#include "action.hpp"

#include <clang/Index/IndexingAction.h>
#include <clang/Index/IndexingOptions.h>

using namespace unplusplus;
using namespace clang;

// clang-format off
static std::unordered_map<SrcMgr::CharacteristicKind, const std::string> ckStrs {
  { SrcMgr::CharacteristicKind::C_ExternCSystem, "ExternCSystem" },
  { SrcMgr::CharacteristicKind::C_User, "User" },
  { SrcMgr::CharacteristicKind::C_System, "System" },
  { SrcMgr::CharacteristicKind::C_User_ModuleMap, "User ModuleMap" },
  { SrcMgr::CharacteristicKind::C_System_ModuleMap, "System ModuleMap" }
};
// clang-format on

bool IndexDataConsumer::handleDeclOccurrence(const Decl *d, clang::index::SymbolRoleSet,
                                             llvm::ArrayRef<clang::index::SymbolRelation>,
                                             clang::SourceLocation sl,
                                             clang::index::IndexDataConsumer::ASTNodeInfo ani) {
  // send output to a temporary buffer in case we have to recurse to write a forward declaration.
  SubOutputs temp(_outs);
  SourceManager &SM = d->getASTContext().getSourceManager();
  std::string location = sl.printToString(SM);
  if (const NamedDecl *nd = dynamic_cast<const NamedDecl *>(d)) {
    try {
      Identifier i(nd, temp.cfg());
      FileID FID = SM.getFileID(SM.getFileLoc(sl));
      bool Invalid = false;
      const SrcMgr::SLocEntry &SEntry = SM.getSLocEntry(FID, &Invalid);
      SrcMgr::CharacteristicKind ck = SEntry.getFile().getFileCharacteristic();
      // The declaration is from a C header that can just be included by the library
      if (ck == SrcMgr::CharacteristicKind::C_ExternCSystem) {
        _outs.addCHeader(SEntry.getFile().getName().str());
        return true;
      }
      temp.hf() << "// Decl name: " << i.c << " aka " << i.cpp << "\n";
      temp.hf() << "// Source: " << location << " which is: ";
      temp.hf() << ckStrs[SEntry.getFile().getFileCharacteristic()] << "\n";
    } catch (const mangling_error err) {
      std::cerr << "Warning: Ignoring " << err.what() << " " << nd->getQualifiedNameAsString()
                << " at " << location << "\n";
    }
  } else {
    std::cerr << "Warning: Ignoring unnamed Decl of kind " << d->getDeclKindName() << "\n";
  }
  return true;
}

std::unique_ptr<clang::FrontendAction> IndexActionFactory::create() {
  clang::index::IndexingOptions opts;
  opts.IndexFunctionLocals = false;
  opts.IndexImplicitInstantiation = false;
  opts.IndexMacrosInPreprocessor = true;
  opts.IndexParametersInDeclarations = true;
  opts.IndexTemplateParameters = false;
  opts.SystemSymbolFilter = clang::index::IndexingOptions::SystemSymbolFilterKind::All;
  IndexDataConsumer idx(_outs);
  return createIndexingAction(std::make_shared<IndexDataConsumer>(idx), opts);
}
