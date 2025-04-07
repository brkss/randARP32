#include "../includes/audio.h"


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

double midiToFrequency(int midiNote) {
    return 440.0 * pow(2.0, (midiNote - 69) / 12.0);
}