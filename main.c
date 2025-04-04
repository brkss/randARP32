#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>


#define SAMPLE_RATE 44100
#define BITS_PER_SAMPLE 16
#define NUM_CHANNELS 1

// BPM and timing controls
#define BPM 120.0  // Beats per minute
#define BEATS_PER_NOTE 0.25  // 1/4 = quarter note, 0.25 = sixteenth note
#define NOTE_DURATION ((60.0 / BPM) * BEATS_PER_NOTE)  // Duration of each note in seconds

#define PI 3.14159265358979323846

// Envelope parameters (in seconds)
#define ATTACK_TIME (NOTE_DURATION * 0.1)   // 10% of note duration
#define DECAY_TIME (NOTE_DURATION * 0.2)    // 20% of note duration
#define SUSTAIN_LEVEL 0.7
#define RELEASE_TIME (NOTE_DURATION * 0.2)  // 20% of note duration

// Reverb parameters
#define REVERB_BUFFER_SIZE 44100  // 1 second delay
#define REVERB_FEEDBACK 0.5      // Feedback amount (0.0 to 1.0)
#define REVERB_MIX 0.9         // Wet/dry mix (0.0 to 1.0)

// Reverb buffer
static short reverbBuffer[REVERB_BUFFER_SIZE];
static int reverbIndex = 0;

// Initialize reverb buffer
void initReverb() {
    for (int i = 0; i < REVERB_BUFFER_SIZE; i++) {
        reverbBuffer[i] = 0;
    }
    reverbIndex = 0;
}

// Apply reverb to a sample
short applyReverb(short input) {
    // Get the delayed sample
    short delayed = reverbBuffer[reverbIndex];
    
    // Mix the input with the delayed sample
    short output = (short)((1.0 - REVERB_MIX) * input + REVERB_MIX * delayed);
    
    // Update the delay buffer with feedback
    // Scale down both input and delayed signal before mixing to prevent overflow
    int feedbackSignal = (int)(input * 0.5 + (delayed * REVERB_FEEDBACK * 0.5));
    
    // Clamp the values to prevent distortion
    if (feedbackSignal > 32767) feedbackSignal = 32767;
    if (feedbackSignal < -32767) feedbackSignal = -32767;
    reverbBuffer[reverbIndex] = (short)feedbackSignal;
    
    // Move to next position in the buffer
    reverbIndex = (reverbIndex + 1) % REVERB_BUFFER_SIZE;
    
    return output;
}

double midiToFrequency(int midiNote) {
    return 440.0 * pow(2.0, (midiNote - 69) / 12.0);
}

// Get envelope amplitude for a given time within a note
double getEnvelopeAmplitude(double timeWithinNote) {
    double attackEnd = ATTACK_TIME;
    double decayEnd = attackEnd + DECAY_TIME;
    double sustainEnd = NOTE_DURATION - RELEASE_TIME;
    
    if (timeWithinNote < attackEnd) {
        // Attack phase - linear ramp up
        return (timeWithinNote / ATTACK_TIME);
    } else if (timeWithinNote < decayEnd) {
        // Decay phase - exponential decay to sustain level
        double decayProgress = (timeWithinNote - attackEnd) / DECAY_TIME;
        return 1.0 - (1.0 - SUSTAIN_LEVEL) * decayProgress;
    } else if (timeWithinNote < sustainEnd) {
        // Sustain phase
        return SUSTAIN_LEVEL;
    } else {
        // Release phase - linear ramp down
        double releaseProgress = (timeWithinNote - sustainEnd) / RELEASE_TIME;
        return SUSTAIN_LEVEL * (1.0 - releaseProgress);
    }
}

short generateSample(double frequency, double time, double timeWithinNote) {
    double ampl = 30000;
    double envelope = getEnvelopeAmplitude(timeWithinNote);
    return (short)(ampl * envelope * sin(2 * PI * frequency * time));
}

void writeWavHeader(FILE *f, int numSamples) {
    int byteRate = SAMPLE_RATE * NUM_CHANNELS * BITS_PER_SAMPLE / 8;
    int blockAlign = NUM_CHANNELS * BITS_PER_SAMPLE / 8;
    int subchunk2Size = numSamples * NUM_CHANNELS * BITS_PER_SAMPLE / 8;
    int chunkSize = subchunk2Size + 36;

    // RIFF Header 
    fwrite("RIFF", 1, 4, f);
    fwrite(&chunkSize, 4, 1, f);
    fwrite("WAVE", 1, 4, f);

    // fmt subchunk
    fwrite("fmt ", 1, 4, f);
    int subchunk1Size = 16;
    fwrite(&subchunk1Size, 4, 1, f);
    short audioFormat = 1; // PCM
    fwrite(&audioFormat, 2, 1, f);
    
    short numChannels = NUM_CHANNELS;
    int sampleRate = SAMPLE_RATE;
    short bitsPerSample = BITS_PER_SAMPLE;
    
    fwrite(&numChannels, 2, 1, f);
    fwrite(&sampleRate, 4, 1, f);
    fwrite(&byteRate, 4, 1, f);
    fwrite(&blockAlign, 2, 1, f);
    fwrite(&bitsPerSample, 2, 1, f);

    // data subchunk
    fwrite("data", 1, 4, f);
    fwrite(&subchunk2Size, 4, 1, f);

}

int main() {
    // Initialize reverb
    initReverb();
    
    // Define a simple chord 
    int chord[] = {55, 59, 62, 67, 71, 74, 77, 79, 82, 87};
    int chordSize = sizeof(chord) / sizeof(chord[0]);

    int userSeed = 54654, sequenceLength = 100;
    
    srand(userSeed); // Seed random number generator

    // Calculate total samples based on BPM and sequence length
    int samplesPerNote = (int)(NOTE_DURATION * SAMPLE_RATE);
    int totalSamples = samplesPerNote * sequenceLength;

    printf("BPM: %.1f\n", BPM);
    printf("Note Duration: %.3f seconds\n", NOTE_DURATION);
    printf("Samples per note: %d\n", samplesPerNote);

    FILE *f = fopen("arpeggio.wav", "wb");
    if(!f){
        perror("could not open file for writing!");
        return 1;
    }

    // Write the WAV header (we will fill in the header with correct sizes)
    writeWavHeader(f, totalSamples);

    // Generate and write samples for each note
    int currentNote = 0;
    for (int n = 0; n < sequenceLength; n++) {
        int midiNote = chord[currentNote];
        if(currentNote == chordSize - 1) {
            currentNote = 0;
        } else {
            currentNote++;
        }
        double freq = midiToFrequency(midiNote);
        printf("Note %d: MIDI %d -> Frequency %.2f Hz\n", n+1, midiNote, freq);
        for (int i = 0; i < samplesPerNote; i++) {
            double t = (double)i / SAMPLE_RATE;
            double timeWithinNote = (double)i / SAMPLE_RATE;
            short sample = generateSample(freq, t, timeWithinNote);
            // Apply reverb to the sample
            sample = applyReverb(sample);
            fwrite(&sample, sizeof(short), 1, f);
        }
    }
    fclose(f);
    printf("WAV file 'arpeggio.wav' generated successfully.\n");

    return 0;
}