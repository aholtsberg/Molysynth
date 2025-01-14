# Molysynth

Contributed by Anders Holtsberg \<<anders.holtsberg@gmail.com>\>


# Introduction

This is an advanced guitar pitch tracker, plus a small synth. The whole idea
is to develop a one file C-code guitar monophonic pitch tracker suitable for 
use in DSP real-time hardware. One of the main ideas is to be
able to develop it off-line, reading a WAV file and producing a WAV file. Then
compile it and upload it to Hothouse Daisy Seed without any hazzle. 

The design choice here is to develop it inside HothouseExamples source tree.
But this does _not_ mean that you have to have any hardware at all, you can just
enjoy developing algorithms of-line.

   * The __src__ library contains the source code in the `molysynth.cpp` file
     as usual for someone that comes from Daisy Seed development.
   * The __dev__ library contains code for working off-line with WAV files.
     It also contains a Jupyter Notebook containing Julia code. 
     It serves as a starting point if
     you want to analyze WAV that go into or out of Molysynth. Copy the
     file before using it because you do _not_ want to check in a ton of data, so
     do _not_ track your own file with git. 
   * The __dat__ library contains a WAV file to get you started. I use Garage Band
     to record my own WAV file to ecperiment with. 

Note that the Daisy Seed specific code is actually ifdeffed away so that porting
this to another environment should be a breeze. Note also that I hate C++ so I kept 
that part only in the Daisy Seed dependent part.

Anyway, just download this to your machine and go into __dev__ and do ´make test´
and listen and enjoy. 


# Background

The goal is to track a lot better than the competition, ie 

 * __Electro-Harmonix mono__
 * __Keeley synth 1__

Note that __Mooer E7__ is _not_ a synth despite the name, not in the meaning 
used here,
since it does not use a real pitch tracker, probably only FFT frequency analyzer.
 
I have yet to analyze __Boss SY-1__ to find out what is actually going on inside it.

Also, harmonizers, like __Mooer Harmony X2__ use a pitch tracker.

One difficulty is that in a true monophonic tracker it is difficult to 
figure out a tone if the last tone is still sounding on another string on the guitar, 
and that problem is the research main point for me. 
Anyway, you can use this code as a framework for trying out your own ideas.

At some point I may add FFT to play with polyphonic output too, but that is for later.


### Hothouse controls

| CONTROL | DESCRIPTION | NOTES |
|-|-|-|
| KNOB 1 | DryWet mix |  |
| KNOB 2 | Sensivity |  |
| KNOB 3 | Attack |  |
| KNOB 4 | Decay |  |
| KNOB 5 | Sustain |  |
| KNOB 6 | Release |  |
| SWITCH 1 | Wave form | **SQUARE** - <br/>**TRIANGLE** - <br/>**SINE** -  |
| SWITCH 2 | Autotune | **AUTOTUNE** - <br/>**OFF** - <br/>**DRY ATTACK** -  |
| SWITCH 3 | ADSR mix | **ADSR** - <br/>**ATTACK ONLY** - <br/>**INSTRUMENT** - |
| FOOTSWITCH 1 | Unused |  |
| FOOTSWITCH 2 | Bypass | Molysynth |
