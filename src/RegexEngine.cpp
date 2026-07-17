#include "RegexEngine.hpp"
#include <vector>
#include <algorithm>

// ============================================================================
// Internals — all hidden in an anonymous namespace.
// ============================================================================
namespace {

// ── NFA state types ───────────────────────────────────────────────────────────

enum class SType { Char, Any, Class, Split, Accept };

struct NfaState {
    SType type;
    char  c;        // for Char
    bool  negate;   // for Class
    std::vector<std::pair<unsigned char, unsigned char>> ranges; // for Class
    int   out  = -1; // primary   successor (-1 = dangling/none)
    int   out1 = -1; // secondary successor (Split only)
};

// ── Thompson's construction ───────────────────────────────────────────────────
// Each parse function returns a Frag — an NFA fragment with a known start
// state and a list of "holes" (dangling output slots still waiting to be
// connected to the next fragment).  `patch(holes, target)` fills them in.

struct PatchSlot { int si; bool useOut1; };

struct Frag {
    int start;
    std::vector<PatchSlot> holes;
};

class NfaBuilder {
    std::vector<NfaState>& states_;
    const std::string&     pat_;
    int                    pos_   = 0;
    bool                   valid_ = true;

    int newState(SType t) {
        states_.push_back({t, 0, false, {}, -1, -1});
        return (int)states_.size() - 1;
    }

    void patch(std::vector<PatchSlot>& holes, int target) {
        for (auto& h : holes)
            (h.useOut1 ? states_[h.si].out1 : states_[h.si].out) = target;
        holes.clear();
    }

    // Epsilon fragment: a SPLIT with both outputs dangling.
    // When patched to T it becomes SPLIT(T,T) — a transparent pass-through.
    Frag epsilon() {
        int s = newState(SType::Split);
        return {s, {{s, false}, {s, true}}};
    }

    Frag cat(Frag a, Frag b) { patch(a.holes, b.start); return {a.start, std::move(b.holes)}; }

    Frag alt(Frag a, Frag b) {
        int s = newState(SType::Split);
        states_[s].out  = a.start;
        states_[s].out1 = b.start;
        auto h = std::move(a.holes);
        for (auto& x : b.holes) h.push_back(x);
        return {s, std::move(h)};
    }

    // a* : SPLIT → a → (loop back to SPLIT), exit via SPLIT.out1
    Frag star(Frag f) {
        int s = newState(SType::Split);
        states_[s].out = f.start;
        patch(f.holes, s);
        return {s, {{s, true}}};
    }

    // a+ : enter a, then behave like a*
    Frag plus(Frag f) {
        int s = newState(SType::Split);
        states_[s].out = f.start;
        patch(f.holes, s);
        return {f.start, {{s, true}}};
    }

    // a? : SPLIT → a (match) or skip (SPLIT.out1)
    Frag quest(Frag f) {
        int s = newState(SType::Split);
        states_[s].out = f.start;
        f.holes.push_back({s, true});
        return {s, std::move(f.holes)};
    }

    // ── Recursive-descent parser ──────────────────────────────────────────

