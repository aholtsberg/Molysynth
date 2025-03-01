# Molysynth

Contributed by Anders Holtsberg \<<anders.holtsberg@gmail.com>\>

Molysynth is an open-source electric monophonic guitar pitch tracker. 
It also demonstrates a simple way to develop real time sound algorithms off-line. 
The programming language is C. Only C, written in a style I enjoy.

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
You can just enjoy exploring ideas off-line without any dependencies.

To repeat: if you have the Cleveland Music Hothouse source code then put this at the 
expected place. If you don't, then download and enjoy anyway. 

   * In the top level library we have the file `molysynth.cpp` as usual in a
     Hothouse project. But
     this is just a thin wrapper and for off-line development this is not
     needed. For off-line development we use the libraries __src__, __dev__ 
     and __wav__.
   * The __src__ library contains the real source code in `molysynth.c` with
     no dependencies at all on Daisy Seed.
   * The __dev__ library contains code for working off-line with WAV files.
     It also contains a Jupyter Notebook containing Julia code. 
     That serves as a starting point if you are litterate in Julia and want 
     to analyze WAV data that goes into or comes out of Molysynth. Copy the
     file before using it because you do _not_ want to check in a ton of data.
     If you use Python then maybe
     that notebook can simply be an inspiration or maybe you have your own way 
     of looking at WAV files.
   * The __wav__ library contains a WAV file to get you started. I use Garage Band
     to record my own WAV files to experiment with. 

Anyway, just download this to your machine and go into __dev__ and do `make test`
and listen and enjoy. 


## Goals

The goal in the end will be to build an open source tracker. A pitch tracker
is something that converts the guitar signal to a stream of frequency + volume 
messages in pedals like 

 * __Electro-Harmonix mono__
 * __Keeley synth 1__

Also, harmonizers, like __Mooer Harmony X2__ use a pitch tracker.

Note that __Mooer E7__ is _not_ a synth despite the name, not in the meaning 
used here, because
it does not synthesize the direct sound - yes it adds synthesized ambient
sound and stuff but the acual fundamental sound when playing chords seems to
come from the guitar itself. I have yet to analyze __Boss SY-1__ to find out
what is actually going on inside it.

Note that the example wav file is played on a guitarra baiana, that is an 
instrument tuned in fifths starting with C, so the little strange sounds
that can be heard in the scale come from the G plucked when the F is still 
sounding, and likewise for A when C is still sounding. That is a research
problem to fix that. 

Anyway, you can use this code as a framework for trying out your own ideas.


## Ideas for future work

Some future work remains in the tracker. Autotune is not mentioned since I tried it
and it makes no sense having it, simply because it feels no good playing with it and
I don't want to put development efforts into something that will not be used. Tell
me if you need it and I may put it back. 

  * Add FFT and make it experiment with polyphonic tracking.
  * Better tracking when the previous tone is still sounding on another string.

Here are some ideas you can try out.

  * Connect this to your favourite synth in the Daisy Seed source tree. What 
    will that sound and feel like?
  * Suppose you drive this with a nice sounding guitar with overdrive and
    effects. What are the background sound effects you can generate driven by the
    by this tracker?
  * What if you do not use it for a sound source but instead for controlling a
    linear filter's parameters with the output of the tracker?
  * What about using it for a harmonizer?


## Hothouse controls

| CONTROL | DESCRIPTION | NOTES |
|-|-|-|
| KNOB 1 | Trigger level       |  |
| KNOB 2 | Dry volume          |  |
| KNOB 3 | Wet volume          |  |
| KNOB 4 | Compression level   |  |
| KNOB 5 | Unused              |  |
| KNOB 6 | Unused              |  |
| SWITCH 3 | Unused            |  |
| SWITCH 1 | Unused            |  |
| SWITCH 2 | Unused            |  |
| FOOTSWITCH 1 | Unused        |  |
| FOOTSWITCH 2 | Bypass        |  |
