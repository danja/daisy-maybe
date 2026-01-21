# DSP

Remove reed (RD & RB).

Insert a wavefolder and overdrive (soft/hard clipping) before each resonator. 
The peak amplitude output of this section will be kept as input made up, ie. compensate for losses.

Output from resonators will go to the system audio out as well as a pair of filters which will be in circuit for feedforward/back connections.
The feedforward/back connections will connect before the channel's distortion section.

# Controllers

CV1 and Knob1 will control resonator delay time (which will correspond to 1V/oct pitch)
CV2 and Knob2 will control wavefolder depth

# UI

The menu system is already close to what is required, tweak it to follow docs/menu-system.md

There will be three top-level items : Master, Distortion and Resonators

## Master 

will contain the items, all with values expressed as a percentage :

* Dist Mix (wavefolders dry/wet)
* Res Mix (resonators dry/wet)
* Feed X-X (X output to X input via filter)
* Feed Y-Y (Y output to Y input via filter)
* Feed X-Y (x output to y input)
* Feed Y-X (y output to x input)

## Distortion

will contain :

* Folds (number of wavefolds made 1-5)
* Overdrive (% from passthrough, through tanh-style soft clipping to hard clipping)

## Resonators

will contain the items :

* ratio (of delay time, resonator Y to resonator X)
* damping X
* damping Y





