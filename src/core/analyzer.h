#ifndef SAFE_C_CORE_ANALYZER_H
#define SAFE_C_CORE_ANALYZER_H

#include "pointer_state.h"
#include <string>
#include <vector>
#include <string_view>
#include <iostream>
#include <unordered_map>

namespace safe_c::core {

enum class ColorChoice {
    Auto,
    Always,
    Never
};

struct Diagnostic {
    ViolationType type;
    SourceLocation error_loc;
    std::string primary_message;
    SourceLocation note_loc;
    std::string note_message;
};

class Analyzer {
public:
    explicit Analyzer(Mode mode = Mode::FatalError);
    ~Analyzer() = default;

    void set_mode(Mode mode) noexcept { mode_ = mode; }
    [[nodiscard]] Mode mode() const noexcept { return mode_; }

    void set_color_mode(ColorChoice choice) noexcept { color_mode_ = choice; }
    [[nodiscard]] ColorChoice color_mode() const noexcept { return color_mode_; }

    // Analyze C/C++ source code directly from string or file
    bool analyze_source(std::string_view source_code, std::string_view filename);
    bool analyze_file(const std::string& filepath);

    [[nodiscard]] const std::vector<Diagnostic>& diagnostics() const noexcept {
        return diagnostics_;
    }

    [[nodiscard]] bool has_errors() const noexcept {
        return !diagnostics_.empty();
    }

    // Print all diagnostics to output stream (stderr by default)
    void print_diagnostics(std::ostream& os = std::cerr) const;

    void clear() {
        diagnostics_.clear();
        state_ = StateTable();
        source_line_cache_.clear();
    }

private:
    void record_violation(
        ViolationType type,
        SourceLocation err_loc,
        std::string primary_msg,
        SourceLocation note_loc = {},
        std::string note_msg = {}
    );

    // Scanner / Parser helpers
    void process_token_stream(const std::vector<std::string>& tokens,
                             const std::vector<SourceLocation>& locations);

    Mode mode_{Mode::FatalError};
    ColorChoice color_mode_{ColorChoice::Auto};
    StateTable state_;
    std::vector<Diagnostic> diagnostics_;
    std::unordered_map<std::string, std::vector<std::string>> source_line_cache_;
};

} // namespace safe_c::core

namespace safe_cpp::core {
    using namespace safe_c::core;
}

namespace memcheck::core {
    using namespace safe_c::core;
}

#endif // SAFE_C_CORE_ANALYZER_H
