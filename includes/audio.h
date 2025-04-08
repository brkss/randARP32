#ifndef AUDIO_H
#define AUDIO_H

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#define SAMPLE_RATE 44100
#define BITS_PER_SAMPLE 16
#define NUM_CHANNELS 1

// BPM and timing controls
#define BPM 90.0  // Faster tempo
#define BEATS_PER_NOTE 0.25  // 1/4 = quarter note, 0.25 = sixteenth note
#define NOTE_DURATION ((60.0 / BPM) * BEATS_PER_NOTE)  // Duration of each note in seconds

#define PI 3.14159265358979323846

// Envelope parameters (in seconds)
#define ATTACK_TIME (NOTE_DURATION * 0.05)   // Faster attack
#define DECAY_TIME (NOTE_DURATION * 0.3)    // Longer decay
#define SUSTAIN_LEVEL 0.8                   // Higher sustain
#define RELEASE_TIME (NOTE_DURATION * 0.2)  // 20% of note duration


// functions prototypes
void writeWavHeader(FILE *f, int numSamples);
short generateSample(double frequency, double time, double timeWithinNote);
double getEnvelopeAmplitude(double timeWithinNote);
double midiToFrequency(int midiNote);

#endif
