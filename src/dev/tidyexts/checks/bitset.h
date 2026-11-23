#pragma once

#include "clang-tidy/ClangTidyCheck.h"

namespace clang::tidy::misc {

// Ensures that some unidiomatic constructions with bitsets are not used
class BitsetCheck : public ClangTidyCheck {
public:
    BitsetCheck(StringRef Name, ClangTidyContext* Context) : ClangTidyCheck(Name, Context) {}

    void registerMatchers(ast_matchers::MatchFinder* Finder) override;

    void check(ast_matchers::MatchFinder::MatchResult const& Result) override;
};

}  // namespace clang::tidy::misc