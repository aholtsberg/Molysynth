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


// Note: for all lambda in the chain 0 is silence and -1 is no clue.
#define LAMBDA_SILENCE 0
#define LAMBDA_NOCLUE -1
#define LAMBDA_MIN 30
#define LAMBDA_MAX 550
#define RING_MASK ((1<<16) - 1)


struct {

   // Settings
   struct {
      float sample_frequency;
      int waveform;
      int autotune;
      int volsens;
      float triglevel;
      // Debog of-line
      int verbose;
      // Synth
      int attack;
      int decay;
      int sustain;
      int release;
   } settings;

   // Synth
   struct {
      float lambda;
      float phi;
      float volume;
      // ADSR
      int state; // 0: silent, 1: attack, 2: decay, 3: sustain, 4:release
      float attack; // delta per sample until volume 0.5
      float decay; // delta per sample until sustain
      float sustain; // 0.0 - 0.5
      float release; // delta per sample until 0.0
   } synth;

   // Decimation filter
   struct {
      float x1;
      float x2;
   } filter;

   // Message from tracker
   struct {
      int state; // 1: new values, 2: trigger, 0: already processed
      float lambda;
      float volume;
   } message;

   // Ringbuffer
   struct {
      float buf[(1<<16)];
      uint16_t i; // Note: this can index the ringbuffer without MASK!
      size_t time;
   } ring;

} g;


#define ZSIZE 32


struct {

   // For ringbuffer
   uint16_t i;
   uint16_t i_previous;
   size_t time;

   // For remembering extreme points between calls
   int xi;
   float xv;

   // Temporaries
   float thismax;
   int lambda_raw;
   float lambda_acf;
   float volume;
   float lambda_fit;
   float acf_m2;
   int acf_len;
   char autotune_name[4];

   // The latest zero crossings
   struct zevent {
      uint16_t i; // zerocrossing index in ringbuffer
      uint16_t xi; // extreme value index in ringbuffer
      float xv; // extreme value
   } z[ZSIZE];

} t;


//============================================================= RING BUFFER ===


inline static float lpfilter(float x) {
   float y = x + 1.8 * g.filter.x1 - 0.82 * g.filter.x2;
   g.filter.x2 = g.filter.x1;
   g.filter.x1 = y;
   return 0.02 * y;
}


static inline void ringbuffer_write(const float *in, size_t size) {
   for (size_t i = 0; i < size; i++) {
      g.ring.buf[g.ring.i++] = lpfilter(in[i]);
   }
   g.ring.time += size;
}


//============================================================= SYNTHESIZER ===


static inline void synthesizer(float *out, size_t size) {
   
   // Read message
   if (g.message.state != 0) {
      g.synth.lambda = g.message.lambda;
      g.message.state = 0;
   }

   // Silence
   if (g.synth.lambda == LAMBDA_SILENCE) {
      for (size_t i = 0; i < size; i++) {
         out[i] = 0;
      }
      g.synth.phi = 0.0;
      g.synth.volume = 0.0;
      return;
   }

   // Run
   float phidelta = 1.0 / g.synth.lambda;
   float vol = 0.5;
   if (g.settings.volsens) {
      vol = g.message.volume;
   }
   for (size_t i = 0; i < size; i++) {
       float phi = g.synth.phi + phidelta;
       float x;
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


//=============================================================  AUTOTUNE =====


// These valuses are for 44.1 kHz
const float standard_lambda[] = {
535.149432, 505.113803, 476.763943, 450.005239, 424.748386, 400.909091, 
378.407793, 357.169395, 337.123017, 318.201756, 300.342464, 283.485537, 
267.574716, 252.556901, 238.381972, 225.002620, 212.374193, 200.454545, 
189.203896, 178.584698, 168.561509, 159.100878, 150.171232, 141.742768, 
133.787358, 126.278451, 119.190986, 112.501310, 106.187097, 100.227273, 
 94.601948,  89.292349,  84.280754,  79.550439,  75.085616,  70.871384, 
 66.893679,  63.139225,  59.595493,  56.250655,  53.093548,  50.113636, 
 47.300974,  44.646174,  42.140377,  39.775220,  37.542808,  35.435692, 
 33.446840,   0.0
};


// These names will be used of-line only
const char standard_name[] =
            " E; F;#F; G;#G; A;bB; B;"
" C,#C, D,#D, E, F,#F, G,#G, A,bB, B," 
" C #C  D #D  E  F #F  G #G  A bB  B " 
" c #c  d #d  e  f #f  g #g  a bb  b " 
" c'#c' d'#d' e'";


float autotune(float x) {
   if (x == 0.0) return 0.0;
   for (int i = 1; 1; i++) {
      float x0 = standard_lambda[i - 1];
      float x1 = standard_lambda[i];
      if (x1 == 0.0) {
         goto return_x0;
      }
      if (x >= x1) {
         if (x0 - x < x - x1) {
            return_x0:
            strncpy(t.autotune_name, standard_name + 3 * (i - 1), 3);
            return x0;
         } else {
            strncpy(t.autotune_name, standard_name + 3 * i, 3);
            return x1;
         }
      }
   }
}


//================================================================= ZEVENTS ===


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
      t.z[k].xv = 0;
   }
}


