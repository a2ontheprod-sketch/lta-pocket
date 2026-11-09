# LTA // POCKET v1.0 — Start here

**Windows VST3 / JUCE project for FL Studio.**

LTA Pocket is a **mix monitoring tool**, not a mastering effect. It makes *generic, deliberately approximate* simulations of playback on a phone, laptop, portable Bluetooth speaker, earbuds, car and TV. These models are NOT measurements of a specific manufacturer or hardware, and cannot perfectly predict what a real device will sound like. Listening on the actual devices remains important.

## What works

- Cream/charcoal, coral/sage flat interface based on the approved mockup. Artwork is embedded in the VST3.
- Six playback choices: PHONE / LAPTOP / SPEAKER / EARBUDS / CAR / TV.
- **Phone subtypes:** when PHONE is already selected, click PHONE again to cycle MODERN, COMPACT and VINTAGE.
- Frequency-response filters, presence contour, frequency-dependent stereo collapse and (where appropriate) soft saturation. All process actual incoming stereo audio.
- MONO (proper stereo sum), QUIET (-16 dB listening check), four optional simulated listening environments: NONE, BUS, OUTDOOR and ROOM.
- Room has short early reflections. Bus/outdoor contain low-level synthetic environmental noise. These are rough listening conditions, not calibrated room simulations.
- Click SIMULATED / ORIGINAL below the phone waveform, or click the central album art, for smooth A/B. Levels are approximately RMS-matched (limited to 7 dB) and transitioning is click-free.
- Audio-reactive waveform from *processed* sound. The play triangle auditions the **most recent up to 12 seconds** of Master audio buffered inside the plugin, looping during preview. Press again to exit. This does NOT remotely operate FL Studio's Play button.
- Big MIX slider: 0 = unprocessed, 100 = fully simulated (processed sound). The Quiet control reduces volume regardless of Mix.
- All audio parameters saved inside the FL Studio project and eligible for host automation. Resizable GUI.
- Offline rendering passthrough whenever FL Studio reports an offline render. **Real-time exports may not be detected — disable or remove the plugin before exporting.**

## Building online using GitHub Actions

1. Download and extract `LTA_POCKET_v1_GITHUB_BUILD.zip`.
2. Create a GitHub repo, for example `lta-pocket`.
3. Upload `CMakeLists.txt`, `Source/`, `Assets/`, this README, and the workflow. Preserve directories.
4. If Actions does NOT show the workflow, click **Add file > Create new file** in the repo, name it exactly `.github/workflows/build-windows.yml`, and paste the content of the included `GITHUB_WORKFLOW_COPY_PASTE.yml`. Commit it.
5. Go to **Actions > Build LTA Pocket VST3 > Run workflow**.
6. If the build is green, open the run and download the artifact `LTA-POCKET-Windows-VST3`.
7. Extract. Copy the **entire `LTA Pocket.vst3` folder** to `C:\Program Files\Common Files\VST3`.
8. In FL Studio: **Options > Manage plugins > Find installed plugins**. Load LTA Pocket as the **last insert on the Master** while testing.

## Typical workflow

1. Open FL Studio and play a beat.
2. Choose PHONE and listen for missing low end and masked vocals.
3. Compare LAPTOP, SPEAKER and EARBUDS.
4. Toggle MONO; see whether your melody disappears.
5. Use QUIET to check levels when listening softly.
6. Try BUS or OUTDOOR for a rough noisy-world test.
7. Click the SIMULATED / ORIGINAL switch for A/B, and use MIX to dial in how much of the approximation you hear.
8. For captured audition: play the beat in FL for at least one second, then click the round play icon on the phone. FL Studio must continue to process this plugin for audition playback. If FL suspends the audio callback when stopped, resume host playback.
9. **Bypass or remove LTA Pocket before your final export**, especially when using real-time export mode. It is a monitor-only effect.

## Honest limitations and safety

- A VST3 on the Master cannot access exact hardware speaker responses or listening-room acoustics. The included device models are representative, not measured hardware calibrations.
- An effect on Master cannot reliably start/stop the host transport. The phone play button therefore replays its own short captured buffer instead.
- Quiet mode is digital attenuation, not a physically calibrated sound-pressure-level (SPL) system.
- BUS and OUTDOOR add synthetic noise that may be included in a **real-time export** unless the VST is bypassed or removed.
- The phone screen shows generic artwork and a real incoming audio waveform, not the FL Studio project title or file metadata.
- No Internet is required at runtime. The JUCE library is downloaded by GitHub during the initial build.

## Build and validation

The portable DSP has a standalone C++ test under `tests/dsp_smoke.cpp` verifying device frequency differences and finite output across all six devices and four environments. Run it using `g++ -std=c++17 -O2 tests/dsp_smoke.cpp -o dsp_smoke && ./dsp_smoke` on a local C++ compiler. The full Windows VST3 has **not been built/tested inside FL Studio** in this environment; GitHub Actions must verify it. If it fails, copy the first actual error lines from the failed step.

## License notes

JUCE has its own licensing requirements. Review JUCE's current terms before publicly distributing a compiled plugin, especially commercially. The included custom artwork is solely an LTA Pocket user-concept asset.

### On-screen bass readout
The small `LOW END ±X dB` indicator measures the change in the signal's approximate sub-130 Hz energy between input and monitored output. It is a simple low-pass RMS comparison, NOT a guarantee of phone bass audibility or subjective mix quality.
