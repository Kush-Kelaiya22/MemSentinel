#include "analyzer.h"
#include <fstream>
#include <sstream>
#include <regex>
#include <cctype>

namespace memcheck::core {

namespace {

// Token representations for structural C/C++ parsing
struct Token {
    std::string text;
    SourceLocation loc;
};

std::vector<Token> tokenize(std::string_view code, std::string_view filename) {
    std::vector<Token> tokens;
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
            tokens.push_back({std::move(str), {std::string(filename), line, start_col}});
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
            tokens.push_back({std::move(ident), {std::string(filename), line, start_col}});
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
            tokens.push_back({std::move(num), {std::string(filename), line, start_col}});
            continue;
        }

        // Multi-character operators: ++, --, +=, -=, ->, ==, !=
        if (i + 1 < len) {
            std::string two_char{c, code[i + 1]};
            if (two_char == "++" || two_char == "--" || two_char == "+=" || two_char == "-=" ||
                two_char == "->" || two_char == "==" || two_char == "!=") {
                tokens.push_back({two_char, {std::string(filename), line, col}});
                i += 2;
                col += 2;
                continue;
            }
        }

        // Single-character symbols
        tokens.push_back({std::string(1, c), {std::string(filename), line, col}});
        i++;
        col++;
    }

    return tokens;
}

} // anonymous namespace

Analyzer::Analyzer(Mode mode) : mode_(mode) {}

