#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "molysynth.h"

char helptext[] =
"NAME\n"
"       moly - guitar synth processing\n\n"
"SYNOPSIS\n"
"       moly [options] wavfile\n\n"
"DESCRIPTION\n"
"       This is a command line tool for experimenting with guitar pitch tracking.\n"
"\n"
"       -v  Verbose, print one line per processed pitch estimation.\n"
"       -o  Output file, tmp.wav is default.\n"
"       -p  Print info about infile."
"\n"
"       All following arguments each take a floating point argument (defaults\n"
"       in parentheses).\n"
"\n"
"       ### Pitch tracker\n"
"       -t  Trig level (0.08)\n"
"       -c  Compress (0.0)\n"
"\n"
"       ### Synth\n"
"       -d  Dryvolume (0.0)\n"
"       -w  Wetvolume (0.5)\n"
"\n";

struct format {
   uint16_t audioFormat;
   uint16_t nbrChannels;
   uint32_t frequency;
   uint32_t bytePerSec;
   uint16_t bytePerBlock;
   uint16_t bitsPerSample;
};


struct chunk {
   uint32_t id;
   uint32_t size;
   int16_t data[];
};


struct session {
   struct format *format;
   int16_t *p;
   int16_t *p_end;
};


#define BSZ 48


// ------------------------------------------------------------- READ WAV -----


void *fmmap(const char *filename, size_t *size) {
   void *buf;
   FILE *file = fopen(filename, "rb");
   if (!file) {
      fprintf(stderr, "Could not open file %s\n", filename);
      return NULL;
   }
   fseek(file, 0, SEEK_END);
   *size = ftell(file);
   buf = malloc(*size);
   if (buf) {
      fseek(file, 0L, SEEK_SET);
      fread(buf, 1, *size, file);
   }
   fclose(file);
   return buf;
}


struct chunk *findChunk(struct chunk *p, uint32_t id) {
   uint32_t n;

   // Header
   if (((struct chunk *)p)->id != 'FFIR') return NULL; // RIFF
   n = ((struct chunk *)p)->size;
   if (((uint32_t *)p)[2] != 'EVAW') return NULL; // WAVE
   n -= 4;
   p = (struct chunk *)((char *)p + 12);

   // Run
   while (n >= 12) {
      // Flat layout only, no 'FORM'
      if (p->id == id) {
         return p;
      }
      int32_t m = 8 + p->size;
      if (m & 3) return NULL; // Align 4 please
      p = (struct chunk *)((char *)p + m);
      n -= m;
   }
   return NULL;
}


void wavInit(struct session *o, uint8_t *m, int optPrintInfo) {
   struct chunk *c;
   c = findChunk((struct chunk *)m, ' tmf'); // <fmt >
   assert(c);
   o->format = (struct format *)&c->data; 
   c = findChunk((struct chunk *)m, 'atad'); // <data>
   assert(c);
   o->p_end = (int16_t *)((uint8_t *)&c->data + c->size);
   o->p = c->data;

   if (optPrintInfo) {
      printf("audioFormat %d\n", o->format->audioFormat);
      printf("nbrChannels %d\n", o->format->nbrChannels);
      printf("frequency %d\n", o->format->frequency);
      printf("bytePerSec %d\n", o->format->bytePerSec);
      printf("bytePerBlock %d\n", o->format->bytePerBlock);
      printf("bitsPerSample %d\n", o->format->bitsPerSample);
   }
}


// ------------------------------------------------------------ WRITE WAV -----


FILE* wavout;
size_t wavout_here;

void wavout_start(char *filename) {
   wavout = fopen(filename, "wb");
   assert(wavout);
   fprintf(wavout, "RIFF....WAVE");
   fwrite("fmt \x10\0\0\0\x01\0\x01\0\x44\xAC\0\0\x88\x58\x01\0\x02\0\x10\0", 
      24, 1, wavout);
   fprintf(wavout, "data....");
   wavout_here = ftell(wavout);
}


