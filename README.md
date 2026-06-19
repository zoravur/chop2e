# Chop2E

This is a sample pad written in C using raylib and coreaudio, inspired by teenage engineering's PO-20. It only works on macOS.

## Usage

To set your samples, create 16 wav files numbered 1.wav, 2.wav, ..., 9.wav, 10.wav, ..., 16.wav, to correspond to each 
of the samples you'd like to use. They must be mono, and we assume a sample rate of 48kHz.

The samples are on the left hand side -- you press a key and it plays the sample. The right hand side is for the controls, 
and the functionality is labelled. For full details, see the source code.

## Building

There is a Makefile. Just run `make` in the root directory (you might have to create `build/`). Please file an issue if there are issues, I'm happy to help.


Here's a video of a song (with the sampling copied from Kanye West's Power) that I made using the sample pad.

TODO(add video)
