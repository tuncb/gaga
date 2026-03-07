#include "note.hpp"

#include <array>
#include <cmath>

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
    "C-", "C#", "D-", "D#", "E-", "F-", "F#", "G-", "G#", "A-", "A#", "B-",
};

}  // namespace

tl::expected<ParsedNote, NoteParseError> decode_note(char letter, char accidental, char octave_char) {
    if (octave_char < '0' || octave_char > '9') {
        return tl::unexpected(NoteParseError::InvalidToken);
    }

    const int octave = octave_char - '0';
    int pitch_index = -1;
    PitchClass pitch = PitchClass::C;

    for (int index = 0; index < static_cast<int>(kPitchMappings.size()); ++index) {
        const auto& mapping = kPitchMappings[index];
        if (mapping.letter == letter && mapping.accidental == accidental) {
            pitch_index = index;
            pitch = mapping.pitch;
            break;
        }
    }

    if (pitch_index < 0) {
        return tl::unexpected(NoteParseError::InvalidToken);
    }

    const int note_index = octave * 12 + pitch_index;
    if (note_index < 0 || note_index > 119) {
        return tl::unexpected(NoteParseError::NoteOutOfRange);
    }

    return ParsedNote{Note{pitch, octave}, static_cast<uint8_t>(note_index)};
}

float note_index_to_frequency(uint8_t note_index) {
    const int midi = static_cast<int>(note_index) + 12;
    return 440.0f * std::pow(2.0f, static_cast<float>(midi - 69) / 12.0f);
}

std::string note_index_to_string(uint8_t note_index) {
    const int octave = note_index / 12;
    const int pitch = note_index % 12;
    return std::string(kPitchNames[static_cast<size_t>(pitch)]) + static_cast<char>('0' + octave);
}

}  // namespace gaga
