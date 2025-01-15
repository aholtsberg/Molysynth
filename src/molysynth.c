#include "molysynth.h"


//================================================================= GLOBALS ===


struct {

   // Settings
   struct {
      int waveform;
      int autotune;
      int volsens;
      int verbose;
      int trigger_level;
   } settings;

   // Synth
   struct {
      float lambda;
      float phi;
      float volume;
   } synth;

   // Decimation filter state
   struct {
      float a1;
      float a2;
   } decfilt;

   // Message from tracker
   struct {
      int state; // 1: new values, 2: trigger, 0: already processed
      float lambda;
      float volume;
   } message;

   // Tracker state
   struct {
   } tracker;

} g;


//============================================================= SYNTHESIZER ===


static inline void synthesizer(const int16_t *in, int16_t *out, size_t size) {
   
   // Read message
   if (g.message.state != 0) {
      g.synth.lambda = g.message.lambda;
      g.message.state = 0;
   }

   // Silence
   if (g.synth.lambda == 0.0) {
      for (int i = 0; i < size; i++) {
         out[i] = 0;
      }
      g.synth.lambda = 0.0;
      g.synth.phi = 0.0;
      g.synth.volume = 0.0;
      return;
   }

   // Run
   float phidelta = 1.0 / g.synth.lambda;
   int16_t vol = 5000;
   if (g.settings.volsens) {
      vol = g.message.volume;
   }
   for (int i = 0; i < size; i++) {
       float phi = g.synth.phi + phidelta;
       int16_t x;
       if (phi < 0.5) {
          x = vol;
       } else if (phi >= 1.0) {
          x = vol * ((phi - 1.0) - (1.0 - g.synth.phi)) / phidelta;
          phi -= 1.0;
       } else if (g.synth.phi < 0.5) {
          x = vol * ((0.5 - g.synth.phi) - (phi - 0.5)) / phidelta;
       } else {
          x = -vol;
       }
       out[i] = x;
       g.synth.phi = phi;
   }
}


//================================================================ EXPORTED ===


int moly_init(uint32_t sampleFrequency) {
   return 0;
}


void moly_callback(const int16_t *in, int16_t *out, size_t size) {
   synthesizer(in, out, size);
}


void moly_analyze(void) {
   g.message.state = 2;
   if (!g.message.lambda) {
      g.message.lambda = 400;
   } else {
      g.message.lambda--;
   }
   g.message.volume = 6000;
}


void moly_set(char opt, int val) {
   if (opt == 'w') g.settings.waveform = val;
   if (opt == 'a') g.settings.autotune = val;
   if (opt == 'y') g.settings.volsens = val;
   if (opt == 'v') g.settings.verbose = val;
   if (opt == 'r') g.settings.trigger_level = 1 << (6 + val);
}

