#ifndef __MOLYSYNTH_H__
#define __MOLYSYNTH_H__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/*****************************************************************************\

                __  ___      __                       __  __  
               /  |/  /___  / /_  _________  ______  / /_/ /_ 
              / /|_/ / __ \/ / / / / ___/ / / / __ \/ __/ __ \
             / /  / / /_/ / / /_/ (__  ) /_/ / / / / /_/ / / /
            /_/  /_/\____/_/\__, /____/\__, /_/ /_/\__/_/ /_/ 
                           /____/     /____/
             
                         Molyphonic guitar synth

                         © Anders Holtsberg, 2024

\*****************************************************************************/

#define MOLY_DRYVOLUME   'y' // 0-999
#define MOLY_WETVOLUME   'e' // 0-999
#define MOLY_SENSITIV    'i' // 0-999
#define MOLY_ATTACK      'a' // 0-999
#define MOLY_DECAY       'd' // 0-999
#define MOLY_SUSTAIN     's' // 0-999
#define MOLY_RELEASE     'r' // 0-999
#define MOLY_WAVEFORM    'w' // 0-2 (square, sawtooth, triangle)
#define MOLY_AUTOTUNE    't' // 0-1 (OFF/AUTO)
#define MOLY_ENVELMIX    'x' // 0-2 (ADSR/ATTACK+INSTRUMENT/INSTRUMENT)
#define MOLY_VERBOSE     'v' // off-line only

// The sample frequency is not hardcoded. This means that our code can 
// run from WAV files (44100 Hz) as well as DSP (48 kHz, 32 kHz, ...)
int moly_init(uint32_t sampleFrequency);

// The sound is arrives with higher priority, hard time constraints.
void moly_callback(const float *in, float *out, size_t bsz);

// The main workload of analyzing is done, well, every now and then.
void moly_analyze(void);

// Any time we can change the settings
void moly_set(char opt, float val);

#endif
