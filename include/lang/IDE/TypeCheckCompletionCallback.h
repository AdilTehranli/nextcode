//===--- TypeCheckCompletionCallback.h  -----------------------------------===//
//
// Copyright (c) 2023 ITGSS Technologies and the Code project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See code/LICENSE.txt for license information
// See code/CONTRIBUTORS.txt for the list of Code project authors
//
//===----------------------------------------------------------------------===//
//
/// \file
/// Provides TypeCheckCompletionCallback implementations for the various kinds
/// of code completion. These extract and persist information needed to compute
/// completion results from the solutions formed during expression typechecking.
//
//===----------------------------------------------------------------------===//

#ifndef CODE_IDE_TYPECHECKCOMPLETIONCALLBACK_H
#define CODE_IDE_TYPECHECKCOMPLETIONCALLBACK_H

#include "code/AST/Expr.h"
#include "code/AST/Type.h"
#include "code/Basic/LLVM.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"

namespace code {
class Decl;
class DeclContext;
class Type;
class ValueDecl;
class CodeCompletionExpr;

namespace constraints {
class ConstraintSystem;
class Solution;
} // namespace constraints

namespace ide {

class TypeCheckCompletionCallback {
  bool GotCallback = false;

protected:
  /// Subclasses of \c TypeCheckCompletionCallback handle solutions discovered
  /// by the constraint system in this function
  virtual void sawSolutionImpl(const constraints::Solution &solution) = 0;

public:
  virtual ~TypeCheckCompletionCallback() {}

  /// Called for each solution produced while type-checking an expression
  /// that the code completion expression participates in.
  void sawSolution(const constraints::Solution &solution) {
    GotCallback = true;
    sawSolutionImpl(solution);
  };

  /// True if at least one solution was passed via the \c sawSolution
  /// callback.
  bool gotCallback() const { return GotCallback; }

  /// Typecheck the code completion expression in its outermost expression
  /// context, calling \c sawSolution for each solution formed.
  virtual void fallbackTypeCheck(DeclContext *DC);
};

// MARK: - Utility functions for subclasses of TypeCheckCompletionCallback

Type getTypeForCompletion(const constraints::Solution &S, ASTNode Node);

/// If \p E occurs in a pattern matching position, returns the type that it is
/// being pattern-matched against.
/// If that type is an enum, it allows us to suggest the enum cases for the code
/// completion expression \p E.
Type getPatternMatchType(const constraints::Solution &S, Expr *E);

/// Populate \p Result with types of variables that were determined in the
/// solution \p S. This in particular includes parameters of closures that
/// were type-checked with the code completion expression.
void getSolutionSpecificVarTypes(
    const constraints::Solution &S,
    llvm::SmallDenseMap<const VarDecl *, Type> &Result);

/// While this RAII is alive the interface types of the variables defined in
/// \c SolutionSpecificVarTypes are temporarily set to the types in the map.
/// Afterwards, their types are restored.
struct WithSolutionSpecificVarTypesRAII {
  llvm::SmallDenseMap<const VarDecl *, Type> RestoreVarTypes;

  WithSolutionSpecificVarTypesRAII(
      llvm::SmallDenseMap<const VarDecl *, Type> SolutionSpecificVarTypes) {
    for (auto SolutionVarType : SolutionSpecificVarTypes) {
      if (SolutionVarType.first->hasInterfaceType()) {
        RestoreVarTypes[SolutionVarType.first] =
            SolutionVarType.first->getInterfaceType();
      } else {
        RestoreVarTypes[SolutionVarType.first] = Type();
      }
      if (!SolutionVarType.second->hasArchetype()) {
        setInterfaceType(const_cast<VarDecl *>(SolutionVarType.first),
                         SolutionVarType.second);
      } else {
        setInterfaceType(const_cast<VarDecl *>(SolutionVarType.first),
                         ErrorType::get(SolutionVarType.second));
      }
    }
  }

  ~WithSolutionSpecificVarTypesRAII() {
    for (auto Var : RestoreVarTypes) {
      setInterfaceType(const_cast<VarDecl *>(Var.first), Var.second);
    }
  }

private:
  /// Sets the interface type of \p VD, similar to \c VD->setInterfaceType
  /// but also allows resetting the interface type of \p VD to null.
  static void setInterfaceType(VarDecl *VD, Type Ty);
};

/// Whether the given completion expression is the only expression in its
/// containing closure or function body and its value is implicitly returned.
///
/// If these conditions are met, code completion needs to avoid penalizing
/// completion results that don't match the expected return type when
/// computing type relations, as since no return statement was explicitly
/// written by the user, it's possible they intend the single expression not
/// as the return value but merely the first entry in a multi-statement body
/// they just haven't finished writing yet.
bool isImplicitSingleExpressionReturn(constraints::ConstraintSystem &CS,
                                      Expr *CompletionExpr);

/// Returns \c true iff the decl context \p DC allows calling async functions.
bool isContextAsync(const constraints::Solution &S, DeclContext *DC);

/// Returns true if both types are null or if they are equal.
bool nullableTypesEqual(Type LHS, Type RHS);

} // namespace ide
} // namespace code

#endif // CODE_IDE_TYPECHECKCOMPLETIONCALLBACK_H
