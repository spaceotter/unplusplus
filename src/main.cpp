#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include "clang/Index/IndexDataConsumer.h"
#include "clang/Index/IndexingAction.h"
#include "clang/Index/IndexingOptions.h"
#include "clang/Tooling/CommonOptionsParser.h"
#include "clang/Tooling/Tooling.h"
#include "llvm/Support/CommandLine.h"

using namespace clang;

class IndexDataConsumer : public index::IndexDataConsumer {
 public:
  // Print a message to stdout if any kind of declaration is found
  bool handleDeclOccurrence(const Decl *d, clang::index::SymbolRoleSet,
                            llvm::ArrayRef<clang::index::SymbolRelation>, clang::SourceLocation sl,
                            clang::index::IndexDataConsumer::ASTNodeInfo ani) override {
    if (const NamedDecl *nd = dynamic_cast<const NamedDecl *>(d)) {
      std::cout << "Decl name:" << nd->getDeclName().getAsString() << std::endl;
    } else {
      std::cout << "Unnamed Decl" << std::endl;
    }
    SourceManager &SM = d->getASTContext().getSourceManager();
    std::cout << "Source: " << sl.printToString(SM) << " which is: ";
    FileID FID = SM.getFileID(SM.getFileLoc(sl));
    bool Invalid = false;
    const SrcMgr::SLocEntry &SEntry = SM.getSLocEntry(FID, &Invalid);
    switch(SEntry.getFile().getFileCharacteristic()) {
      case SrcMgr::CharacteristicKind::C_ExternCSystem:
        std::cout << "ExternCSystem";
        break;
      case SrcMgr::CharacteristicKind::C_User:
        std::cout << "User";
        break;
      case SrcMgr::CharacteristicKind::C_System:
        std::cout << "System";
        break;
      case SrcMgr::CharacteristicKind::C_User_ModuleMap:
        std::cout << "User ModuleMap";
        break;
      case SrcMgr::CharacteristicKind::C_System_ModuleMap:
        std::cout << "System ModuleMap";
        break;
    }
    std::cout << std::endl;
    if (ani.Parent != nullptr) {
      if (const NamedDecl *nd = dynamic_cast<const NamedDecl *>(ani.Parent)) {
        std::cout << "Parent Decl name:" << nd->getDeclName().getAsString() << std::endl;
      } else {
        std::cout << "Unnamed Decl" << std::endl;
      }
    }
    return true;
  }
};

class IndexActionFactory : public clang::tooling::FrontendActionFactory {
 public:
  std::unique_ptr<clang::FrontendAction> create() override {
    clang::index::IndexingOptions opts;
    opts.IndexFunctionLocals = false;
    opts.IndexImplicitInstantiation = false;
    opts.IndexMacrosInPreprocessor = true;
    opts.IndexParametersInDeclarations = true;
    opts.IndexTemplateParameters = false;
    opts.SystemSymbolFilter = clang::index::IndexingOptions::SystemSymbolFilterKind::All;
    IndexDataConsumer idx;
    return createIndexingAction(std::make_shared<IndexDataConsumer>(idx), opts);
  }
};

static llvm::cl::OptionCategory UppCategory("unplusplus options");

int main(int argc, const char **argv) {
  std::vector<const char *> args(argv, argv + argc);
  args.push_back("--");
  args.push_back("clang++");
  args.push_back("-c");
  args.push_back("-resource-dir");
  args.push_back(CLANG_RESOURCE_DIRECTORY);
  int size = args.size();
  tooling::CommonOptionsParser OptionsParser(size, args.data(), UppCategory);
  tooling::ClangTool Tool(OptionsParser.getCompilations(), OptionsParser.getSourcePathList());
  IndexActionFactory Factory;
  return Tool.run(&Factory);
}