    Frag parseAtom() {
        if (pos_ >= (int)pat_.size()) { valid_ = false; return epsilon(); }
        char c = pat_[pos_++];

        if (c == '(') {
            Frag inner = parseExpr();
            if (pos_ < (int)pat_.size() && pat_[pos_] == ')') ++pos_;
            return inner;
        }

        if (c == '[') {
            int s = newState(SType::Class);
            bool neg = (pos_ < (int)pat_.size() && pat_[pos_] == '^');
            if (neg) ++pos_;
            states_[s].negate = neg;

            bool firstChar = true;
            while (pos_ < (int)pat_.size() && (pat_[pos_] != ']' || firstChar)) {
                firstChar = false;
                auto readChar = [&]() -> unsigned char {
                    unsigned char ch = (unsigned char)pat_[pos_++];
                    if (ch == '\\' && pos_ < (int)pat_.size()) {
                        ch = (unsigned char)pat_[pos_++];
                        if (ch == 'n') ch = '\n';
                        else if (ch == 't') ch = '\t';
                    }
                    return ch;
                };
                unsigned char from = readChar();
                if (pos_ + 1 < (int)pat_.size() && pat_[pos_] == '-' && pat_[pos_+1] != ']') {
                    ++pos_; // consume '-'
                    unsigned char to = readChar();
                    states_[s].ranges.push_back({from, to});
                } else {
                    states_[s].ranges.push_back({from, from});
                }
            }
            if (pos_ < (int)pat_.size() && pat_[pos_] == ']') ++pos_;
            return {s, {{s, false}}};
        }

        if (c == '.') { int s = newState(SType::Any); return {s, {{s, false}}}; }

        if (c == '\\' && pos_ < (int)pat_.size()) {
            char esc = pat_[pos_++];
            auto makeClass = [&](std::vector<std::pair<unsigned char,unsigned char>> r, bool neg) {
                int s = newState(SType::Class);
                states_[s].negate = neg;
                states_[s].ranges = std::move(r);
                return Frag{s, {{s, false}}};
            };
            switch (esc) {
                case 'n': { int s=newState(SType::Char); states_[s].c='\n'; return {s,{{s,false}}}; }
                case 't': { int s=newState(SType::Char); states_[s].c='\t'; return {s,{{s,false}}}; }
                case 'r': { int s=newState(SType::Char); states_[s].c='\r'; return {s,{{s,false}}}; }
                case 'd': return makeClass({{'0','9'}}, false);
                case 'D': return makeClass({{'0','9'}}, true);
                case 'w': return makeClass({{'a','z'},{'A','Z'},{'0','9'},{'_','_'}}, false);
                case 'W': return makeClass({{'a','z'},{'A','Z'},{'0','9'},{'_','_'}}, true);
                case 's': return makeClass({{' ',' '},{'\t','\t'},{'\n','\n'},{'\r','\r'}}, false);
                case 'S': return makeClass({{' ',' '},{'\t','\t'},{'\n','\n'},{'\r','\r'}}, true);
                default:  { int s=newState(SType::Char); states_[s].c=esc; return {s,{{s,false}}}; }
            }
        }

        // Literal character (including unmatched *, +, ?, ), | — treat as literal
        int s = newState(SType::Char);
        states_[s].c = c;
        return {s, {{s, false}}};
    }

    Frag parseFactor() {
        Frag f = parseAtom();
        if (!valid_ || pos_ >= (int)pat_.size()) return f;
        switch (pat_[pos_]) {
            case '*': ++pos_; return star(f);
            case '+': ++pos_; return plus(f);
            case '?': ++pos_; return quest(f);
        }
        return f;
    }

    Frag parseTerm() {
        if (pos_ >= (int)pat_.size() || pat_[pos_] == '|' || pat_[pos_] == ')')
            return epsilon();
        Frag result = parseFactor();
        while (valid_ && pos_ < (int)pat_.size() &&
               pat_[pos_] != '|' && pat_[pos_] != ')')
            result = cat(result, parseFactor());
        return result;
    }

    Frag parseExpr() {
        Frag result = parseTerm();
        while (valid_ && pos_ < (int)pat_.size() && pat_[pos_] == '|') {
            ++pos_;
            result = alt(result, parseTerm());
        }
        return result;
    }

public:
    NfaBuilder(std::vector<NfaState>& states, const std::string& pat)
        : states_(states), pat_(pat) {}

