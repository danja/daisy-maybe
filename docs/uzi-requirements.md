# Uzi Spectral Phaser

Uzi will be a new subproject using the same core DSP and UI system as slime.

Code will be very modular to help maintenance, separation of concerns best practices etc. Comment should appear in code only when the operation isn't obvious. 

## DSP

The system will be two channel.
The FFT/IFFT of slime (Hann window will be used to manipulate the signal in the frequency domain.
Low-end frequencies under 250Hz will be preserved.
The FFT section will be preceded by a distortion section like that of resonators, with wavefolding followed by soft/hard clipping.

### Controls

The values from CV1 and Knob1 will control distance between each spectral notch. Log law will be used to keep things moderately harmonically related.

The values from CV2 and Knob2 will control phase offset of the spectral shape

## UI

Menu system will follow the design of docs/menu-system.md

## Master

* Mix (dry/wet %)
* LFO Depth
* LFO Frequency

## Distortion

* Wave
* Overdrive (soft/hard clipping %)

## FFT

* Crossover (in the frequency domain, channel 1 mixed into channel 2, channel 2 mixed into channel 1)
* Blur (notch sharpness)
* Spectral bin rounding
* Block size