//=========================================================== PITCH TRACKER ===


static void t_update(void) {

   // We note that g.ring.i can change under our feet so we copy it once.
   t.i_previous = t.i;
   t.i = g.ring.i;
   t.time = g.ring.time; // Only for debug, no need for semaphore

   // Find new zero crossings (and compute thismax on the fly).
   t.thismax = 0.0;
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
      if (x1 > t.thismax) t.thismax = x1;
      if (-x1 > t.thismax) t.thismax = -x1;
   }

}


static bool peakisfeasable(int i, float limit) {
   float x = t.z[i].xv;
   if (limit < 0) limit = -limit;
   if (x > limit) return true;
   if (x < -limit) return true;
   return false;
}


// Note: checklambda returns -1 for 0. That is correct.
int checklambda(int lambda) {
   if (lambda < LAMBDA_MIN || lambda > LAMBDA_MAX) return LAMBDA_NOCLUE;
   return lambda;
}


int median3(int *p) {
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


bool lambdas_are_close(int lambda1, int lambda2) {
   int halfnote = lambda2 >> 4;
   if (lambda1 > lambda2 + halfnote) return false;
   if (lambda2 > lambda1 + halfnote) return false;
   return true;
}


int picklambdaraw(int lm0, int lm1) {
   lm0 = checklambda(lm0);
   lm1 = checklambda(lm1);

   // If one fails, the other may be functioning
   if (lm0 == LAMBDA_NOCLUE) {
      return lm1;
   }
   if (lm1 == LAMBDA_NOCLUE) {
      return lm0;
   }

   // This is what we want
   if (lambdas_are_close(lm0, lm1)) {
      return (lm0 + lm1) / 2;
   }

   // Check with history otherwise
   if (t.lambda_raw <= 0) {
      return LAMBDA_NOCLUE;
   }
   if (lambdas_are_close(lm0, t.lambda_raw)) {
      return  lm0;
   } 
   if (lambdas_are_close(lm1, t.lambda_raw)) {
      return  lm1;
   } 
   return LAMBDA_NOCLUE;
}


static void t_lambda_raw_oneside(int i_start, float *xv, int lambda[3]) {
   int j[2];
   int k = 0;
   float limit = t.thismax / 2;
   for (int i = i_start; i < ZSIZE - 1; i += 2) {
      if (peakisfeasable(i, limit)) {
         int lim = 3.0 * t.z[i].xv / 4.0;
         if (k == 1 && !peakisfeasable(j[0], lim)) {
            j[0] = i;
            limit = lim;
            continue;
         }
         j[k++] = i;
         limit = 3.0 * t.z[i].xv / 4.0;
         if (limit < 0) limit = -limit;
         if (k == 2) break;
      }
   }
   if (k == 2) {
      // Beware: an earlier zevent is stored in higher index
      lambda[0] = (t.z[j[0] + 1].i - t.z[j[1] + 1].i) & RING_MASK; // crossing 1
      lambda[1] = (t.z[j[0]].xi - t.z[j[1]].xi) & RING_MASK; // extreme value
      lambda[2] = (t.z[j[0]].i - t.z[j[1]].i) & RING_MASK; // crossing 2
      *xv = (t.z[j[0]].xv + t.z[j[1]].xv) / 2;
      if (*xv < 0) *xv = -*xv;
   }
}


static void t_lambda_raw(void) {
   int lambda[2][3] = {0};
   float xv[2] = {0};

   t_lambda_raw_oneside(0, xv + 0, lambda[0]);
   t_lambda_raw_oneside(1, xv + 1, lambda[1]);

   P("%3d %3d %3d  %3d %3d %3d ",
   lambda[0][0], lambda[0][1], lambda[0][2],
   lambda[1][0], lambda[1][1], lambda[1][2]);

   t.lambda_raw = picklambdaraw(median3(lambda[0]), median3(lambda[1]));
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
         // As long as cycle error has not doubled (OR cycle error grows 
         // to more than 20 percent of first cycle energy) AND cycle energy
         // does not shrink below half of first cycle energy THEN we add this
         // cycle and grab more.
         if ((d2t < 2 * d2first || d2t < 0.2 * m2first) && 2 * m2t > m2first) {
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
   t.volume = sqrt(m2);
   t.lambda_fit = d2;
   t.acf_m2 = m2;
   t.acf_len = n;
   P("%2d %.3f ", ncycles, t.volume);
   return d2;
}


// The window size and m2 are already precomputed
float meandiff2(int lambda) {
   int n = t.acf_len;
   float d2 = 0.0;
   uint16_t i_stop = t.i + n - lambda;
   for (uint16_t i = t.i - n; i != i_stop; i++) {
      float x0 = g.ring.buf[i];
      float x1 = g.ring.buf[(uint16_t)(i + lambda)];
      float d = x0 - x1;
      d2 += d * d;
   }
   return d2 / (t.acf_m2 * (float)(n - lambda));
}


static void t_lambda_acf() {
   int lM = t.lambda_raw;
   int delta = lM / 50;
   if (delta < 2) delta = 2;
   int lL = lM - delta;
   int lR = lM + delta;
   float dM = meandiff2mid(lM);
   P("%1.3f ", dM);
   float dL = meandiff2(lL);
   float dR = meandiff2(lR);
   float b = dR - dL;
   float c = dR - 2.0 * dM + dL;
   if (c == 0.0) {
      // TODO 
      t.lambda_acf = 0.0;
      t.lambda_fit = 0.0;
      return;
   }
   float lHat = lM - (float)delta * (b / (2.0 * c));
   if (lHat < lL || lR < lHat) {
      lHat = 0.0;
   }
   t.lambda_acf = lHat;
   P("%5.1f ", t.lambda_acf);
   return;
}



//================================================================ EXPORTED ===


int moly_init(uint32_t sampleFrequency) {
   g.settings.sample_frequency = sampleFrequency;
   g.settings.triglevel = 0.08;
   if (sampleFrequency != 44100) {
      // TODO autotune
   }
   return 0;
}


void moly_callback(const float *in, float *out, size_t size) {
   if (g.settings.sample_frequency == 0.0) return;
   synthesizer(out, size);
   ringbuffer_write(in, size);
}


void moly_analyze(void) {
   t_update();
   P("%zu %.2f  ",  t.time, t.thismax);

   // Silence?
   if (t.thismax < g.settings.triglevel / 2 || 
      (t.lambda_raw == LAMBDA_SILENCE && t.thismax < g.settings.triglevel)) {
      zevents_wipeout();
      t.lambda_raw = LAMBDA_SILENCE;
      t.lambda_acf = 0.0;
      g.message.lambda = 0.0;
      g.message.volume = 0.0;
      g.message.state = 1;
      P("\n");
      return;
   }

   // Lambda
   t_lambda_raw();
   if (t.lambda_raw <= 0) {
      P("\n");
      return; // No message!
   }
   t_lambda_acf();
   if (t.lambda_acf == 0.0) {
      P("\n");
      return;
   }

   // Autotune and trigger
   if (g.settings.autotune) {
      t.lambda_acf = autotune(t.lambda_acf);
      P("%5.1f %s ", t.lambda_acf, t.autotune_name);
   }

/*
   trigger()
*/
   P("\n");

   if (t.lambda_acf > 0) {
      g.message.lambda = t.lambda_acf;
      g.message.volume = 1.0;
      g.message.state = 1;
   }
}


void moly_set(char opt, int val) {
   if (opt == MOLY_ATTACK) g.settings.attack = val;   // 0-999
   if (opt == MOLY_DECAY) g.settings.decay = val;     // 0-999
   if (opt == MOLY_SUSTAIN) g.settings.sustain = val; // 0-999
   if (opt == MOLY_RELEASE) g.settings.release = val; // 0-999

   if (opt == 'w') g.settings.waveform = val;
   if (opt == 't') g.settings.autotune = val;
   if (opt == 'y') g.settings.volsens = val;
   if (opt == 'v') g.settings.verbose = val;
   //if (opt == 'r') g.settings.triglevel = 1 << (6 + val);
}

