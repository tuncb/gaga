#include "note.hpp"

#include <array>
#include <cmath>
#include <cctype>

namespace gaga {

namespace {

struct PitchMapping {
    char letter;
    char accidental;
    PitchClass pitch;
};

constexpr std::array<PitchMapping, 12> kPitchMappings{{
    {'C', '-', PitchClass::C},
    {'C', '#', PitchClass::Cs},
    {'D', '-', PitchClass::D},
    {'D', '#', PitchClass::Ds},
    {'E', '-', PitchClass::E},
    {'F', '-', PitchClass::F},
    {'F', '#', PitchClass::Fs},
    {'G', '-', PitchClass::G},
    {'G', '#', PitchClass::Gs},
    {'A', '-', PitchClass::A},
    {'A', '#', PitchClass::As},
    {'B', '-', PitchClass::B},
}};

constexpr std::array<const char*, 12> kPitchNames{
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B",
};

char ascii_upper(char value) {
    if (value >= 'a' && value <= 'z') {
        return static_cast<char>(value - 'a' + 'A');
    }
    return value;
}

tl::expected<int, NoteParseError> parse_pitch_index(char letter, bool sharp) {
    switch (ascii_upper(letter)) {
    case 'C':
        return sharp ? 1 : 0;
    case 'D':
        return sharp ? 3 : 2;
    case 'E':
        if (sharp) {
            return tl::unexpected(NoteParseError::InvalidToken);
        }
        return 4;
    case 'F':
        return sharp ? 6 : 5;
    case 'G':
        return sharp ? 8 : 7;
    case 'A':
        return sharp ? 10 : 9;
    case 'B':
        if (sharp) {
            return tl::unexpected(NoteParseError::InvalidToken);
        }
        return 11;
    default:
        return tl::unexpected(NoteParseError::InvalidToken);
    }
}

}  // namespace

tl::expected<ParsedNote, NoteParseError> parse_scientific_pitch(std::string_view text) {
    if (text.size() < 2) {
        return tl::unexpected(NoteParseError::InvalidToken);
    }

    const char letter = ascii_upper(text[0]);
    bool sharp = false;
    size_t index = 1;
    if (index < text.size() && text[index] == '#') {
        sharp = true;
        ++index;
    }

    if (index >= text.size()) {
        return tl::unexpected(NoteParseError::InvalidToken);
    }

    bool negative = false;
    if (text[index] == '-') {
        negative = true;
        ++index;
    }
    if (index >= text.size()) {
        return tl::unexpected(NoteParseError::InvalidToken);
    }

    int octave = 0;
    for (; index < text.size(); ++index) {
        const char current = text[index];
        if (std::isdigit(static_cast<unsigned char>(current)) == 0) {
            return tl::unexpected(NoteParseError::InvalidToken);
        }
        octave = (octave * 10) + (current - '0');
    }
    if (negative) {
        octave = -octave;
    }

    const auto pitch_index = parse_pitch_index(letter, sharp);
    if (!pitch_index) {
        return tl::unexpected(pitch_index.error());
    }

    const int midi_note = ((octave + 1) * 12) + pitch_index.value();
    if (midi_note < 0 || midi_note > 127) {
        return tl::unexpected(NoteParseError::NoteOutOfRange);
    }

    const auto& mapping = kPitchMappings[static_cast<size_t>(pitch_index.value())];
    return ParsedNote{Note{mapping.pitch, octave}, static_cast<uint8_t>(midi_note)};
}

float midi_note_to_frequency(uint8_t midi_note) {
    return 440.0f * std::pow(2.0f, static_cast<float>(static_cast<int>(midi_note) - 69) / 12.0f);
}

std::string midi_note_to_string(uint8_t midi_note) {
    const int octave = static_cast<int>(midi_note) / 12 - 1;
    const int pitch = static_cast<int>(midi_note) % 12;
    return std::string(kPitchNames[static_cast<size_t>(pitch)]) + std::to_string(octave);
}

}  // namespace gaga
