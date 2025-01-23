# Molysynth

Contributed by Anders Holtsberg \<<anders.holtsberg@gmail.com>\>

Molysynth is an open-source electric molyphonic guitar pitch tracker. 
It demonstrates a simple way to develop real time sound algorithms off-line. 
The programming language is C. Only C. KISS to you from me.

You are only interested in this package if you are interested in
   * music instrument real-time __pitch tracking__ in general, or
   * anything that can run on board a __Cleveland Music Hothouse__, or
   * any other __Daisy Seed__ pedal, or
   * any other sound real-time __DSP__.


## Description

This is an advanced guitar pitch tracker. The small small synth is included
for the purpose of testing the pitch tracker.
The whole idea is to develop a C-code guitar monophonic pitch tracker suitable
for use in DSP real-time hardware. One of the main ideas is to 
develop it off-line, reading a WAV file and producing a WAV file. 
Then - and only then -
compile it and upload it to Hothouse Daisy Seed without any hazzle.
This will also make it totally self sufficient for anyone wanting to put it
on other hardware.

The design choice here is to develop it like it resides in the
HothouseExamples source tree.
But actually, you don't have to download _any_ other source code. This repo is
totally self sufficient until you actually want to put the code on a DSP. 
You can just enjoy developing off-line without any dependencies.

To repeat: if you have the Cleveland Music Hothouse source code then put this at the 
expected place. If you don't, then download and enjoy anyway. Did I mention that 
it has _no_ dependencies?

   * In the top level library we have the file `molysynth.cpp` as usual in a
     Hothouse project. But
     this is just a thin wrapper and for off-line development this is not
     needed. For off-line development we use the libraries __src__, __dev__ 
     and __dat__.
   * The __src__ library contains the real source code in `molysynth.c` with
     no dependencies at all on Daisy Seed.
   * The __dev__ library contains code for working off-line with WAV files.
     It also contains a Jupyter Notebook containing Julia code. 
     That serves as a starting point if you are litterate in Julia and want 
     to analyze WAV data that goes into or comes out of Molysynth. Copy the
     file before using it because you do _not_ want to check in a ton of data, so
     do _not_ track your own notebook with git.
     If you use Python then maybe
     that notebook can simply be an inspiration or maybe you have your own way 
     of looking at WAV files.
   * The __dat__ library contains a WAV file to get you started. I use Garage Band
     to record my own WAV files to experiment with. 

Anyway, just download this to your machine and go into __dev__ and do `make test`
and listen and enjoy. 


## Goals

The goal in the end will be to track a lot better than the competition, ie 

 * __Electro-Harmonix mono__
 * __Keeley synth 1__

Also, harmonizers, like __Mooer Harmony X2__ use a pitch tracker.

Note that __Mooer E7__ is _not_ a synth despite the name, not in the meaning 
used here.
It does not synthesize the direct sound - yes it adds synthesized ambient sound and
stuff but the acual base frequencies when playing chords come from the guitar. 

I have yet to analyze __Boss SY-1__ to find out what is actually going on inside it.

One difficulty is that in a true monophonic tracker it is difficult to 
figure out a tone if the last tone is still sounding on another string on the guitar, 
and that problem is the research main point for me. I have not implemented
or even tested my ideas so far, this is only the framework for being
able to do it. 
I may add FFT to play with polyphonic output too, but that is
also for later.

Anyway, you can use this code as a framework for trying out your own ideas.


## Hothouse controls

| CONTROL | DESCRIPTION | NOTES |
|-|-|-|
| KNOB 1 | Sensitivity |  |
| KNOB 2 | Dry volume |  |
| KNOB 3 | Wet volume |  |
| KNOB 4 | Attack/Decay |  (Need more knobs...) |
| KNOB 5 | Sustain |  |
| KNOB 6 | Release |  |
| SWITCH 1 | Wave form | **SQUARE** - <br/>**SAWTOOTH** - <br/>**TRIANGLE** -  |
| SWITCH 2 | Autotune | **AUTOTUNE** - <br/>**OFF** - <br/>**SLIDE** - |
| SWITCH 3 | Envelope | **ADSR** - <br/>**INSTRUMENT** - <br/>**AR+INSTRUMENT** - |
| FOOTSWITCH 1 | Octave same | Octave down |
| FOOTSWITCH 2 | Bypass | Molysynth |
