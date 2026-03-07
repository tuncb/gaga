#include "terminal_display.hpp"

#include <cstdio>
#include <ostream>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#endif

namespace gaga {

namespace {

bool enable_virtual_terminal_processing() {
#ifdef _WIN32
    if (_isatty(_fileno(stdout)) == 0) {
        return false;
    }

    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle == nullptr || handle == INVALID_HANDLE_VALUE) {
        return false;
    }

    DWORD mode = 0;
    if (GetConsoleMode(handle, &mode) == 0) {
        return false;
    }

    const DWORD requested_mode = mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if ((mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING) == 0 &&
        SetConsoleMode(handle, requested_mode) == 0) {
        return false;
    }

    return true;
#else
    return isatty(STDOUT_FILENO) != 0;
#endif
}

}  // namespace

TerminalDisplay::~TerminalDisplay() {
    shutdown();
}

bool TerminalDisplay::initialize(std::ostream& out) {
    if (enabled_) {
        return true;
    }

    if (!enable_virtual_terminal_processing()) {
        return false;
    }

    out_ = &out;
    enabled_ = true;
    *out_ << "\x1b[?25l";
    out_->flush();
    return true;
}

void TerminalDisplay::render(const PatternSnapshot& snapshot, uint32_t active_row, bool force_redraw) {
    if (!enabled_ || out_ == nullptr) {
        return;
    }

    if (!force_redraw &&
        rendered_generation_ == snapshot.display_generation &&
        rendered_row_ == active_row) {
        return;
    }

    uint32_t active_source_line = 0;
    if (active_row != kNoDisplayedRow && active_row < snapshot.pattern.source_line.size()) {
        active_source_line = snapshot.pattern.source_line[active_row];
    }

    std::string buffer;
    if (first_frame_) {
        buffer += "\x1b[2J";
    }
    buffer += "\x1b[H";

    for (size_t line_index = 0; line_index < snapshot.source_lines.size(); ++line_index) {
        if (active_source_line == line_index + 1U) {
            buffer += "\x1b[32m";
            buffer += snapshot.source_lines[line_index];
            buffer += "\x1b[0m";
        } else {
            buffer += snapshot.source_lines[line_index];
        }
        buffer += '\n';
    }

    buffer += "\x1b[0m\x1b[J";

    *out_ << buffer;
    out_->flush();

    first_frame_ = false;
    rendered_generation_ = snapshot.display_generation;
    rendered_row_ = active_row;
}

void TerminalDisplay::shutdown() {
    if (!enabled_ || out_ == nullptr) {
        return;
    }

    *out_ << "\x1b[0m\x1b[?25h";
    if (!first_frame_) {
        *out_ << "\n";
    }
    out_->flush();

    out_ = nullptr;
    enabled_ = false;
}

}  // namespace gaga
