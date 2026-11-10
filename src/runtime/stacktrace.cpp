#include "stacktrace.h"
#include <sstream>
#include <iomanip>
#include <cstdint>

#if defined(_WIN32)
  #include <windows.h>
  #include <dbghelp.h>
#else
  #include <execinfo.h>
  #include <dlfcn.h>
#endif

namespace safe_c::rt {

std::vector<StackFrame> capture_stacktrace(size_t max_frames, size_t skip_frames) {
    std::vector<StackFrame> result;

#if defined(_WIN32)
    std::vector<void*> backtrace(max_frames);
    USHORT captured = CaptureStackBackTrace(
        static_cast<DWORD>(skip_frames),
        static_cast<DWORD>(max_frames),
        backtrace.data(),
        nullptr
    );

    result.reserve(captured);
    for (USHORT i = 0; i < captured; ++i) {
        StackFrame frame;
        frame.address = backtrace[i];
        result.push_back(std::move(frame));
    }
#else
    std::vector<void*> backtrace(max_frames);
    int captured = ::backtrace(backtrace.data(), static_cast<int>(max_frames));

    if (captured > static_cast<int>(skip_frames)) {
        char** symbols = ::backtrace_symbols(backtrace.data(), captured);
        for (int i = static_cast<int>(skip_frames); i < captured; ++i) {
            StackFrame frame;
            frame.address = backtrace[i];
            if (symbols && symbols[i]) {
                frame.function_name = symbols[i];
            }
            result.push_back(std::move(frame));
        }
        if (symbols) {
            std::free(symbols);
        }
    }
#endif

    return result;
}

std::string format_stacktrace(const std::vector<StackFrame>& frames) {
    std::ostringstream ss;
    for (size_t i = 0; i < frames.size(); ++i) {
        ss << "  [" << std::setw(2) << i << "] 0x"
           << std::hex << reinterpret_cast<uintptr_t>(frames[i].address) << std::dec;
        if (!frames[i].function_name.empty()) {
            ss << " " << frames[i].function_name;
        }
        ss << "\n";
    }
    return ss.str();
}

} // namespace safe_c::rt
