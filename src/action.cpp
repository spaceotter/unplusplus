/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#include <clang/Index/IndexingOptions.h>
#include <clang/Index/IndexingAction.h>

#include "action.hpp"

using namespace unplusplus;
using namespace clang;

bool IndexDataConsumer::handleDeclOccurrence(const Decl *d, clang::index::SymbolRoleSet,
                                             llvm::ArrayRef<clang::index::SymbolRelation>,
                                             clang::SourceLocation sl,
                                             clang::index::IndexDataConsumer::ASTNodeInfo ani) {
  // send output to a temporary buffer in case we have to recurse to write a forward declaration.
  SubOutputs temp(_outs);
  if (const NamedDecl *nd = dynamic_cast<const NamedDecl *>(d)) {
    temp.hf() << "// Decl name:" << nd->getDeclName().getAsString() << "\n";
  } else {
    temp.hf() << "// Unnamed Decl\n";
  }
  SourceManager &SM = d->getASTContext().getSourceManager();
  temp.hf() << "// Source: " << sl.printToString(SM) << " which is: ";
  FileID FID = SM.getFileID(SM.getFileLoc(sl));
  bool Invalid = false;
  const SrcMgr::SLocEntry &SEntry = SM.getSLocEntry(FID, &Invalid);
  switch (SEntry.getFile().getFileCharacteristic()) {
    case SrcMgr::CharacteristicKind::C_ExternCSystem:
      temp.hf() << "ExternCSystem";
      break;
    case SrcMgr::CharacteristicKind::C_User:
      temp.hf() << "User";
      break;
    case SrcMgr::CharacteristicKind::C_System:
      temp.hf() << "System";
      break;
    case SrcMgr::CharacteristicKind::C_User_ModuleMap:
      temp.hf() << "User ModuleMap";
      break;
    case SrcMgr::CharacteristicKind::C_System_ModuleMap:
      temp.hf() << "System ModuleMap";
      break;
  }
  temp.hf() << "\n";
  if (ani.Parent != nullptr) {
    if (const NamedDecl *nd = dynamic_cast<const NamedDecl *>(ani.Parent)) {
      temp.hf() << "// Parent Decl name:" << nd->getDeclName().getAsString() << "\n";
    } else {
      temp.hf() << "// Unnamed Decl\n";
    }
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
