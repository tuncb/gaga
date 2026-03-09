) Core phrase / table tracker FX commands

These are the main sequencer-side commands from the manual’s Sequencer FX Commands section. TIC modes come from the Table View section, and MTT is a newer command added later in firmware 6.5.0.

Command	Brief definition
ARP XY	Fast 3-note arpeggio from the current note; X and Y are interval offsets.
ARC XY	Configures the ARP mode/pattern and speed.
CHA XY	Probability gate; in phrases X/Y split the chance for left/right of the command, in tables it sets chance for everything to the left.
DEL XX	Delays the phrase row; in tables it delays the table playhead.
GRV XX	Sets the groove for the current track until changed.
GGR XX	Sets the groove for all tracks.
HOP XY / HOPFF	In phrases, jump to row Y in the next phrase; HOPFF stops the current track. In tables, jump to row Y for X repeats.
INS XX	Changes or triggers the instrument from an FX slot.
KIL XX	Hard-stops the current note after XX ticks.
RND XY	Randomizes the previously active FX command by a chosen amount/range.
RNL XY	Randomizes the command to the left; in the first phrase column it can randomize note+instrument, and in tables note+velocity.
RET XY	Retriggers the row; can ramp volume on each retrig, or act like a single delayed retrig when Y=0.
REP XX	Repeats the last FX command and increments its value over time until stopped.
RTO XX	Sets the limit where REP stops.
RMX XY	Repositions playheads of selected tracks to the left.
NTH XY	Loop-count condition; skips left/right content depending on which pass through the phrase/table you are on.
PSL XX	Portamento / pitch-slide time in ticks.
PBN XX	Continuous pitch bend up or down.
PVB XY	Vibrato; X = speed, Y = depth.
PVX XY	More extreme vibrato.
SCA XY	Sets the track key and scale.
SCG XY	Sets the global key and scale.
SNG XX	Moves the track to a relative song row, if valid.
SED XX	Sets the random seed for the current track.
TBL XX	Selects the instrument table.
THO XX	Jumps table playback to a table row.
TIC XX	Sets table tick rate or mode: per-trigger (00), fixed tick count (01–FB), octave map (FC), velocity map (FD), note map (FE), or 200 Hz (FF).
TBX XX	Runs an auxiliary table in parallel; 00 stops it.
TPO XX	Sets song tempo.
TSP XX	Sets global song transpose.
NXT XX	Triggers instrument XX on the track to the right using the current note/velocity.
OFF XX	Note-off after XX ticks; if ADSR is present, it triggers release.
MTT XX*	Micro-timing nudge earlier or later at 1/8-tick resolution.

*MTT was added after the Oct 2025 manual, in firmware 6.5.0.

2) Mixer / send-effect / input automation commands

These are the tracker commands for automating the mixer, ModFX, delay, reverb, DJ filter, and audio inputs directly from phrase/table FX lanes.

Command	Brief definition
EQM XX	Selects the EQ slot for the main mix.
EQI XX	Selects the EQ slot for the current instrument.
VMV XX	Sets main song volume.
XMT XX	Sets ModFX type and phase position.
XMM XX	Sets ModFX depth.
XMF XX	Sets ModFX rate/frequency.
XMW XX	Sets ModFX stereo width.
XMR XX	Sets ModFX send to reverb.
XDT XY	Sets left/right delay times.
XDF XX	Sets delay feedback.
XDW XX	Sets delay stereo width.
XDR XX	Sets delay send to reverb.
XRS XX	Sets reverb room size.
XRD XX	Sets reverb decay.
XRM XX	Sets reverb modulation depth.
XRF XX	Sets reverb modulation rate/frequency.
XRW XX	Sets reverb stereo width.
XRZ XX	Enables/disables reverb freeze.
VMX XX	Sets ModFX return volume.
VDE XX	Sets delay return volume.
VRE XX	Sets reverb return volume.
VT1–VT8 XX	Sets track volume.
DJC XX	Sets DJ filter cutoff.
DJR XX	Sets DJ filter resonance.
DJT XX	Sets DJ filter type.
IVO XX	Sets analog input volume.
IMX XX	Sets analog input ModFX send.
IDE XX	Sets analog input delay send.
IRV XX	Sets analog input reverb send.
IV2 XX	Sets second input volume in dual-mono mode.
IM2 XX	Sets second input ModFX send in dual-mono mode.
ID2 XX	Sets second input delay send in dual-mono mode.
IR2 XX	Sets second input reverb send in dual-mono mode.
USB XX	Sets USB input volume.
3) Common instrument FX commands

The manual says almost all instrument parameters have FX commands; this is just the common subset listed in the appendix.

Command	Brief definition
VOL XX	Offsets instrument volume.
PIT XX	Offsets pitch in semitones.
FIN XX	Fine-tunes pitch within about ±1 semitone.
EA1 / EA2	Offsets envelope amount.
AT1 / AT2	Offsets envelope attack.
HO1 / HO2	Offsets envelope hold.
DE1 / DE2	Offsets envelope decay.
ET1 / ET2	Retriggers the envelope when value is greater than 00.
LA1 / LA2	Offsets LFO amount.
LF1 / LF2	Offsets LFO frequency.
LT1 / LT2	Retriggers the LFO; value sets phase/start position.

This covers the generic tracker-side command set you’ll use most in phrases and tables. The next useful pass would be the engine-specific commands for Sampler, MIDI Out, Wavsynth, and FM.