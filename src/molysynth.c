// Copyright (C) 2025 Anders Holtsberg <anders.holtsberg@gmail.com>
//
// This program is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the
// Free Software Foundation, either version 3 of the License, or (at your
// option) any later version.


#include <math.h>
#include <string.h>
#include "molysynth.h"

#ifdef OFFLINE
#include <stdio.h>
#define P(...) if (g.settings.verbose) printf(__VA_ARGS__)
#else
#define P(...)
#endif


//================================================================= GLOBALS ===


#define LAMBDA_MIN 30
#define LAMBDA_MAX 550
#define MTYPE_NONE 0
#define MTYPE_NEW 1
#define MTYPE_TRIG 2
#define RING_MASK ((1<<16) - 1)
#define SILENCE_LEVEL (0.25 * g.settings.triglevel)
#define ACFD2_MAX 0.5

// Various globals
struct {

   // Settings
   struct {
      float sample_frequency;
      float dryvolume;
      float wetvolume;
      float triglevel;
      float complevel;
      int verbose;
   } settings;

   // Synth
   struct {
      float lambda;
      float phi;
      float vol;
      float vol_delta;
      int vol_count;
      int vol_state;
   } synth;

   // Low-pass filter
   struct {
      float x1;
      float x2;
   } filter;

   // Message from tracker
   struct moly_message message;

   // Ringbuffer
   struct {
      float buf[(1<<16)];
      uint16_t i; // Note: this can index the ringbuffer without MASK!
      size_t time;
   } ring;

} g;


#define ZSIZE 32

// The tracker
struct {
   // Ringbuffer
   uint16_t i;
   uint16_t i_previous;
   size_t time;
   uint16_t xi;
   float xv;
   struct zevent {
      uint16_t i; // zerocrossing index
      uint16_t xi; // extreme value index
      float xv; // extreme value
   } z[ZSIZE];

   // Other stuff
   bool trig;
   bool locked;
   float thismax;
   float prevmax;
   float prevvolume;
   float prevlambda;
   int lambda_raw;
   float lambda_acf;
   float volume;
   float acf_m2;
   float acf_d2;
   int acf_len;
} t;


//============================================================= RING BUFFER ===


inline static float lpfilter(float x) {
   float y = x + 1.8 * g.filter.x1 - 0.82 * g.filter.x2;
   g.filter.x2 = g.filter.x1;
   g.filter.x1 = y;
   return 0.02 * y;
}


// Exported! Filtering is necessary to bring down the number of zero crossings.
void moly_addtobuf(const float *in, size_t size) {
   for (size_t i = 0; i < size; i++) {
      g.ring.buf[g.ring.i++] = lpfilter(in[i]); // uint16_t for ringbuffer :-)
   }
   g.ring.time += size;
}


//============================================================= SYNTHESIZER ===


// We can not change the volume abruptly, that will make Heaviside step funcion
// noise. Therefore we change it slowly.
static inline void synth_volume_set(int type, float volume) {
   if (type == MTYPE_TRIG) {
      g.synth.vol = 0.0;
   }
   g.synth.vol_count = 1 * 48; // 1 ms in the future
   g.synth.vol_delta = (g.settings.wetvolume * volume - g.synth.vol) / g.synth.vol_count;
}


static inline float synth_volume_next(void) {
   if (g.synth.vol_count >= 0) {
      g.synth.vol_count--;
      return g.synth.vol += g.synth.vol_delta;
   } else {
      return g.synth.vol;
   }
}


