#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#include "includes/audio.h"
#include "includes/utils.h"
#include "includes/bjorklund.h"
#include "includes/reverb.h"










int main() {
    // Initialize reverb
    initReverb();
    
    // Define a simple chord - C major arpeggio
    int chord[] = {60, 64, 67, 72, 76, 79, 84, 88};  // C4, E4, G4, C5, E5, G5, C6, E6
    int chordSize = sizeof(chord) / sizeof(chord[0]);
    printf("Chord size: %d\n", chordSize);
    printf("Chord notes: ");
    for(int i = 0; i < chordSize; i++) {
        printf("%d ", chord[i]);
    }
    printf("\n");

    // Euclidean rhythm parameters
    int steps = 16;  // Total steps in the pattern
    int pulses = 7;  // Number of active beats
    int pattern[16] = {0};  // Will hold our Euclidean pattern

    printf("Chord notes: ");
    for(int i = 0; i < chordSize; i++) {
        printf("%d ", chord[i]);
    }
    printf("\n");
    bjorklund_euclid(steps, pulses, pattern);

    
    printf("Euclidean Pattern (%d steps, %d pulses): ", steps, pulses);
    for (int i = 0; i < steps; i++) {
        printf("%d", pattern[i]);
    }
    printf("\n");

    int sequenceLength = 100;
    int userSeed = 54654;
    srand(userSeed);

    // Calculate total samples based on BPM and sequence length
    int samplesPerNote = (int)(NOTE_DURATION * SAMPLE_RATE);
    int totalSamples = samplesPerNote * sequenceLength * steps;  // Multiply by steps for full pattern

    printf("BPM: %.1f\n", BPM);
    printf("Note Duration: %.3f seconds\n", NOTE_DURATION);
    printf("Samples per note: %d\n", samplesPerNote);

    FILE *f = fopen("arpeggio.wav", "wb");
    if(!f){
        perror("could not open file for writing!");
        return 1;
    }

    

    // Write the WAV header
    writeWavHeader(f, totalSamples);

    // Generate and write samples for each note
    int currentNote = 0;
    int patternStep = 0;
    double globalTime = 0.0;  // Keep track of global time for continuous waveform
    int totalSteps = sequenceLength * steps;
    double lastFreq = midiToFrequency(chord[0]);  // Keep track of last frequency for smooth transitions


    for (int n = 0; n < totalSteps; n++) {
        // Only play note if current step in pattern is 1
        if (pattern[patternStep]) {
            int midiNote = chord[currentNote];
            double freq = midiToFrequency(midiNote);
            lastFreq = freq;  // Store current frequency
            print_progress(n, totalSteps);
            
            
            for (int i = 0; i < samplesPerNote; i++) {
                double timeWithinNote = (double)i / SAMPLE_RATE;
                short sample = generateSample(freq, globalTime, timeWithinNote);
                sample = applyReverb(sample);
                fwrite(&sample, sizeof(short), 1, f);
                globalTime += 1.0 / SAMPLE_RATE;
            }
            currentNote = (currentNote + 1) % chordSize;
        } else {
            // For silent steps, maintain the wave phase using the last frequency
            for (int i = 0; i < samplesPerNote; i++) {
                double timeWithinNote = (double)i / SAMPLE_RATE;
                // Generate a very quiet sample to maintain phase
                short sample = generateSample(lastFreq, globalTime, timeWithinNote) / 1000;
                sample = applyReverb(sample);
                fwrite(&sample, sizeof(short), 1, f);
                globalTime += 1.0 / SAMPLE_RATE;
            }
        }

        // Update pattern step
        patternStep = (patternStep + 1) % steps;
    }

    fclose(f);
    printf("\nWAV file 'arpeggio.wav' generated successfully.\n");

    return 0;
}