#include "analyzer.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <algorithm>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif

namespace safe_c::core {

namespace {

// Token representations for structural C/C++ parsing
struct Token {
    std::string text;
    SourceLocation loc;
};

std::vector<Token> tokenize(std::string_view code, std::string_view filename) {
    std::vector<Token> tokens;
    const std::string canon_fn = resolve_canonical_path(filename);
    const std::string fn(filename);
    uint32_t line = 1;
    uint32_t col = 1;
    size_t i = 0;
    const size_t len = code.size();

    while (i < len) {
        char c = code[i];

        // Handle line counting
        if (c == '\n') {
            line++;
            col = 1;
            i++;
            continue;
        }

        // Skip whitespace
        if (std::isspace(static_cast<unsigned char>(c))) {
            col++;
            i++;
            continue;
        }

        // Single-line comments
        if (c == '/' && i + 1 < len && code[i + 1] == '/') {
            while (i < len && code[i] != '\n') {
                i++;
            }
            continue;
        }

        // Multi-line comments
        if (c == '/' && i + 1 < len && code[i + 1] == '*') {
            i += 2;
            col += 2;
            while (i + 1 < len && !(code[i] == '*' && code[i + 1] == '/')) {
                if (code[i] == '\n') {
                    line++;
                    col = 1;
                } else {
                    col++;
                }
                i++;
            }
            if (i + 1 < len) {
                i += 2;
                col += 2;
            }
            continue;
        }

        // String literals
        if (c == '"') {
            uint32_t start_col = col;
            std::string str;
            str += c;
            i++;
            col++;
            while (i < len && code[i] != '"') {
                if (code[i] == '\\' && i + 1 < len) {
                    str += code[i++];
                    col++;
                }
                str += code[i++];
                col++;
            }
            if (i < len) {
                str += code[i++];
                col++;
            }
            uint32_t tok_len = col - start_col;
            tokens.push_back({std::move(str), SourceLocation(fn, canon_fn, line, start_col, tok_len)});
            continue;
        }

        // Identifiers and keywords
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            uint32_t start_col = col;
            std::string ident;
            while (i < len && (std::isalnum(static_cast<unsigned char>(code[i])) || code[i] == '_')) {
                ident += code[i++];
                col++;
            }
            uint32_t tok_len = col - start_col;
            tokens.push_back({std::move(ident), SourceLocation(fn, canon_fn, line, start_col, tok_len)});
            continue;
        }

        // Numbers
        if (std::isdigit(static_cast<unsigned char>(c))) {
            uint32_t start_col = col;
            std::string num;
            while (i < len && (std::isalnum(static_cast<unsigned char>(code[i])) || code[i] == '.' || code[i] == 'x' || code[i] == 'X')) {
                num += code[i++];
                col++;
            }
            uint32_t tok_len = col - start_col;
            tokens.push_back({std::move(num), SourceLocation(fn, canon_fn, line, start_col, tok_len)});
            continue;
        }

        // Multi-character operators: ++, --, +=, -=, ->, ==, !=
        if (i + 1 < len) {
            std::string two_char{c, code[i + 1]};
            if (two_char == "++" || two_char == "--" || two_char == "+=" || two_char == "-=" ||
                two_char == "->" || two_char == "==" || two_char == "!=") {
                tokens.push_back({two_char, SourceLocation(fn, canon_fn, line, col, 2)});
                i += 2;
                col += 2;
                continue;
            }
        }

        // Single-character symbols
        tokens.push_back({std::string(1, c), SourceLocation(fn, canon_fn, line, col, 1)});
        i++;
        col++;
    }

