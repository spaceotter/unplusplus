/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#pragma once

#include <clang/AST/Decl.h>
#include <clang/Lex/Preprocessor.h>
#include <clang/Sema/Sema.h>
#include <clang/AST/Mangle.h>

#include <list>
#include <memory>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "filter.hpp"
#include "json.hpp"
#include "outputs.hpp"

namespace unplusplus {
class JobManager;

// The base class for jobs, that depend on other jobs. *All* dependencies should be established in
// the constructor of derived classes. The derived constructor should call checkReady() at the end,
// or the job may not run. Then, the overridden impl() will be called when all dependencies are
// satisfied.
class JobBase {
  JobManager &_manager;
  // these must be *ordered*, or the declarations are not processed in a deterministic order!
  // This is very important because identical symbols are renamed depending on order.
  std::list<JobBase *> _depends;    // Dependencies of this job (minus those that are done)
  std::list<JobBase *> _dependent;  // Jobs that depend on this job
  bool _done = false;

 protected:
  Outputs &_out;
  std::string _name;
  clang::Sema &_s;
  // This function should be overidden as the *only* code that writes to the output(s), and it
  // should *not* set any dependencies.
  virtual void impl() = 0;

 public:
  JobBase(JobManager &manager, clang::Sema &S);
  virtual ~JobBase() = default;
  IdentifierConfig &cfg();
  JsonConfig &jcfg();
  clang::ASTNameGenerator &nameGen();
  JobManager &manager() { return _manager; }
  bool isDone() const { return _done; }
  const std::string &name() const { return _name; }
  const std::list<JobBase *> &dependencies() { return _depends; }

  // Enqueue the job if it has no remaining dependencies.
  void checkReady();
  // Run the job and satisfy its dependencies.
  void run();
  void depends(JobBase *other);
  void depends(clang::Decl *D, bool define);
  void depends(clang::QualType QT, bool define);
  void satisfy(JobBase *dependency);
};

// A Base class for Jobs that process a clang::Decl.
template <class T>
class Job : public JobBase {
 protected:
  T *_d;
  std::string _location;

 public:
  typedef T type;

  Job(T *D, clang::Sema &S, JobManager &manager)
      : JobBase(manager, S),
        _d(D),
        _location(_d->getLocation().printToString(_d->getASTContext().getSourceManager())) {
    _name = cfg().getDebugName(D);
  }
};

extern template class Job<clang::TypedefDecl>;
extern template class Job<clang::FunctionDecl>;
extern template class Job<clang::CXXRecordDecl>;
extern template class Job<clang::VarDecl>;
extern template class Job<clang::EnumDecl>;

class TypedefJob : public Job<clang::TypedefDecl> {
  bool _replacesFiltered = false;
  bool _anonymousStruct = false;
  std::string _keyword;

 public:
  TypedefJob(type *D, clang::Sema &S, JobManager &manager);
  void impl() override;
};

class VarJob : public Job<clang::VarDecl> {
  clang::QualType _ptr;

 public:
  VarJob(type *D, clang::Sema &S, JobManager &jm);
  void impl() override;
};

class JobManager {
  Outputs &_out;
  DeclFilter _filter;
  IdentifierConfig _cfg;
  JsonConfig _jcfg;
  clang::ASTNameGenerator _ng;
  std::unordered_set<clang::Decl *> _decls;
  std::unordered_set<clang::Decl *> _renamed;
  std::vector<std::unique_ptr<JobBase>> _jobs;
  std::unordered_map<clang::Decl *, JobBase *> _declarations;
  std::unordered_map<clang::Decl *, JobBase *> _definitions;
  std::queue<clang::TemplateDecl *> _templates;
  std::queue<JobBase *> _ready;
  std::queue<clang::Decl *> _lazy;

 public:
  JobManager(Outputs &out, clang::ASTContext &ASTC, DeclFilterConfig &FC)
      : _out(out),
        _filter(ASTC.getLangOpts(), FC),
        _cfg(ASTC.getLangOpts(), _filter),
        _jcfg(_cfg, ASTC, out),
        _ng(ASTC) {}
  ~JobManager();

  Outputs &out() { return _out; }
  IdentifierConfig &cfg() { return _cfg; }
  DeclFilter &filter() { return _filter; }
  JsonConfig &jcfg() { return _jcfg; }
  clang::ASTNameGenerator &nameGen() { return _ng; }
  void flush(clang::Sema &S);

  // Apply the operator to the declarations nested in the type
  void traverse(clang::QualType QT, std::function<void(clang::Decl *)> OP);
  // Apply the operator to the declarations nested in the template arguments
  void traverse(const llvm::ArrayRef<clang::TemplateArgument> &Args,
                std::function<void(clang::Decl *)> OP);

  // Create jobs immediately for the declaration, so that a dependency can be created on them.
  void create(clang::Decl *D, clang::Sema &S);
  void create(clang::QualType QT, clang::Sema &S);
  void create(const llvm::ArrayRef<clang::TemplateArgument> &Args, clang::Sema &S);

  // Lets the manager know the declaration exists, but doesn't create the jobs immediately for use
  // as a dependency.
  void lazyCreate(clang::Decl *D, clang::Sema &S);
  void lazyCreate(clang::QualType QT, clang::Sema &S);
  void lazyCreate(const llvm::ArrayRef<clang::TemplateArgument> &Args, clang::Sema &S);

  void declare(clang::Decl *D, JobBase *J) { _declarations[D] = J; }
  void define(clang::Decl *D, JobBase *J) { _definitions[D] = J; }
  bool isDefined(clang::Decl *D);
  bool prevDeclared(clang::Decl *D);

  // Rename the filtered-out declaration using the new declaration that isn't filtered out.
  bool renameFiltered(clang::NamedDecl *D, clang::NamedDecl *New);

  bool isRenamed(clang::NamedDecl *D) { return _renamed.count(D); }

  void visitMacros(const clang::Preprocessor &PP);

  // Emit fully instantiated template specializations, and any additional specializations that were
  // discovered while emitting the known ones.
  void finishTemplates(clang::Sema &S);

  friend class JobBase;
};
}  // namespace unplusplus
