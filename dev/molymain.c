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
"       This is a command line tool for experimenting with pitch tracking.\n"
"\n"
"       -v  Verbose, print one line per processed pitch estimation.\n"
"       -t  Autotune\n"
"\n"
"       The following arguments each take a floating point argument (defaults\n"
"       in parenthesis)."
"\n"
"       -y  Dryvolume (0.0)\n"
"       -w  Wetvolume (1.0)\n"
"       -i  Sensivity (0.08)\n"
"       -a  Attack (0.1)\n"
"       -d  Decay (0.1)\n"
"       -s  Sustain (0.3)\n"
"       -r  Release (0.2)\n"
"       -w  Waveform (0: square)\n"
"       -x  Envelmix (0: ADSR, 1: instrument, 2: A+instrument+D)\n"
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
   char *filename = 0;
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
         } else if (argv[i][0] == '-') {
            int c = argv[i][1];
            if (index("yeiadsrtx", c)) {
               moly_set(c, optarg(&argv[i][2]));
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
         filename = argv[i];
      }
   }
   if (!filename) {
      printf("%s", helptext);
      exit(1);
   }

   // Open
   struct session *o = newSession(filename, optPrintInfo);

   // Start writing outfile
   FILE *f = fopen("tmp.wav", "wb");
   assert(f);
   fprintf(f, "RIFF....WAVE");
   fwrite("fmt \x10\0\0\0\x01\0\x01\0\x44\xAC\0\0\x88\x58\x01\0\x02\0\x10\0", 
      24, 1, f);
   fprintf(f, "data....");
   size_t d = ftell(f);

   // Process and make a linked list of everything
   int acount = 0;
   float inbuf[BSZ];
   float outbuf[BSZ];
   while (o->p <= o->p_end - BSZ * 2) {
      for (int i = 0; i < BSZ; i++) {
         inbuf[i] = (float)*o->p++ / 32768.0;
         o->p++; // Skip other channel
      }
      moly_callback(inbuf, outbuf, BSZ);
      for (int i = 0; i < BSZ; i++) {
         float x = outbuf[i] * 32768.0;
         if (x < -32767.0) x = -32767.0;
         if (x > 32768.0) x = 32768.0;
         int16_t y = (int16_t)(outbuf[i] * 32768.0);
         fwrite(&y, sizeof(int16_t), 1, f);
      }

      // To simulate a real DSP system where the callback runs in high prioriy
      // we run the main analyze function every 10th time which is about 100 
      // times per second.
      if (++acount == 10) {
         acount = 0;
         moly_analyze();
      }
   }

   // Write 2 datasizes, then done
   uint32_t filesize = (uint32_t)ftell(f);
   uint32_t datasize = (uint32_t)(ftell(f) - d);
   fseek(f, d - 4, SEEK_SET);
   fwrite(&datasize, 4, 1, f);
   fseek(f, 4, SEEK_SET);
   filesize -= 8;
   fwrite(&filesize, 4, 1, f);
   fclose(f);
}