    bool build(int& outStart, int& outAccept) {
        Frag f  = parseExpr();
        if (!valid_) return false;
        outAccept = newState(SType::Accept);
        patch(f.holes, outAccept);
        outStart = f.start;
        return true;
    }
};

// ── NFA simulation ────────────────────────────────────────────────────────────

// Recursively expands epsilon (Split) transitions from state `s` into `result`.
void epsClosure(const std::vector<NfaState>& states,
                std::vector<bool>& visited,
                std::vector<int>&  result,
                int s) {
    if (s < 0 || s >= (int)states.size() || visited[s]) return;
    visited[s] = true;
    result.push_back(s);
    if (states[s].type == SType::Split) {
        epsClosure(states, visited, result, states[s].out);
        epsClosure(states, visited, result, states[s].out1);
    }
}

std::vector<int> closure(const std::vector<NfaState>& states,
                         const std::vector<int>& seeds) {
    std::vector<bool> visited(states.size(), false);
    std::vector<int>  result;
    for (int s : seeds) epsClosure(states, visited, result, s);
    return result;
}

bool charMatches(const NfaState& st, char c) {
    switch (st.type) {
        case SType::Char: return st.c == c;
        case SType::Any:  return c != '\n';
        case SType::Class: {
            bool inCls = false;
            for (auto& [lo, hi] : st.ranges)
                if ((unsigned char)c >= lo && (unsigned char)c <= hi) { inCls = true; break; }
            return st.negate ? !inCls : inCls;
        }
        default: return false;
    }
}

// Attempts a match starting at text[startPos].
// Returns the length of the longest match, or -1 if no match exists.
int matchAt(const std::vector<NfaState>& states, int nfaStart, int nfaAccept,
            const std::string& text, int startPos) {
    std::vector<int> cur = closure(states, {nfaStart});
    int matchLen = -1;
    for (int s : cur) if (s == nfaAccept) matchLen = 0;

    for (int i = startPos; i < (int)text.size(); ++i) {
        std::vector<int> nextSeeds;
        for (int s : cur) {
            if (states[s].type == SType::Accept || states[s].type == SType::Split) continue;
            if (charMatches(states[s], text[i]) && states[s].out >= 0)
                nextSeeds.push_back(states[s].out);
        }
        cur = closure(states, nextSeeds);
        if (cur.empty()) break;
        for (int s : cur) if (s == nfaAccept) matchLen = i - startPos + 1;
    }
    return matchLen;
}

// Strips leading ^ and trailing $ anchors from `pat`, setting the flags.
std::string stripAnchors(const std::string& pat, bool& anchoredStart, bool& anchoredEnd) {
    std::string s = pat;
    anchoredStart = !s.empty() && s[0] == '^';
    if (anchoredStart) s = s.substr(1);
    anchoredEnd = !s.empty() && s.back() == '$' &&
                  (s.size() < 2 || s[s.size()-2] != '\\');
    if (anchoredEnd) s.pop_back();
    return s;
}

} // namespace

// ── Public API ────────────────────────────────────────────────────────────────

bool RegexEngine::isValid(const std::string& pattern) {
    bool as, ae;
    std::string pat = stripAnchors(pattern, as, ae);
    if (pat.empty()) return true;
    std::vector<NfaState> states;
    states.reserve(pat.size() * 3 + 2);
    int start, accept;
    NfaBuilder builder(states, pat);
    return builder.build(start, accept);
}

std::vector<RegexEngine::Match> RegexEngine::findAll(
    const std::string& pattern, const std::string& text)
{
    std::vector<Match> results;
    if (pattern.empty()) return results;

    bool anchoredStart, anchoredEnd;
    std::string pat = stripAnchors(pattern, anchoredStart, anchoredEnd);
    if (pat.empty()) return results; // anchor-only patterns not meaningful for search

    std::vector<NfaState> states;
    states.reserve(pat.size() * 3 + 2);
    int nfaStart, nfaAccept;
    NfaBuilder builder(states, pat);
    if (!builder.build(nfaStart, nfaAccept)) return results;

    int i = 0;
    while (i <= (int)text.size()) {
        int len = matchAt(states, nfaStart, nfaAccept, text, i);
        if (len > 0) {
            bool endOk = !anchoredEnd || (i + len == (int)text.size());
            if (endOk) results.push_back({i, len});
            i += len;
        } else {
            i++;
        }
        if (anchoredStart) break; // only try from position 0
    }
    return results;
}
