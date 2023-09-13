# SFOTool for TinySoundFont
A tool to create heavily compressed .SFO files from .SF2 SoundFont files.

## Purpose
SFO files are just regular SoundFont v2 files with the entire block of raw PCM samples replaced
with a single Ogg Vorbis compressed stream. Unlike .sf3 files, which can have every separate font
sample compressed individually, this will compress the entire sound data as if it were a single
sample. This results in much higher compression than processing samples individually but also
higher loss of quality.

## Usage Help
```sh
sfotool <SF2/SFO>: Show type of sample stream contained (PCM or OGG)
sfotool <SF2> <WAV>: Dump PCM sample stream to .WAV file
sfotool <SFO> <OGG>: Dump OGG sample stream to .OGG file
sfotool <SF2/SFO> <WAV> <SF2>: Write new .SF2 soundfont file using PCM sample stream from .WAV file
sfotool <SF2/SFO> <OGG> <SFO>: Write new .SFO soundfont file using OGG sample stream from .OGG file
```

## Making a .SFO file from a .SF2 file
1. Dump the PCM data of a .SF2 file to a .WAV file  
   `sfotool <SF2> <WAV>`
2. Compress the .WAV file to .OGG (i.e. with [Audacity](https://www.audacityteam.org/download/legacy-windows/))  
   Make sure to choose the desired compression quality level
3. Build the .SFO file from the .SF2 file and the new .OGG  
   `sfotool <SF2> <OGG> <SFO>`

# License
SFOTool is available under the [Unlicense](http://unlicense.org/) (public domain).
