# BowString — guitar to violin / sarangi (standalone app + plugin)

BowString listens to your guitar, tracks the note you're playing, and drives a bowed-string voice
(violin or sarangi) with slow bow swells, glide, delayed vibrato, bow noise and body resonances.
After that it runs through a pedal chain: Gate → Comp → Drive → EQ → Chorus → Delay → Reverb → Output.

## 1. Get the app

BowString runs **on its own as a standalone app** (no DAW needed), and also as a plugin
(AU for Logic, VST3 for FL Studio, Ableton, Reaper, Cubase and others).

### Option A: download ready-made builds (no Xcode or Visual Studio needed)

1. Create a free GitHub account and make a new repository.
2. Upload everything in this folder, **including the hidden `.github` folder** (or push it with Git).
3. Open the repository's **Actions** tab. The "Build BowString" workflow runs automatically (about 10–15 minutes).
4. When it shows a green tick, open the run and download, under "Artifacts":
   - **BowString-macOS**: the Mac app, the AU for Logic, and the VST3
   - **BowString-Windows**: `BowString.exe` and the VST3

### Option B: build it yourself

**Mac** (needs Xcode and `brew install cmake`):

```bash
cmake -B build -G Xcode
cmake --build build --config Release --target BowString_Standalone BowString_AU
```

The app ends up in `build/BowString_artefacts/Release/Standalone/BowString.app`, and the AU
is copied into Logic's plug-in folder automatically.

**Windows** (needs Visual Studio 2022 with "Desktop development with C++"; open the
Developer Command Prompt **as Administrator**):

```
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release --target BowString_Standalone BowString_VST3
```

The app ends up in `build\BowString_artefacts\Release\Standalone\BowString.exe`.

## 2. Using the standalone app

1. Plug your guitar into your audio interface and connect headphones or speakers to the interface.
2. Open BowString and click **Options → Audio/MIDI Settings** at the top of the window.
3. Choose your interface as both input and output, enable the input channel your guitar is on,
   and set the buffer size to 128 samples or lower.
   - On Windows, pick the **ASIO** driver type for low latency.
4. If you see "Audio input is muted to avoid feedback loop", untick **Mute audio input**.
   (Use headphones if you're on laptop speakers, or it may howl.)
5. On Mac, allow microphone access the first time it asks. That is how macOS lets the app hear the guitar input.

**First launch warnings** (the builds aren't signed with a paid developer certificate):
- Mac: right-click `BowString.app` → **Open** → Open. If it still refuses, run `xattr -cr BowString.app` in Terminal.
- Windows: if SmartScreen appears, click **More info → Run anyway**.

### Using it as a plugin in Logic

1. Logic → Settings → Plug-in Manager → find **Ali Hassan → BowString** → Reset & Rescan Selection if needed.
2. Create an Audio track with your guitar input and turn on Input Monitoring.
3. Audio FX slot → Audio Units → Ali Hassan → **BowString**.

## 3. Playing tips

- It's **monophonic**: play single notes, not chords. Let one note stop before the next, or slide.
- Use the **neck pickup** with the tone knob rolled back a little. Cleaner input = more stable tracking.
- If notes cut out or flicker, lower **Gate → Threshold**. If it triggers on string noise, raise it.
- Violin sits higher than guitar: try **Octave +1** for real violin range.
- For sarangi: Instrument = Sarangi, set **Sarangi key** to your song's Sa, turn **Sympathetic** up (40–70%),
  **Glide** up (150–300 ms) and slide into notes.
- **Blend** = 100% is pure bowed voice; lower it to mix in your real guitar (with its pick attack removed).
- **Attack** is how slowly the bow swells in. **Dynamics** = how much your picking strength controls volume.

## Controls

| Section | Controls |
|---|---|
| Instrument | Instrument (Violin/Sarangi), Sarangi key, Octave, Blend, Bright, Bow noise, Sympathetic |
| Bowing | Attack, Release, Dynamics, Glide, Vib rate, Vib depth |
| Gate | Threshold |
| Comp | Sustain |
| Drive | Drive, Level |
| EQ | Bass (120 Hz shelf), Mid (900 Hz), Treble (3.5 kHz shelf) |
| Chorus | Rate, Depth, Mix |
| Delay | Time, Feedback, Mix (ping-pong stereo) |
| Reverb | Size, Damping, Mix |
| Output | Volume |

All controls are automatable in Logic and saved with your project.

## Files

- `Source/BowDSP.h` — pitch tracker (YIN), bowed voice, sympathetic strings, delay
- `Source/PluginProcessor.*` — parameters and the full signal chain
- `Source/PluginEditor.*` — the pedalboard interface