bool Analyzer::analyze_file(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "memcheck: fatal error: cannot open file: " << filepath << "\n";
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

    auto tokens = tokenize(source_code, filename);
    const size_t num_tokens = tokens.size();
    int scope_depth = 0;

    for (size_t i = 0; i < num_tokens; ++i) {
        const auto& tok = tokens[i];

        if (tok.text == "{") {
            scope_depth++;
            continue;
        }
        if (tok.text == "}") {
            scope_depth--;
            if (scope_depth <= 0) {
                // Function exit: Check for memory leaks!
                auto leaks = state_.find_leaks();
                for (const auto& [symbol, cell] : leaks) {
                    record_violation(
                        ViolationType::MemoryLeakUnfreed,
                        tok.loc,
                        "[MEMSENTINEL] Memory leak detected: dynamically allocated pointer '" + symbol + "' was not freed before exiting scope.",
                        cell->alloc_site,
                        "Memory was allocated here and never freed:"
                    );
                }
                scope_depth = 0;
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

            if (!is_null_check && scope_depth <= 1) {
                auto leaks = state_.find_leaks();
                for (const auto& [symbol, cell] : leaks) {
                    record_violation(
                        ViolationType::MemoryLeakUnfreed,
                        tok.loc,
                        "[MEMSENTINEL] Memory leak: Pointer '" + symbol + "' allocated on heap is not deallocated before return.",
                        cell->alloc_site,
                        "Original allocation occurred here:"
                    );
                }
            }
            continue;
        }

        // 1. Detect dynamic allocation: malloc, calloc, new, new[]
        // Pattern: [type] symbol = malloc(...) or symbol = (type*)malloc(...)
        if (tok.text == "malloc" || tok.text == "calloc" || tok.text == "aligned_alloc") {
            // Find assigned symbol by looking backwards across '='
            if (i >= 2) {
                for (int back = static_cast<int>(i) - 1; back >= 0; --back) {
                    if (tokens[back].text == "=" && back > 0) {
                        std::string target_sym = tokens[back - 1].text;
                        if (target_sym == "*") {
                            // int* ptr; *ptr = malloc? No, int *ptr = malloc
                            if (back >= 2) target_sym = tokens[back - 2].text;
                        }
                        state_.register_allocation(target_sym, tok.loc, 64, false);
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
                        state_.register_allocation(target_sym, tok.loc, 64, is_array);
                        break;
                    }
                    if (tokens[back].text == ";" || tokens[back].text == "{") {
                        break;
                    }
                }
            }
            continue;
        }

        // 2. Detect pointer assignment / alias: q = p;
        if (tok.text == "=" && i > 0 && i + 1 < num_tokens) {
            std::string lhs = tokens[i - 1].text;
            std::string rhs = tokens[i + 1].text;
            if (state_.has_symbol(rhs)) {
                state_.register_alias(lhs, rhs, 0);
            }
        }

        // 3. Pointer arithmetic tracking with architecture-adaptive word stride
        const int64_t arch_stride = static_cast<int64_t>(sizeof(void*));
        if (tok.text == "++" && i > 0 && state_.has_symbol(tokens[i - 1].text)) {
            state_.apply_pointer_arithmetic(tokens[i - 1].text, arch_stride);
        } else if (tok.text == "--" && i > 0 && state_.has_symbol(tokens[i - 1].text)) {
            state_.apply_pointer_arithmetic(tokens[i - 1].text, -arch_stride);
        } else if (tok.text == "+=" && i > 0 && state_.has_symbol(tokens[i - 1].text)) {
            int64_t val = arch_stride;
            if (i + 1 < num_tokens) {
                try { val = std::stoll(tokens[i + 1].text); } catch (...) {}
            }
            state_.apply_pointer_arithmetic(tokens[i - 1].text, val);
        } else if (tok.text == "-=" && i > 0 && state_.has_symbol(tokens[i - 1].text)) {
            int64_t val = arch_stride;
            if (i + 1 < num_tokens) {
                try { val = std::stoll(tokens[i + 1].text); } catch (...) {}
            }
            state_.apply_pointer_arithmetic(tokens[i - 1].text, -val);
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
                        "[MEMSENTINEL] Double free detected on pointer '" + target_sym + "'.",
                        cell ? cell->free_site : SourceLocation{},
                        "Previous deallocation occurred here:"
                    );
                } else if (*err == ViolationType::ModifiedBaseFree) {
                    record_violation(
                        ViolationType::ModifiedBaseFree,
                        tok.loc,
                        "[MEMSENTINEL] Attempt to free altered/interior pointer '" + target_sym + "'. Address was shifted by " +
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
                            "[MEMSENTINEL] Double free / delete detected on pointer '" + target_sym + "'.",
                            cell ? cell->free_site : SourceLocation{},
                            "Pointer was previously deleted here:"
                        );
                    } else if (*err == ViolationType::ModifiedBaseFree) {
                        record_violation(
                            ViolationType::ModifiedBaseFree,
                            tok.loc,
                            "[MEMSENTINEL] Attempt to delete pointer '" + target_sym + "' with modified base offset (offset = " +
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
                    "[MEMSENTINEL] Use-After-Free: Pointer '" + tokens[i + 1].text + "' dereferenced after deallocation.",
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
                    "[MEMSENTINEL] Use-After-Free: Accessing member on freed pointer '" + tokens[i - 1].text + "'.",
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
                    "[MEMSENTINEL] Use-After-Free: Array subscript index on freed pointer '" + tokens[i - 1].text + "'.",
                    cell ? cell->free_site : SourceLocation{},
                    "Pointer was deallocated here:"
                );
            }
        }
    }

    return !has_errors();
}

void Analyzer::print_diagnostics(std::ostream& os) const {
    for (const auto& diag : diagnostics_) {
        // GCC standard format: file:line:col: error: ...
        os << diag.error_loc.filename << ":" << diag.error_loc.line << ":" << diag.error_loc.column
           << ": error: " << diag.primary_message << "\n";
        if (!diag.note_message.empty() && !diag.note_loc.filename.empty()) {
            os << diag.note_loc.filename << ":" << diag.note_loc.line << ":" << diag.note_loc.column
               << ": note: " << diag.note_message << "\n";
        }
    }
    if (!diagnostics_.empty()) {
        os << "compilation terminated due to memory safety violations.\n";
    }
}

} // namespace memcheck::core