void wavout_end(void) {
   uint32_t filesize = (uint32_t)ftell(wavout);
   uint32_t datasize = (uint32_t)(ftell(wavout) - wavout_here);
   fseek(wavout, wavout_here - 4, SEEK_SET);
   fwrite(&datasize, 4, 1, wavout);
   fseek(wavout, 4, SEEK_SET);
   filesize -= 8;
   fwrite(&filesize, 4, 1, wavout);
   fclose(wavout);
}


// -------------------------------------------------------------- SESSION -----


struct session *newSession(char *filename, int optPrintInfo) {
   size_t n;
   assert(filename);
   uint8_t *m = fmmap(filename, &n);
   assert(m);
   struct session *o = calloc(sizeof(struct session), 1);
   assert(o);
   wavInit(o, m, optPrintInfo);
   return o;
}


float optarg(char *c) {
   assert(('0' <= *c && *c <= '9') || *c == '.');
   float x = 0.0;
   int k = 1;
   for (;; c++) {
      if (*c == '.') {
         assert(k == 1);
         k = 10 * k;
      } else if ('0' <= *c && *c <= '9') {
         int d = (*c - '0');
         if (k == 1) {
            x = 10 * x + (float)d;
         } else {
            x += d / (float)k;
            k = 10 * k;
         }
      } else {
         return x;
      }
   }
}


int main(int argc, char *argv[]) {
   char *fileIn = 0;
   char *fileOut = "tmp.wav";
   int optPrintInfo = 0;

   moly_init(44100.0);

   // Options
   for (int i = 1; i < argc; i++) {
      if (argv[i][0] == '-') {
         if (!strcmp(argv[i], "-h")) {
            printf("%s", helptext);
            exit(0);
         } else if (!strcmp(argv[i], "-v")) {
            moly_set('v', 1.0);
         } else if (!strcmp(argv[i], "-p")) {
            optPrintInfo = 1;
         } else if (!strcmp(argv[i], "-o")) {
            ++i;
            assert(i < argc);
            fileOut = argv[i];
         } else if (argv[i][0] == '-') {
            int c = argv[i][1];
            if (index("tcdw", c)) {
               char *p = argv[i] + 2;
               if (*p == '\0') {
                  ++i;
                  assert(i < argc);
                  p = argv[i];
               }
               moly_set(c, optarg(p));
            } else {
               goto bail;
            }
         } else {
            bail:
            printf("There is no option %s\n", argv[i]);
            exit(1);
         }
      }
      else {
         fileIn = argv[i];
      }
   }
   if (!fileIn) {
      printf("%s", helptext);
      exit(1);
   }

   // Open
   struct session *o = newSession(fileIn, optPrintInfo);
   wavout_start(fileOut);

   // Process and make a linked list of everything
   int mycount = 0;
   float inbuf[BSZ];
   float outbuf[BSZ];
   while (o->p <= o->p_end - BSZ * 2) {

      // 1. Read next input buffer
      for (int i = 0; i < BSZ; i++) {
         inbuf[i] = (float)*o->p++ / 32768.0;
         o->p++; // Skip other channel
      }

      // 2. High priority
      moly_addtobuf(inbuf, BSZ);
      moly_synth(inbuf, outbuf, BSZ); // <-- Replace by your own synth

      // 3. Write the result to file
      for (int i = 0; i < BSZ; i++) {
         float x = outbuf[i] * 32768.0;
         if (x < -32767.0) x = -32767.0;
         if (x > 32767.0) x = 32767.0;
         int16_t y = (int16_t)x;
         fwrite(&y, sizeof(int16_t), 1, wavout);
      }

      // 4. Low-priority
      // To simulate a real DSP system, where the analysis is in low-priority,
      // we run the main analyze function every 10th time which is about 100 
      // times per second.
      if (++mycount == 10) {
         mycount = 0;
         struct moly_message *m = moly_analyze();
         moly_synth_message(m); // <-- Replace by your own synth
      }
   }

   wavout_end();
}
