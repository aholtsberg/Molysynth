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
             
                         Monophonic guitar synth

                         © Anders Holtsberg, 2025

\*****************************************************************************/

// Pitch tracker
#define MOLY_TRIGLEVEL   't' // Default 0.08
#define MOLY_COMPLEVEL   'c' // Default 0.0

// Mini synth
#define MOLY_DRYVOLUME   'd' // Default 0.0
#define MOLY_WETVOLUME   'w' // Default 1.0

// For use off-line
#define MOLY_VERBOSE     'v' // off-line only

// After analyzing what is going on (I recommend 100 times per second), we
// receive the result in form of a message.
//
// NOTE 1: The wavelength lambda is guaranteed to be non-zero if the volume
//   is non-zero, ie the tracker itself continues the last tone if it can not 
//   figure out the frequency at the moment.
//
// NOTE 2: There is no TRIG_OFF message, silence is only marked with volume 0.0,
//   and TRIG can arrive without silence in between.
//
// NOTE 3: The receiver (the synth) may write type 0 in the message on order to
//   remember that the message has already been read.
//
// NOTE 4: The whole hoopla here is for making it very easy for you to replace 
//   the synth with your synth, or harmonizer or adaptive filter or whatever.
//
// NOTE 5: The volume can be compressed The raw volume is also in the message 
//   just in case someone wants it besides the compressed volume.

#define MOLY_MTYPE_CONTINUE 1 // No trig
#define MOLY_MTYPE_TRIG 2 // Trig, a new tone starts

struct moly_message {
   int type;
   float lambda; // Wavelength in number of samples
   float volume; // This is the compressed volume
   float volume_raw; // This is the original volume
};

// The sample frequency is not hardcoded. This means that our code can 
// run from WAV files (44.1 kHz) as well as DSP (48 kHz, 32 kHz). A hardcoded
// filter in the tracker expects it to be somewhere in that range.
int moly_init(uint32_t sampleFrequency);
void moly_addtobuf(const float *in, size_t bsz);
struct moly_message *moly_analyze(void);

// The mini synth
void moly_synth(const float *in, float *out, size_t bsz);
void moly_synth_message(struct moly_message *m);

// Any time we can change the settings
void moly_set(char opt, float val);

#endif
