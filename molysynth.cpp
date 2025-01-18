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

// Bypass vars
Led led_bypass;
bool bypass = true;


void AudioCallback(
  AudioHandle::InputBuffer in, 
  AudioHandle::OutputBuffer out,
  size_t size)
{
  hw.ProcessAllControls();
  bypass ^= hw.switches[Hothouse::FOOTSWITCH_2].RisingEdge();

  // Bypass
  if (bypass) {
    for (size_t i = 0; i < size; i++) {
      out[1][i] = out[0][i] = in[0][i];
    }
    return;
  }

  // The real stuff
  moly_callback(in[0], out[0], size);
  for (size_t i = 0; i < size; i++) {
    out[1][i] = out[0][i];
  }
}


int main() {
  hw.Init();
  hw.SetAudioBlockSize(48);
  hw.SetAudioSampleRate(SaiHandle::Config::SampleRate::SAI_48KHZ);
  moly_init(48000);

  led_bypass.Init(hw.seed.GetPin(Hothouse::LED_2), false);

  hw.StartAdc();
  hw.StartAudio(AudioCallback);

  while (true) {
    hw.DelayMs(10);

    // Toggle effect bypass LED when footswitch is pressed
    led_bypass.Set(bypass ? 0.0f : 1.0f);
    led_bypass.Update();

    // Call System::ResetToBootloader() if FOOTSWITCH_1 is pressed for 2 seconds
    hw.CheckResetToBootloader();

    // Main workload
    moly_analyze();
  }
  return 0;
}