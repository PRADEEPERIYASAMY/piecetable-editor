#pragma once
#include <string>
#include <vector>

// ============================================================================
// RegexEngine
//
// An NFA-based regular expression engine built with Thompson's construction.
//
// Supported syntax:
//   .          any character (except newline)
//   *  +  ?    greedy quantifiers (zero-or-more, one-or-more, zero-or-one)
//   |          alternation
//   ( ... )    grouping (affects quantifier scope, not capture)
//   [ abc ]    character class
//   [ a-z ]    range inside class
//   [^ abc ]   negated class
//   ^          start-of-line anchor (only at pattern start)
//   $          end-of-line anchor (only at pattern end)
//   \n \t \r   common escape sequences
//   \d \w \s   digit / word / whitespace shorthands
//   \D \W \S   negated shorthands
//   \\         literal backslash
//
// Algorithm — Thompson NFA simulation:
//   1. Parse the pattern into an NFA using Thompson's construction (each
//      operator maps to a small NFA fragment glued together with epsilon
//      transitions — see the "cat / alt / star / plus / quest" functions in
//      the .cpp).
//   2. Simulate the NFA by maintaining the *set* of active states rather than
//      a single state, advancing the whole set on each input character.
//      This guarantees O(pattern_len × text_len) worst-case time with no
//      catastrophic backtracking — unlike the recursive backtracking used by
//      std::regex with certain patterns.
//
// Reference: "Regular Expression Matching Can Be Simple And Fast",
//            Russ Cox (https://swtch.com/~rsc/regexp/regexp1.html)
// ============================================================================

class RegexEngine {
public:
    struct Match {
        int col;    // 0-indexed column of the match start in the line
        int length; // number of characters matched (always > 0 in findAll output)
    };

    // Compiles `pattern` and returns all non-overlapping, leftmost-longest
    // matches found in `text`. Zero-length matches are suppressed.
    // Returns an empty vector if `pattern` is syntactically invalid.
    static std::vector<Match> findAll(const std::string& pattern,
                                     const std::string& text);

    // Returns true if `pattern` compiles without error.
    static bool isValid(const std::string& pattern);
};
