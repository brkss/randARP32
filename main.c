#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>


#define SAMPLE_RATE 44100
#define BITS_PER_SAMPLE 16
#define NUM_CHANNELS 1

#define NOTE_DURATION 0.5
#define PI 3.14159265358979323846


double midiToFrequency(int midiNote) {
    return 440.0 * pow(2.0, (midiNote - 69) / 12.0);
}

short generateSample(double frequency, double time){
    double ampl = 30000;
    return (short)(ampl * sin(2 * PI * frequency * time));
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
    // Define a simple chord 
    int chord[] = {60, 64, 67, 72};
    int chordSize = sizeof(chord) / sizeof(chord[0]);


    int userSeed, sequenceLength;
    printf("Enter a seed number: ");
    scanf("%d", &userSeed);
    printf("Enter desired sequence length (number of notes): ");
    scanf("%d", &sequenceLength);

    srand(userSeed); // Seed random number generator

    // Calculate total samples: note duration * sample rate * sequence length
    int samplesPerNote = (int)(NOTE_DURATION * SAMPLE_RATE);
    int totalSamples = samplesPerNote * sequenceLength;

    FILE *f = fopen("arpeggio.wav", "wb");
    if(!f){
        perror("could not open file for writing!");
        return 1;
    }

    // Write the WAV header (we will fill in the header with correct sizes)
    writeWavHeader(f, totalSamples);

    // Generate and write samples for each note
    for (int n = 0; n < sequenceLength; n++) {
        int midiNote = chord[rand() % chordSize];
        double freq = midiToFrequency(midiNote);
        printf("Note %d: MIDI %d -> Frequency %.2f Hz\n", n+1, midiNote, freq);
        for (int i = 0; i < samplesPerNote; i++) {
            double t = (double)i / SAMPLE_RATE;
            short sample = generateSample(freq, t);
            fwrite(&sample, sizeof(short), 1, f);
        }
    }
    fclose(f);
    printf("WAV file 'arpeggio.wav' generated successfully.\n");

    return 0;
}