static inline void synthesizer(float *out, size_t size) {
   
   // Read message
   if (g.message.type != MTYPE_NONE) {
      if (g.message.volume != 0.0 && g.message.lambda != 0.0) {
         g.synth.lambda = g.message.lambda;
      }
      synth_volume_set(g.message.type, g.message.volume);
      g.message.type = MTYPE_NONE;
   }

   // Silence
   if (g.synth.lambda == 0.0) {
      for (size_t i = 0; i < size; i++) {
         out[i] = 0;
      }
      g.synth.phi = 0.0;
      g.synth.vol = 0.0;
      return;
   }

   // Run
   float phidelta = 1.0 / g.synth.lambda;
   for (size_t i = 0; i < size; i++) {
       float phi = g.synth.phi + phidelta;
       float x = 1.0;
       // This hoopla is because we have to interpolate when the square wave
       // jumps. This is likely well known to you folks out there but I
       // discovered it the hard way. If this is not done, then it sounds
       // really bad. 
       if (phi < 0.5) {
          // do nothing
       } else if (phi >= 1.0) {
          x = x * ((phi - 1.0) - (1.0 - g.synth.phi)) / phidelta;
          phi -= 1.0;
       } else if (g.synth.phi < 0.5) {
          x = x * ((0.5 - g.synth.phi) - (phi - 0.5)) / phidelta;
       } else {
          x = -x;
       }
       out[i] = synth_volume_next() * x;
       g.synth.phi = phi;
   }
}


static void add_dry(const float *in, float *out, size_t size) {
   float v = g.settings.dryvolume;
   if (!v) return;
   for (size_t i = 0; i < size; i++) {
      out[i] +=  v * in[i];
   }
}


//==================================================== ZERO CROSSING EVENTS ===


static void zevent_add(int i, int xi, float xv) {
   for (int k = ZSIZE - 1; k > 0; k--) {
      t.z[k] = t.z[k - 1];
   }
   t.z[0].i = i;
   t.z[0].xi = xi;
   t.z[0].xv = xv;
}


static void zevents_wipeout(void) {
   for (int k = 0; k < ZSIZE; k++) {
      t.z[k].i = 0;
      t.z[k].xi = 0;
      t.z[k].xv = 0.0;
   }
}


//========================================================= COMPRESS VOLUME ===


static inline float compress_volume(float volume) {
   if (volume > g.settings.complevel) return volume;
   if (volume > 0.2 * g.settings.complevel) return g.settings.complevel;
   return 5.0 * volume;
}


//=========================================================== PITCH TRACKER ===


static void set_message(float lambda, float volume) {

   // Problem?
   if (lambda == 0.0 && volume > 0.0) {
      if (t.prevvolume > 0.0) {
         lambda = t.prevlambda;
      } else {
         volume = 0.0;
      }
   }

   // Side effects for silence and trigger
   int mtype = MTYPE_NEW;
   if (volume == 0.0) {
      zevents_wipeout();
      t.lambda_raw = 0;
      t.lambda_acf = 0.0;
      t.trig = false;
      t.locked = false;
   } else if (t.trig) {
      mtype = MTYPE_TRIG;
      t.trig = false;
   }

   // Remember these
   t.prevvolume = volume;
   t.prevlambda = lambda;

   // Write new message
   g.message.lambda = lambda;
   g.message.volume = compress_volume(volume);
   g.message.type = mtype; // <-- Message is atomic. This is written last!
   P("%3.1f %5.3f ", g.message.lambda, g.message.volume);
   if (mtype == MTYPE_TRIG) {
       P("T ");
   }
}


static void t_update(void) {

   // We note that g.ring.i can change under our feet so we copy it first.
   t.i_previous = t.i;
   t.i = g.ring.i;
   t.time = g.ring.time; // Only for debug, no need for semaphore

   // Find new zero crossings.
   float themin = 0.0;
   float themax = 0.0;
   float x0;
   float x1 = g.ring.buf[t.i_previous - 1];
   for (uint16_t i = t.i_previous; i != t.i; i++) {
      x0 = x1;
      x1 = g.ring.buf[i];
      if (x0 >= 0.0) {
         if (x1 < 0.0) {
            zevent_add(i, t.xi, t.xv);
            t.xv = x1; t.xi = i; // Start min
         }
         else if (x1 > t.xv) {
            t.xv = x1; t.xi = i; // Update max 
         }
      }
      else {
         if (x1 >= 0) {
            zevent_add(i, t.xi, t.xv);
            t.xv = x1; t.xi = i; // Start max
         }
         else if (x1 < t.xv) { // Update min
            t.xv = x1; t.xi = i;
         }
      }
      if (x1 > themax) themax = x1;
      if (x1 < themin) themin = x1;
   }
   // t.thismax = (themax - themin) / 2.0; // Not really as good :-(
    t.thismax = themax > -themin? themax: -themin;
   //float tmp = themax > -themin? themax: -themin;
   //t.thismax = 0.75 * t.thismax + 0.25 * tmp;

   // We compute trig already here so the analysis can use it
   if ((t.prevlambda == 0.0 && t.thismax > g.settings.triglevel) ||
      (3 * t.thismax > 4 * t.prevmax)) {
      t.trig = true;
   }
   t.prevmax = t.thismax;
}


