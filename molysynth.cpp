// Molysynth for Hothouse DIY DSP Platform
// Copyright (C) 2024 Anders Holtsberg <anders.holtsberg@gmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

// ### Uncomment if IntelliSense can't resolve DaisySP-LGPL classes ###
// #include "daisysp-lgpl.h"

// ======================================= Yes, we drag in the C file here! ===
extern "C" {
#include "src/molysynth.c"
}
// ============================================================================

#include "daisysp.h"
#include "hothouse.h"

using clevelandmusicco::Hothouse;
using daisy::AudioHandle;
using daisy::Led;
using daisy::SaiHandle;

Hothouse hw;
Led led_bypass;
bool bypass = true;
int mytimer = 10;


void AudioCallback(
  AudioHandle::InputBuffer in, 
  AudioHandle::OutputBuffer out,
  size_t size)
{
  if (--mytimer < 0) mytimer = 0;

  // Bypass
  if (bypass) {
    for (size_t i = 0; i < size; i++) {
      out[1][i] = out[0][i] = in[0][i];
    }
    return;
  }

  // The real stuff
  moly_addtobuf(in[0], size);
  moly_synth(in[0], out[0], size);
  for (size_t i = 0; i < size; i++) {
    out[1][i] = out[0][i];
  }
}


int main() {

  // Init
  hw.Init();
  hw.SetAudioBlockSize(48);
  hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
  moly_init(48000);

  // Callback
  hw.StartAdc();
  hw.StartAudio(AudioCallback);

  while (true) {
    hw.ProcessAllControls();
    moly_set(MOLY_TRIGLEVEL, 0.1 * hw.knobs[0].Process());
    moly_set(MOLY_DRYVOLUME, 2.0 * hw.knobs[1].Process());
    moly_set(MOLY_WETVOLUME, hw.knobs[2].Process());
    moly_set(MOLY_COMPLEVEL, hw.knobs[3].Process());
    bypass ^= hw.switches[Hothouse::FOOTSWITCH_2].RisingEdge();
    led_bypass.Set(bypass ? 0.0f : 1.0f);
    led_bypass.Update();

    // Call System::ResetToBootloader() if FOOTSWITCH_1 is pressed for 2 seconds
    hw.CheckResetToBootloader();

    // Main workload; we call this every 10*48 samples, ie 100 times per second
    hw.DelayMs(mytimer);
    mytimer = 10;
    moly_analyze();
  }

  return 0;
}
