The next manageable group is SED, CHA, and NTH. 
These are still in scope, but they need a real track-state layer for deterministic RNG and loop-pass counting. 
The main problems are semantic, not structural: whether CHA gates the whole row or only the note event, 
how NTH counts passes in loop mode, and whether reloads reset or preserve seed/counter state.

The rest of the “good candidate” list is not actually easy anymore: OFF XX, KIL, DEL, RET, MTT, PSL, PBN, PVB, PVX, ARP, and ARC. 
The problem is that the engine still only knows row boundaries and one fixed frames_per_row; it has no sub-row event queue, no tick scheduler, and no continuous modulation layer in src/audio.cpp, src/transport.cpp, or src/synth.cpp. 
Until that exists, those commands will be awkward, fragile, or only partially correct.