static bool peakisfeasable(int i, float limit) {
   float x = t.z[i].xv;
   if (limit < 0) limit = -limit;
   if (x > limit) return true;
   if (x < -limit) return true;
   return false;
}


static int checklambda(int lambda) {
   if (lambda < LAMBDA_MIN || lambda > LAMBDA_MAX) return 0;
   return lambda;
}


static int median3(int *p) {
   if (p[0] < p[1]) {
      if (p[2] > p[1]) return p[1];
      if (p[2] > p[0]) return p[2];
      return p[0];
   }
   else {
      if (p[2] > p[0]) return p[0];
      if (p[2] > p[1]) return p[2];
      return p[1];      
   }
}


static bool lambdas_are_close(int lambda1, int lambda2) {
   int wholenote = lambda2 >> 3;
   if (lambda1 > lambda2 + wholenote) return false;
   if (lambda2 > lambda1 + wholenote) return false;
   return true;
}


static bool bumpfitsmuchbetter(int i, int j, int k) {
   uint16_t ui, uj, uk;
   float di, dj, dk, tmp;
   float mj, mk;
   if (k > 16 || t.z[k + 1].xv == 0.0) return false;    

   // Mismatch distance left to peak
   ui = t.z[i].xi - t.z[i + 1].i + 1;
   uj = t.z[j].xi - t.z[j + 1].i + 1;
   uk = t.z[k].xi - t.z[k + 1].i + 1;
   dj = (float)(int16_t)(uj - ui);
   dk = (float)(int16_t)(uk - ui);
   dj = dj * dj;
   dk = dk * dk;
   di = ui * ui;
   mj = dj / di;
   mk = dk / di;

   // Mismatch distance peak to right
   ui = t.z[i].i - t.z[i].xi + 1;
   uj = t.z[j].i - t.z[j].xi + 1;
   uk = t.z[k].i - t.z[k].xi + 1;
   dj = (float)(int16_t)(uj - ui);
   dk = (float)(int16_t)(uk - ui);
   dj = dj * dj;
   dk = dk * dk;
   di = ui * ui;

   // One side can move around heavily if a peak moves through zero.
   // Hopefully this happens only on one side.
   tmp = dj / di;
   if (tmp < mj) mj = tmp;
   tmp = dk / di;
   if (tmp < mk) mk = tmp;

   // Mismatch peak height
   di = t.z[i].xv;
   dj = t.z[j].xv; 
   dk = t.z[k].xv;
   dj = dj - di;
   dj = dj * dj;
   dk = dk - di;
   dk = dk * dk;
   di = di * di;
   mj += dj / di;
   mk += dk / di;

   //P("\nX %d %d %d %0.5f %0.5f\n", i, j, k, mj, mk);
   if (mj > 0.01 && 16.0 * mk < mj) { // mj is squared, so it is 1/10 and 1/4
      return true;
   }
   return false;
}


static int pick_lambda_raw(int lm0, int lm1) {
   lm0 = checklambda(lm0);
   lm1 = checklambda(lm1);
   int lambda = 0;
   int prevlambda = (int)t.prevlambda;

   // Find a good lambda
   if (lm0 == 0) {
      lambda = lm1;
   }
   else if (lm1 == 0) {
      lambda = lm0;
   }
   else if (lambdas_are_close(lm0, lm1)) {
      lambda = (lm0 + lm1) / 2;
   }
   else if (prevlambda == 0) {
      return 0;
   }
   else {
      if (lambdas_are_close(lm0, prevlambda)) return lm0;
      if (lambdas_are_close(lm1, prevlambda)) return lm1;
   }
   if (lambda == 0) return 0;

   // Now we come to part two. What if both sides have gone an octave up to
   // the first overtone? We simply override it.
   if (t.locked && lambdas_are_close(2 * lambda, prevlambda)) {
      lambda = lambda * 2;
   }

   // Also, it happens that bumpfitsmuchbetter because there is another tone
   // interferring and resulting in octave down. We correct it. 
   if (t.locked && lambdas_are_close(lambda, 2 * prevlambda)) {
      lambda = prevlambda;
   }
   return lambda;
}


