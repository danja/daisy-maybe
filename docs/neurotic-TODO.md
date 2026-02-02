# Neurotic TODO

## General

If a value of a parameter in the menu is zero, display zero - often this is left blank.

Functionality should not be duplicated between CV/Knob controls (one pair of variables) and rotary controller values (another pair or more variables).

Feel free to add more rotary controller/menu parameters if there are any obvious places.

LFO Rate value is not displayed on the menu - presumably a number formatting error. Check how resonators handles decimal values.

## Algorithms

### CrossRes

Swap the Mass and Tension controls. Pitch should always be on the first CV/knob.
Add a Resonance level control to the menu/rotary control.

### Braid

This sounds very crunchy. Is there any way of making it cleaner in the FFT?

### Binaural

Spin should be handled by the main LFO controls.

### Formant

Add a Resonance control to the rotary controller/menu.

### Harmonic

Output level is very low compared to other algorithms, and with crunchy distortion.

### PhaseLoom

Knobs/CV controls have no effect. Its not clear what this is meant to be doing.

### Smear

Current both CV1/Knob1 and CV2/Knob2 influence pitch. Pitch should only be on the first, resonance on the second.

## New Algorithms

### Compressor/Expander

CV1/Knob1 should control the level of signal compression, CV2/Knob2 control the overall time constant.
The rotary controller/menu should have Attack and Decay time values which are scaled by the overall time constant.

### Pitch Shifter

CV1/Knob1 will be a musical pitch shift up/down on the input signal for each channel as an offset. CV2/Knob2 will scale the offset value.