    return tokens;
}

bool should_use_color(ColorChoice choice, std::ostream& os) {
    if (choice == ColorChoice::Never) return false;
    if (choice == ColorChoice::Always) return true;

    const char* no_color = std::getenv("NO_COLOR");
    if (no_color && no_color[0] != '\0') return false;

    const char* gcc_colors = std::getenv("GCC_COLORS");
    if (gcc_colors && gcc_colors[0] == '\0') return false;

    const char* safe_c_color = std::getenv("SAFE_C_COLOR");
    if (safe_c_color) {
        if (std::strcmp(safe_c_color, "never") == 0 || std::strcmp(safe_c_color, "0") == 0) return false;
        if (std::strcmp(safe_c_color, "always") == 0 || std::strcmp(safe_c_color, "1") == 0) return true;
    }

#if defined(_WIN32)
    HANDLE hOut = NULL;
    if (&os == &std::cerr) {
        hOut = GetStdHandle(STD_ERROR_HANDLE);
    } else if (&os == &std::cout) {
        hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    }
    if (hOut && hOut != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (GetConsoleMode(hOut, &mode)) {
            if (!(mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
                SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
            }
            return true;
        }
    }
    return false;
#else
    if (&os == &std::cerr) {
        return isatty(fileno(stderr)) == 1;
    }
    if (&os == &std::cout) {
        return isatty(fileno(stdout)) == 1;
    }
    return false;
#endif
}

std::string colorize_message(const std::string& msg, bool use_color) {
    if (!use_color) return msg;
    std::string result;
    bool in_quote = false;
    for (size_t i = 0; i < msg.size(); ++i) {
        char c = msg[i];
        if (c == '\'') {
            if (!in_quote) {
                result += "\033[01m'";
                in_quote = true;
            } else {
                result += "'\033[0m";
                in_quote = false;
            }
        } else {
            result += c;
        }
    }
    if (in_quote) {
        result += "\033[0m";
    }
    return result;
}

std::string highlight_source_line(std::string_view line, bool use_color) {
    if (!use_color) return std::string(line);

    static const std::unordered_set<std::string_view> keywords = {
        "void", "char", "short", "int", "long", "float", "double", "signed",
        "unsigned", "bool", "auto", "const", "static", "sizeof", "struct",
        "union", "enum", "typedef", "return", "if", "else", "while", "for",
        "do", "switch", "case", "break", "continue", "default", "goto",
        "NULL", "nullptr", "malloc", "calloc", "realloc", "free", "new",
        "delete", "class", "public", "private", "protected", "virtual",
        "override", "template", "typename", "using", "namespace", "std"
    };

    std::string result;
    size_t i = 0;
    const size_t len = line.size();

    while (i < len) {
        char c = line[i];

        // Single line comment
        if (c == '/' && i + 1 < len && line[i + 1] == '/') {
            result += "\033[90m";
            result += line.substr(i);
            result += "\033[0m";
            break;
        }

        // String / char literals
        if (c == '"' || c == '\'') {
            char quote = c;
            result += "\033[32m";
            result += quote;
            i++;
            while (i < len && line[i] != quote) {
                if (line[i] == '\\' && i + 1 < len) {
                    result += line[i++];
                }
                result += line[i++];
            }
            if (i < len) {
                result += line[i++];
            }
            result += "\033[0m";
            continue;
        }

        // Identifiers and keywords
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            size_t start = i;
            while (i < len && (std::isalnum(static_cast<unsigned char>(line[i])) || line[i] == '_')) {
                i++;
            }
            std::string_view token = line.substr(start, i - start);
            if (keywords.find(token) != keywords.end()) {
                result += "\033[01;36m";
                result += token;
                result += "\033[0m";
            } else {
                result += token;
            }
            continue;
        }

        // Numbers
        if (std::isdigit(static_cast<unsigned char>(c))) {
            result += "\033[35m";
            while (i < len && (std::isalnum(static_cast<unsigned char>(line[i])) || line[i] == '.' || line[i] == 'x' || line[i] == 'X')) {
                result += line[i++];
            }
            result += "\033[0m";
            continue;
        }

        result += c;
        i++;
    }
    return result;
}

std::string compute_indent(std::string_view line_str, uint32_t col) {
    std::string indent;
    if (col > 1 && col - 1 <= line_str.length()) {
        for (size_t k = 0; k < col - 1; ++k) {
            indent += (line_str[k] == '\t') ? '\t' : ' ';
        }
    } else {
        indent = std::string(col > 0 ? col - 1 : 0, ' ');
    }
    return indent;
}

} // anonymous namespace

Analyzer::Analyzer(Mode mode) : mode_(mode) {}

bool Analyzer::analyze_file(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "safe-c: fatal error: cannot open file: " << filepath << "\n";
        return false;
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return analyze_source(ss.str(), filepath);
}

