#include "checks/bitset.h"

#include <string>

#include "clang/AST/ExprCXX.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Lex/Lexer.h"

namespace clang::tidy::misc {
using namespace ast_matchers;

void BitsetCheck::registerMatchers(MatchFinder* Finder) {
    auto const bitset_decl = cxxRecordDecl(hasName("::boost::dynamic_bitset"));
    auto const bitset =
            hasType(qualType(hasUnqualifiedDesugaredType(recordType(hasDeclaration(bitset_decl)))));
    auto const positive = ignoringParenImpCasts(expr(bitset).bind("positive"));
    auto const negative = ignoringParenImpCasts(cxxOperatorCallExpr(
            hasOverloadedOperatorName("~"), callee(cxxMethodDecl(ofClass(bitset_decl))),
            hasUnaryOperand(ignoringParenImpCasts(expr(bitset).bind("negative")))));
    auto const difference = cxxOperatorCallExpr(hasOverloadedOperatorName("&"),
                                                callee(functionDecl(hasName("::boost::operator&"))),
                                                hasOperands(positive, negative))
                                    .bind("difference");
    auto const none_method = callee(cxxMethodDecl(hasName("none"), ofClass(bitset_decl)));

    Finder->addMatcher(traverse(TK_IgnoreUnlessSpelledInSource,
                                cxxMemberCallExpr(none_method, on(difference)).bind("none")),
                       this);
    Finder->addMatcher(
            traverse(TK_IgnoreUnlessSpelledInSource,
                     expr(difference, unless(hasAncestor(cxxMemberCallExpr(
                                              none_method, on(equalsBoundNode("difference"))))))),
            this);
}

void BitsetCheck::check(MatchFinder::MatchResult const& Result) {
    auto const* difference = Result.Nodes.getNodeAs<CXXOperatorCallExpr>("difference");
    auto const* none = Result.Nodes.getNodeAs<CXXMemberCallExpr>("none");
    auto const& replacement = none ? static_cast<Expr const&>(*none) : *difference;
    auto diagnostic = diag(none ? none->getExprLoc() : difference->getOperatorLoc(),
                           none ? "use is_subset_of() to test for a bitset subset"
                                : "use operator- to compute a bitset difference");

    auto const& sm = *Result.SourceManager;
    auto const& lang = Result.Context->getLangOpts();
    auto const range = Lexer::makeFileCharRange(
            CharSourceRange::getTokenRange(replacement.getSourceRange()), sm, lang);
    if (range.isInvalid() || difference->getOperatorLoc().isMacroID() ||
        replacement.getBeginLoc().isMacroID() || replacement.getEndLoc().isMacroID()) {
        return;
    }

    auto const source = Lexer::getSourceText(range, sm, lang);
    if (source.contains("/*") || source.contains("//") || source.contains('#')) return;

    auto const operand_text = [&](Expr const& operand) -> std::string {
        bool invalid = false;
        auto const text = Lexer::getSourceText(
                CharSourceRange::getTokenRange(operand.getSourceRange()), sm, lang, &invalid);
        if (invalid || text.empty()) return {};
        if (isa<DeclRefExpr, MemberExpr, ParenExpr>(operand) ||
            (isa<CallExpr>(operand) && !isa<CXXOperatorCallExpr>(operand))) {
            return text.str();
        }
        return ("(" + text + ")").str();
    };

    auto const lhs = operand_text(*Result.Nodes.getNodeAs<Expr>("positive"));
    auto const rhs = operand_text(*Result.Nodes.getNodeAs<Expr>("negative"));
    if (lhs.empty() || rhs.empty()) return;

    diagnostic << FixItHint::CreateReplacement(
            range, none ? lhs + ".is_subset_of(" + rhs + ")" : lhs + " - " + rhs);
}

}  // namespace clang::tidy::misc
