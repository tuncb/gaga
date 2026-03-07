#pragma once

#include <cstdint>
#include <iosfwd>

#include "audio.hpp"
#include "pattern.hpp"

namespace gaga {

class TerminalDisplay {
public:
    TerminalDisplay() = default;
    ~TerminalDisplay();

    TerminalDisplay(const TerminalDisplay&) = delete;
    TerminalDisplay& operator=(const TerminalDisplay&) = delete;

    bool initialize(std::ostream& out);
    void render(const PatternSnapshot& snapshot, uint32_t active_row, bool force_redraw = false);
    void shutdown();

    [[nodiscard]] bool enabled() const { return enabled_; }

private:
    std::ostream* out_ = nullptr;
    bool enabled_ = false;
    bool first_frame_ = true;
    uint32_t rendered_generation_ = 0;
    uint32_t rendered_row_ = kNoDisplayedRow;
};

}  // namespace gaga