void Analyzer::record_violation(
    ViolationType type,
    SourceLocation err_loc,
    std::string primary_msg,
    SourceLocation note_loc,
    std::string note_msg
) {
    // Deduplication with address scope resolution:
    // Ensures 1 particular line is shown once only for a given error type.
    // If the same line has a different error, it is recorded and displayed.
    for (const auto& existing : diagnostics_) {
        if (existing.error_loc.matches_file(err_loc) &&
            existing.error_loc.line == err_loc.line &&
            existing.type == type) {
            return;
        }
    }

    diagnostics_.push_back(Diagnostic{
        type,
        std::move(err_loc),
        std::move(primary_msg),
        std::move(note_loc),
        std::move(note_msg)
    });
}

bool Analyzer::analyze_source(std::string_view source_code, std::string_view filename) {
    if (mode_ == Mode::Disabled) {
        return true;
    }

    // Cache source code lines for GNU/GCC error snippet and gutter rendering
    const std::string canon_fn = resolve_canonical_path(filename);
    const std::string fn(filename);
    std::vector<std::string> lines;
    std::string_view remaining = source_code;
    while (!remaining.empty()) {
        size_t pos = remaining.find('\n');
        if (pos == std::string_view::npos) {
            std::string_view line_sv = remaining;
            if (!line_sv.empty() && line_sv.back() == '\r') {
                line_sv.remove_suffix(1);
            }
            lines.emplace_back(line_sv);
            break;
        }
        std::string_view line_sv = remaining.substr(0, pos);
        if (!line_sv.empty() && line_sv.back() == '\r') {
            line_sv.remove_suffix(1);
        }
        lines.emplace_back(line_sv);
        remaining.remove_prefix(pos + 1);
    }
    source_line_cache_[canon_fn] = lines;
    source_line_cache_[fn] = lines;

    auto tokens = tokenize(source_code, filename);
    const size_t num_tokens = tokens.size();

    for (size_t i = 0; i < num_tokens; ++i) {
        const auto& tok = tokens[i];

        // Lexical scope entry matching GCC compound statement semantics
        if (tok.text == "{") {
            state_.enter_scope();
            continue;
        }

        // Lexical scope exit: deallocations bounded by lexical block
        if (tok.text == "}") {
            auto leaks = state_.exit_scope();
            for (const auto& [symbol, cell] : leaks) {
                record_violation(
                    ViolationType::MemoryLeakUnfreed,
                    tok.loc,
                    "[Safe-C] Memory leak detected: dynamically allocated pointer '" + symbol + "' was not freed before exiting scope.",
                    cell->alloc_site,
                    "Memory was allocated here and never freed:"
                );
            }
            if (state_.current_scope() == 0) {
                state_.clear();
            }
            continue;
        }

        // Return statement check: leak detection for unescaped pointers
        if (tok.text == "return") {
            // Check if return returns one of our pointers: return ptr;
            if (i + 1 < num_tokens && state_.has_symbol(tokens[i + 1].text)) {
                state_.mark_escaped(tokens[i + 1].text);
            }
            // Check if this is a null check: if (!ptr) return; or if (ptr == NULL) return;
            bool is_null_check = false;
            if (i >= 3) {
                for (int back = static_cast<int>(i) - 1; back >= 0 && back >= static_cast<int>(i) - 6; --back) {
                    if (tokens[back].text == "!" || tokens[back].text == "NULL" || tokens[back].text == "nullptr") {
                        is_null_check = true;
                        break;
                    }
                    if (tokens[back].text == ";") break;
                }
            }

            if (!is_null_check && state_.current_scope() <= 1) {
                auto leaks = state_.find_leaks();
                for (const auto& [symbol, cell] : leaks) {
                    record_violation(
                        ViolationType::MemoryLeakUnfreed,
                        tok.loc,
                        "[Safe-C] Memory leak: Pointer '" + symbol + "' allocated on heap is not deallocated before return.",
                        cell->alloc_site,
                        "Original allocation occurred here:"
                    );
                }
                state_.clear();
            }
            continue;
        }

        // 1. Detect dynamic allocation: malloc, calloc, aligned_alloc
        if (tok.text == "malloc" || tok.text == "calloc" || tok.text == "aligned_alloc") {
            // Find assigned symbol by looking backwards across '='
            if (i >= 2) {
                for (int back = static_cast<int>(i) - 1; back >= 0; --back) {
                    if (tokens[back].text == "=" && back > 0) {
                        std::string target_sym = tokens[back - 1].text;
                        if (target_sym == "*") {
                            if (back >= 2) target_sym = tokens[back - 2].text;
                        }
                        state_.register_allocation(target_sym, tok.loc, 64, false, state_.current_scope());
                        break;
                    }
                    if (tokens[back].text == ";" || tokens[back].text == "{" || tokens[back].text == "}") {
                        break;
                    }
                }
            }
            continue;
        }

        // C++ operator new
        if (tok.text == "new") {
            bool is_array = false;
            if (i + 2 < num_tokens && tokens[i + 2].text == "[") {
                is_array = true;
            }
            if (i >= 2) {
                for (int back = static_cast<int>(i) - 1; back >= 0; --back) {
                    if (tokens[back].text == "=" && back > 0) {
                        std::string target_sym = tokens[back - 1].text;
                        state_.register_allocation(target_sym, tok.loc, 64, is_array, state_.current_scope());
                        break;
                    }
                    if (tokens[back].text == ";" || tokens[back].text == "{") {
                        break;
                    }
                }
            }
            continue;
        }

        // 2. Detect pointer assignment / alias: q = p; or q = p + 4;
        if (tok.text == "=" && i > 0 && i + 1 < num_tokens) {
            std::string lhs = tokens[i - 1].text;
            std::string rhs = tokens[i + 1].text;
            if (lhs != rhs && state_.has_symbol(rhs)) {
                int64_t offset_delta = 0;
                if (i + 3 < num_tokens && (tokens[i + 2].text == "+" || tokens[i + 2].text == "-")) {
                    try {
                        int64_t val = std::stoll(tokens[i + 3].text);
                        offset_delta = (tokens[i + 2].text == "+") ? val : -val;
                    } catch (...) {}
                }
                state_.register_alias(lhs, rhs, offset_delta, state_.current_scope());
            }
        }

        // 3. Pointer arithmetic tracking with architecture-adaptive word stride
        const int64_t arch_stride = static_cast<int64_t>(sizeof(void*));
        auto check_and_apply_arithmetic = [&](const std::string& sym, int64_t delta, const SourceLocation& loc) {
            if (!state_.has_symbol(sym)) return;
            const auto* binding = state_.get_binding(sym);
            if (!binding || !binding->cell()) return;

            // Offset can be changed as long as there is a variable that contains the original base address
            if (!state_.can_alter_offset(sym, delta)) {
                record_violation(
                    ViolationType::ModifiedBaseFree,
                    loc,
                    "[Safe-C] Cannot alter pointer '" + sym + "' offset: no variable retains the original allocation base address.",
                    binding->cell()->alloc_site,
                    "Original allocation occurred here:"
                );
            }
            state_.apply_pointer_arithmetic(sym, delta);
        };

        if (tok.text == "++") {
            if (i > 0 && state_.has_symbol(tokens[i - 1].text)) {
                check_and_apply_arithmetic(tokens[i - 1].text, arch_stride, tok.loc);
            } else if (i + 1 < num_tokens && state_.has_symbol(tokens[i + 1].text)) {
                check_and_apply_arithmetic(tokens[i + 1].text, arch_stride, tok.loc);
            }
        } else if (tok.text == "--") {
            if (i > 0 && state_.has_symbol(tokens[i - 1].text)) {
                check_and_apply_arithmetic(tokens[i - 1].text, -arch_stride, tok.loc);
            } else if (i + 1 < num_tokens && state_.has_symbol(tokens[i + 1].text)) {
                check_and_apply_arithmetic(tokens[i + 1].text, -arch_stride, tok.loc);
            }
        } else if (tok.text == "+=" && i > 0 && state_.has_symbol(tokens[i - 1].text)) {
            int64_t val = arch_stride;
            if (i + 1 < num_tokens) {
                try { val = std::stoll(tokens[i + 1].text); } catch (...) {}
            }
            check_and_apply_arithmetic(tokens[i - 1].text, val, tok.loc);
        } else if (tok.text == "-=" && i > 0 && state_.has_symbol(tokens[i - 1].text)) {
            int64_t val = arch_stride;
            if (i + 1 < num_tokens) {
                try { val = std::stoll(tokens[i + 1].text); } catch (...) {}
            }
            check_and_apply_arithmetic(tokens[i - 1].text, -val, tok.loc);
        } else if (tok.text == "=" && i > 0 && i + 3 < num_tokens &&
                   tokens[i - 1].text == tokens[i + 1].text && state_.has_symbol(tokens[i - 1].text) &&
                   (tokens[i + 2].text == "+" || tokens[i + 2].text == "-")) {
            int64_t val = arch_stride;
            try { val = std::stoll(tokens[i + 3].text); } catch (...) {}
            int64_t delta = (tokens[i + 2].text == "+") ? val : -val;
            check_and_apply_arithmetic(tokens[i - 1].text, delta, tok.loc);
        }

        // 4. Detect deallocation: free(ptr), delete ptr, delete[] ptr
        if (tok.text == "free" && i + 2 < num_tokens && tokens[i + 1].text == "(") {
            std::string target_sym = tokens[i + 2].text;
            std::shared_ptr<AllocationCell> cell;
            int64_t offset = 0;
            auto err = state_.register_free(target_sym, tok.loc, cell, offset);

            if (err.has_value()) {
                if (*err == ViolationType::DoubleFree) {
                    record_violation(
                        ViolationType::DoubleFree,
                        tok.loc,
                        "[Safe-C] Double free detected on pointer '" + target_sym + "'.",
                        cell ? cell->free_site : SourceLocation{},
                        "Previous deallocation occurred here:"
                    );
                } else if (*err == ViolationType::ModifiedBaseFree) {
                    record_violation(
                        ViolationType::ModifiedBaseFree,
                        tok.loc,
                        "[Safe-C] Attempt to free altered/interior pointer '" + target_sym + "'. Address was shifted by " +
                            std::to_string(offset) + " bytes before deallocation.",
                        cell ? cell->alloc_site : SourceLocation{},
                        "Original allocation base was created here:"
                    );
                }
            }
            continue;
        }

        if (tok.text == "delete") {
            size_t target_idx = i + 1;
            if (target_idx < num_tokens && tokens[target_idx].text == "[" &&
                target_idx + 1 < num_tokens && tokens[target_idx + 1].text == "]") {
                target_idx += 2; // delete[]
            }
            if (target_idx < num_tokens) {
                std::string target_sym = tokens[target_idx].text;
                std::shared_ptr<AllocationCell> cell;
                int64_t offset = 0;
                auto err = state_.register_free(target_sym, tok.loc, cell, offset);

                if (err.has_value()) {
                    if (*err == ViolationType::DoubleFree) {
                        record_violation(
                            ViolationType::DoubleFree,
                            tok.loc,
                            "[Safe-C] Double free / delete detected on pointer '" + target_sym + "'.",
                            cell ? cell->free_site : SourceLocation{},
                            "Pointer was previously deleted here:"
                        );
                    } else if (*err == ViolationType::ModifiedBaseFree) {
                        record_violation(
                            ViolationType::ModifiedBaseFree,
                            tok.loc,
                            "[Safe-C] Attempt to delete pointer '" + target_sym + "' with modified base offset (offset = " +
                                std::to_string(offset) + ").",
                            cell ? cell->alloc_site : SourceLocation{},
                            "Original allocation occurred here:"
                        );
                    }
                }
            }
            continue;
        }

        // 5. Detect Use-After-Free: *ptr, ptr->field, ptr[i]
        if (tok.text == "*" && i + 1 < num_tokens && state_.has_symbol(tokens[i + 1].text)) {
            std::shared_ptr<AllocationCell> cell;
            auto err = state_.validate_access(tokens[i + 1].text, cell);
            if (err.has_value() && *err == ViolationType::UseAfterFree) {
                record_violation(
                    ViolationType::UseAfterFree,
                    tok.loc,
                    "[Safe-C] Use-After-Free: Pointer '" + tokens[i + 1].text + "' dereferenced after deallocation.",
                    cell ? cell->free_site : SourceLocation{},
                    "Pointer was deallocated here:"
                );
            }
        } else if (tok.text == "->" && i > 0 && state_.has_symbol(tokens[i - 1].text)) {
            std::shared_ptr<AllocationCell> cell;
            auto err = state_.validate_access(tokens[i - 1].text, cell);
            if (err.has_value() && *err == ViolationType::UseAfterFree) {
                record_violation(
                    ViolationType::UseAfterFree,
                    tok.loc,
                    "[Safe-C] Use-After-Free: Accessing member on freed pointer '" + tokens[i - 1].text + "'.",
                    cell ? cell->free_site : SourceLocation{},
                    "Pointer was deallocated here:"
                );
            }
        } else if (tok.text == "[" && i > 0 && state_.has_symbol(tokens[i - 1].text)) {
            std::shared_ptr<AllocationCell> cell;
            auto err = state_.validate_access(tokens[i - 1].text, cell);
            if (err.has_value() && *err == ViolationType::UseAfterFree) {
                record_violation(
                    ViolationType::UseAfterFree,
                    tok.loc,
                    "[Safe-C] Use-After-Free: Array subscript index on freed pointer '" + tokens[i - 1].text + "'.",
                    cell ? cell->free_site : SourceLocation{},
                    "Pointer was deallocated here:"
                );
            }
        }
    }

    return !has_errors();
}

