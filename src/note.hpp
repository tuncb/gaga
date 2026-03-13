#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include <tl/expected.hpp>

namespace gaga {

enum class PitchClass : uint8_t {
    C,
    Cs,
    D,
    Ds,
    E,
    F,
    Fs,
    G,
    Gs,
    A,
    As,
    B
};

struct Note {
    PitchClass pitch;
    int octave;
};

enum class NoteParseError : uint8_t {
    InvalidToken,
    NoteOutOfRange
};

struct ParsedNote {
    Note note;
    uint8_t midi_note;
};

tl::expected<ParsedNote, NoteParseError> parse_scientific_pitch(std::string_view text);
float midi_note_to_frequency(uint8_t midi_note);
std::string midi_note_to_string(uint8_t midi_note);

}  // namespace gaga
