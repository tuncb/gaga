#pragma once

#include <cstdint>
#include <string>

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
    uint8_t note_index;
};

tl::expected<ParsedNote, NoteParseError> decode_note(char letter, char accidental, char octave_char);
float note_index_to_frequency(uint8_t note_index);
std::string note_index_to_string(uint8_t note_index);

}  // namespace gaga