void Analyzer::print_diagnostics(std::ostream& os) const {
    const bool use_color = should_use_color(color_mode_, os);

    auto render_snippet = [&](const SourceLocation& loc, bool is_error) {
        if (loc.line == 0) return;
        auto it = source_line_cache_.find(loc.canonical_path);
        if (it == source_line_cache_.end()) {
            it = source_line_cache_.find(loc.filename);
        }
        if (it == source_line_cache_.end() || loc.line > it->second.size()) {
            return;
        }

        const std::string& line_str = it->second[loc.line - 1];
        int line_str_len = static_cast<int>(std::to_string(loc.line).length());
        int gutter_width = std::max(5, line_str_len + 1);

        // Gutter line: "    6 | " in bold blue (\033[01;34m)
        std::string line_num_str = std::to_string(loc.line);
        std::string pad(gutter_width - line_num_str.length(), ' ');
        if (use_color) os << "\033[01;34m";
        os << pad << line_num_str << " | ";
        if (use_color) os << "\033[0m";

        os << highlight_source_line(line_str, use_color) << "\n";

        // Caret line: "      |     ^~~~~~~~~"
        std::string caret_pad(gutter_width, ' ');
        if (use_color) os << "\033[01;34m";
        os << caret_pad << " | ";
        if (use_color) os << "\033[0m";

        os << compute_indent(line_str, loc.column);

        uint32_t caret_len = (loc.length > 0) ? loc.length : 1;
        std::string caret_str = "^" + std::string(caret_len > 1 ? caret_len - 1 : 0, '~');
        if (use_color) {
            os << (is_error ? "\033[01;31m" : "\033[01;36m");
        }
        os << caret_str;
        if (use_color) os << "\033[0m";
        os << "\n";
    };

    for (const auto& diag : diagnostics_) {
        // Location header: filename:line:col:
        if (use_color) os << "\033[01m";
        os << diag.error_loc.filename << ":" << diag.error_loc.line << ":" << diag.error_loc.column << ": ";
        if (use_color) os << "\033[0m";

        // Severity: error: in bold red
        if (use_color) os << "\033[01;31m";
        os << "error: ";
        if (use_color) os << "\033[0m";

        os << colorize_message(diag.primary_message, use_color) << "\n";
        render_snippet(diag.error_loc, true);

        if (!diag.note_message.empty() && !diag.note_loc.filename.empty()) {
            if (use_color) os << "\033[01m";
            os << diag.note_loc.filename << ":" << diag.note_loc.line << ":" << diag.note_loc.column << ": ";
            if (use_color) os << "\033[0m";

            if (use_color) os << "\033[01;36m";
            os << "note: ";
            if (use_color) os << "\033[0m";

            os << colorize_message(diag.note_message, use_color) << "\n";
            render_snippet(diag.note_loc, false);
        }
    }

    if (!diagnostics_.empty()) {
        if (use_color) os << "\033[01;31m";
        os << "compilation terminated due to memory safety violations.\n";
        if (use_color) os << "\033[0m";
    }
}

} // namespace safe_c::core
