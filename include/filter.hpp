/*
 * unplusplus
 * Copyright 2021 Eric Eaton
 */

#pragma once

#include <clang/AST/Decl.h>

#include <functional>

namespace unplusplus {
// Check whether the given declaration should be considered "internal" to the C++ standard library,
// because some declarations in it are not a part of the standard specification and should not be
// used directly.
bool isLibraryInternal(const clang::Decl *D);

// Check whether the access is not publically accessible. Unplusplus can only use the public
// interface of a C++ library.
bool isInaccessible(const clang::Decl *D);

// Recursively check parts of the template arguments against the predicate. The predicate should
// only inspect the immediate declaration.
bool traverse(const llvm::ArrayRef<clang::TemplateArgument> &Args,
              std::function<bool(const clang::Decl *)> Predicate);

// Recursively check parts of the type against the predicate. The predicate should only inspect the
// immediate declaration.
bool traverse(const clang::Type *T, std::function<bool(const clang::Decl *)> Predicate);

// Recursively check parts of the type against the predicate. The predicate should only inspect the
// immediate declaration.
bool traverse(clang::QualType QT, std::function<bool(const clang::Decl *)> Predicate);

// Recursively check parts of the declaration against the predicate. The predicate should only
// inspect the immediate declaration.
bool traverse(const clang::Decl *D, std::function<bool(const clang::Decl *)> Predicate);

// Whether the declaration should be filtered out and never used by unplusplus, unless it is given
// another name by a typedef or similar construct.
bool filterOut(const clang::Decl *D);

// Scrub any filtered-out decls from the type, but leave the size of the resulting type the same
void sanitizeType(clang::QualType &QT, const clang::ASTContext &AC);
}  // namespace unplusplus
