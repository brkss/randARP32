#include "../includes/reverb.h"




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