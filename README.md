# SUNRIZE

A dreamy pad synth (VST3 + AU for Mac).

## Build (GitHub)
1. Create a new GitHub repo and upload everything in this folder (including the hidden `.github` folder).
2. Open the **Actions** tab, where the build "Build SUNRIZE (Mac)" starts automatically (takes about 10–15 min).
3. When it's green, download the **SUNRIZE-mac** artifact at the bottom of the run.

## Install
```
sudo cp -R SUNRIZE.vst3 /Library/Audio/Plug-Ins/VST3/
sudo cp -R SUNRIZE.component /Library/Audio/Plug-Ins/Components/
sudo xattr -cr /Library/Audio/Plug-Ins/VST3/SUNRIZE.vst3
sudo xattr -cr /Library/Audio/Plug-Ins/Components/SUNRIZE.component
```
Then rescan plugins in Ableton. SUNRIZE is under Instruments.

## Controls
- **Big knob (sun):** filter, from night to noon. Click/drag vertically. Double-click resets.
- **TONE** soft to bright layer + resonance. **WEATHER** tape wow + filter "clouds". **WIDTH** detune width + slow attack.
- **BIRDS** high chirps from the held notes. **BLOOM** reverb + shimmer. **AFTERGLOW** echo + release time.
- **DRIFT** chorus. **DUST** vinyl hiss/crackle. **LIGHT** breathing high layer + air. **DROPS** water droplets.
- **OUTPUT** volume. **i** opens the info window. Arrows switch between the 10 sounds.

Font: IBM Plex Mono (SIL Open Font License).
