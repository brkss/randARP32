#ifndef REVERB_H
#define REVERB_H

// Reverb parameters
#define REVERB_BUFFER_SIZE 44100  // 1 second delay
#define REVERB_FEEDBACK 0.6       // Less feedback
#define REVERB_MIX 0.2           // Less wet mix

// functions prototypes
void initReverb();
short applyReverb(short input);

#endif
