#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "molysynth.h"

char helptext[] =
"**************** TO BE FIXED *******\n"
"NAME\n"
"       moly - guitar synth processing\n\n"
"SYNOPSIS\n"
"       moly [options] wavfile\n\n"
"DESCRIPTION\n"
"       This is a command line tool for experimenting with guitar synth\n"
"       algorithms.\n"
"\n"
"       The following options are available. The defaults are given both\n"
"       for no option and for option without argument.\n"
"\n"
"       -v  Verbose, print one line per processed pitch estimation.\n"
"       -t  Autotune.\n"

"       -a  Auto-tune (0..9, default 0/9)\n"
"       -c  Octave down (0..9 blend, default 0/9)\n"
"       -e  Envelope: Attack, Decay, Sustain, Release (0..9)\n"
"       -g  Output gain (0..9, default 3/6).\n"
"       -h  Print this helptext.\n"
"       -m  Passthrough mix (0..9 blend, default 0/4).\n"
"       -o  Outfile (default tmp.wav).\n"
"       -p  Print info about the wav file.\n"
"       -r  Trigger level (0-9: 1 << (6+x), default 5)\n"
"       -w  Waveform (0..3 is none/sine/square/sawtooth).\n"
"       -y  Velocity sensivity, dynamic (0..9 default 9/0).\n"
"       -z  Fuzzbox (0..9, default 0/5).\n"
"       -Z  Tube screamer (0..9, default 0/5).\n"
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


int optarg(char c) {
   assert('0' <= c && c <= '9');
   return c - '0';
}


int main(int argc, char *argv[]) {
   char *filename = 0;
   int optPrintInfo = 0;

   // Options
   int opt_w = 0;
   int opt_z = 0;
   int opt_y = 0;
   int opt_r = 5;
   int opt_v = 0;
   int opt_t = 0;
   for (int i = 1; i < argc; i++) {
      if (argv[i][0] == '-') {
         if (!strcmp(argv[i], "-h")) {
            printf("%s", helptext);
            exit(0);
         } else if (!strcmp(argv[i], "-p")) {
            optPrintInfo = 1;
         } else if (!strncmp(argv[i], "-z", 2)) {
            opt_z = optarg(argv[i][2]);
         } else if (!strncmp(argv[i], "-w", 2)) {
            opt_w = optarg(argv[i][2]);
         } else if (!strncmp(argv[i], "-t", 2)) {
            opt_t = optarg(argv[i][2]);
         } else if (!strncmp(argv[i], "-y", 2)) {
            opt_y = optarg(argv[i][2]);
         } else if (!strncmp(argv[i], "-r", 2)) {
            opt_r = optarg(argv[i][2]);
         } else if (!strcmp(argv[i], "-v")) {
            opt_v = 1;
         } else {
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
   moly_init(44100.0);

   // Start writing outfile
   FILE *f = fopen("tmp.wav", "wb");
   assert(f);
   fprintf(f, "RIFF....WAVE");
   fwrite("fmt \x10\0\0\0\x01\0\x01\0\x44\xAC\0\0\x88\x58\x01\0\x02\0\x10\0", 
      24, 1, f);
   fprintf(f, "data....");
   size_t d = ftell(f);

   // Process and make a linked list of everything
   //TODOstruct msyn *s = msyn_create();
   moly_set('w', opt_w);
   moly_set('z', opt_z);
   moly_set('t', opt_t);
   moly_set('y', opt_y);
   moly_set('r', opt_r);
   moly_set('v', opt_v);

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
         int16_t x = (int16_t)(outbuf[i] * 32768.0);
         fwrite(&x, sizeof(int16_t), 1, f);
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