static void t_lambda_raw_oneside(int i_start, int lambda[3]) {
   int j[2];
   int k = 0;
   float limit = t.thismax / 2;
   // Note: every second peak is on the same side, therefore += 2
   for (int i = i_start; i < ZSIZE - 1; i += 2) {
      if (peakisfeasable(i, limit)) {
         int lim = 3.0 * t.z[i].xv / 4.0;
         if (k == 1 && !peakisfeasable(j[0], lim)) {
            j[0] = i;
            limit = lim;
            continue;
         }
         if (k == 1) {
            // Let me talk to you at this point. Now we are about to make the 
            // decision. Here is a match. But wait a second. A main problem is
            // that the first overtone can be a lot stronger than the 
            // fundamental frequency. What we are going to do now is to check
            // the next bump back to see if it fits substantially better than
            // this one. In that case we take that one instead.
            if (bumpfitsmuchbetter(j[0], i, i + 2)) {
               i += 2;
            }
         }
         j[k++] = i;
         if (k == 2) break;
         limit = 3.0 * t.z[i].xv / 4.0;
         if (limit < 0) limit = -limit;
      }
   }
   if (k == 2) {
      // Beware: an earlier zevent is stored in higher index
      if (t.z[j[1] + 1].xv != 0.0) {
         lambda[0] = (t.z[j[0] + 1].i - t.z[j[1] + 1].i) & RING_MASK; // crossing 1
      }
      lambda[1] = (t.z[j[0]].xi - t.z[j[1]].xi) & RING_MASK; // extreme value
      lambda[2] = (t.z[j[0]].i - t.z[j[1]].i) & RING_MASK; // crossing 2
   }
}


static void t_lambda_raw(void) {
   int lambda[2][3] = {0};
   t_lambda_raw_oneside(0, lambda[0]);
   t_lambda_raw_oneside(1, lambda[1]);

   P("%3d %3d %3d  %3d %3d %3d ",
   lambda[0][0], lambda[0][1], lambda[0][2],
   lambda[1][0], lambda[1][1], lambda[1][2]);

   t.lambda_raw = pick_lambda_raw(median3(lambda[0]), median3(lambda[1]));
   P(" %3d  ", t.lambda_raw);
}


//========================================================= AUTOCORRELATION ===


// Instead of maximizing ACF we minimize normalized sum squared diff.
// That is the same thing.
static float meandiff2mid(int lambda) {
   uint16_t k = t.i - lambda; // For ringbuffer
   float d2first = 0.0;
   float m2first = 0.0;

   // Initialize m2 with the last cycle
   float d2 = 0.0;
   float m2 = 0.0;
   for (uint16_t i = 0; i < lambda; i++) {
      float x = g.ring.buf[(uint16_t)(k + i)];
      m2 += x * x;
   }

   // Go backwards one cycle at a time
   int ncycles = 2;
   for (;; ncycles++) {
      // Do next cycle
      float d2t = 0.0;
      float m2t = 0.0;
      for (int i = 0; i < lambda; i++) {
         --k;
         float x0 = g.ring.buf[k];
         float x1 = g.ring.buf[(uint16_t)(k + lambda)];
         float d = x0 - x1;
         d2t += d * d;
         m2t += x1 * x1;
      }
      // Now, remember the first cycle's value
      if (ncycles == 2) {
         d2 = d2first = d2t;
         m2first = m2; // Not m2t, see above
         m2 += m2t; // Note that m2 now has energy from two cycles
      } else {
         // As long as cycle error has not tripled (OR cycle error grows 
         // to more than 30 percent of first cycle energy) AND cycle energy
         // does not shrink below half of first cycle energy THEN we add this
         // cycle and grab more.
         if ((d2t < 3 * d2first || d2t < 0.3 * m2first) && 2 * m2t > m2first) {
            d2 += d2t;
            m2 += m2t;
         } else {
            // If it breaks here the first time, ncycles is 2.
            break;
         }
      }
      if (ncycles == 16) break;
   }

   d2 = d2 / (float) ((ncycles - 1) * lambda);
   int n = ncycles * lambda;
   m2 = m2 / (float) n;
   if (m2 == 0.0) m2 = 1.0; // No div by 0 on next line
   d2 = d2 / m2;
   t.volume = sqrt(m2); // TODO: remove volume
   t.acf_m2 = m2;
   t.acf_d2 = d2;
   t.acf_len = n;
   P("%2d %.3f %0.3f ", ncycles, t.volume, d2);
   return d2;
}


// The window size and m2 are already precomputed
static float meandiff2(int lambda) {
   int n = t.acf_len;
   float d2 = 0.0;
   uint16_t i_stop = t.i - lambda;
   for (uint16_t i = t.i - n; i != i_stop; i++) {
      float x0 = g.ring.buf[i];
      float x1 = g.ring.buf[(uint16_t)(i + lambda)];
      float d = x0 - x1;
      d2 += d * d;
   }
   return d2 / (t.acf_m2 * (float)(n - lambda));
}


static float t_lambda_acf(int lM) {
   float dL, dM, dR, b, c;
   int lL, lR, delta;
   float lHat = 0.0;
   if (lM == 0) {
      goto bail;
   }
   delta = lM / 50; // Halftone approximately
   if (delta < 2) delta = 2;
   lL = lM - delta;
   lR = lM + delta;
   dM = meandiff2mid(lM);
   if (t.acf_d2 > ACFD2_MAX) {
      goto bail;
   }
   dL = meandiff2(lL);
   dR = meandiff2(lR);
   b = dR - dL;
   c = dR - 2.0 * dM + dL;
   if (c <= 0.0) {
      goto bail;
   }
   lHat = lM - (float)delta * (b / (2.0 * c));
   if (lHat < lL || lR < lHat) lHat = 0.0;

   bail:
   if (lHat == 0.0) t.acf_d2 = ACFD2_MAX;
   return lHat;
}



//================================================================ EXPORTED ===


int moly_init(uint32_t sampleFrequency) {
   g.settings.sample_frequency = sampleFrequency;
   g.settings.dryvolume = 0.0;
   g.settings.wetvolume = 0.5;
   g.settings.triglevel = 0.08;
   g.settings.complevel = 0.0;
   g.settings.verbose = 0;
   return 0;
}


void moly_synth(const float *in, float *out, size_t size) {
   if (g.settings.sample_frequency == 0.0) return;
   synthesizer(out, size);
   add_dry(in, out, size);
}


void moly_synth_message(struct moly_message *m) {
   // Do nothing. The synth knows where the message resides :-).
}


struct moly_message* moly_analyze(void) {
   float d2;
   t_update();
   P("%zu %.3f  ", t.time, t.thismax);

   // Silence?
   if (t.thismax < SILENCE_LEVEL || 
      (t.prevlambda == 0.0 && t.thismax < g.settings.triglevel)) {
      set_message(0.0, 0.0);
      goto bail;
   }

   // Not silence!
   t.lambda_acf = 0.0;
   d2 = ACFD2_MAX;
   if (t.prevlambda != 0.0) {
      //t.lambda_acf = t_lambda_acf(t.lambda_raw);
      if (t.lambda_acf != 0.0) d2 = t.acf_d2;
   }
   if (t.lambda_acf == 0.0 || d2 > 0.1) {
      t_lambda_raw();
      float tmp = t_lambda_acf(t.lambda_raw);
      if (t.acf_d2 < d2) {
         t.lambda_acf = tmp;
         d2 = t.acf_d2;
      }
   }
   if (d2 < 0.1) t.locked = true;
   set_message(t.lambda_acf, t.thismax);

   bail:
   P("\n");
   return &g.message;
}


void moly_set(char opt, float val) {
   if (opt == 'd') g.settings.dryvolume = val;
   if (opt == 'w') g.settings.wetvolume = val;
   if (opt == 't') g.settings.triglevel = val;
   if (opt == 'c') g.settings.complevel = val;
   if (opt == 'v') g.settings.verbose = (int)val;
}

