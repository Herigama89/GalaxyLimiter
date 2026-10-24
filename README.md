# Galaxy Limiter

A mastering lookahead limiter (VST3, AU, Standalone) built with JUCE 8, with 10 selectable limiter styles.

## Features

- Lookahead brickwall limiter with sample-accurate ceiling (0–10 ms lookahead)
- 10 limiting styles, each a different voicing of attack shape, dual-stage release, knee, transient clipping and harmonic colour
- Oversampling: Off / 2x / 4x / 8x / 16x (linear-phase FIR)
- True-peak mode (forces at least 4x and keeps 0.1 dB headroom for inter-sample peaks)
- Gain, output ceiling, attack, release, lookahead, stereo link
- Unity-gain monitoring (hear the tone without the loudness jump)
- Delta monitoring (hear only what the limiter removes)
- TPDF dither to 16 or 24 bit
- Scrolling input / output / gain-reduction history display
- Output peak meters and EBU R128 loudness (momentary, short-term, integrated)
- Latency is reported to the host automatically (lookahead + oversampling filters)

## The 10 styles

| Style | Character |
|---|---|
| Transparent | Clean, smooth attack, program-dependent release |
| Punchy | Shorter attack; transients pushed into a soft clipper |
| Dynamic | Strong program-dependent release, keeps groove |
| Aggressive | Fast, loud, heavy transient clipping |
| Modern | Balanced dual-stage release for loud clean masters |
| Safe | Slow and careful; minimal distortion |
| Bus Glue | Wide soft knee, slow release, compressor-like |
| Vari-Mu Tube | Wide knee, slow release, 2nd-harmonic warmth |
| FET Analog | Very fast attack, odd-harmonic bite |
| Opto | Release slows the harder you push |

All style voicings live in `Source/LimiterStyles.h`. Every number is commented,
so you can tune them or add more styles (increase `kNumStyles`).

## Building

Requirements: CMake 3.22+, Git, and a C++17 compiler
(Xcode 14+ on macOS, Visual Studio 2022 on Windows). JUCE is downloaded automatically.

### macOS (VST3 + AU)

```bash
cmake -B build -G Xcode
cmake --build build --config Release
```

### Windows (VST3)

```bash
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

With `COPY_PLUGIN_AFTER_BUILD` on, the plugins are copied to:

- macOS: `~/Library/Audio/Plug-Ins/VST3` and `~/Library/Audio/Plug-Ins/Components`
- Windows: `C:\Program Files\Common Files\VST3` (run the build as administrator, or turn the option off and copy manually)

After the first AU install on macOS you may need to run `killall -9 AudioComponentRegistrar`
or restart your DAW so Logic rescans.

### Before shipping

- Validate with [pluginval](https://github.com/Tracktion/pluginval) at strictness 10.
- Replace `COMPANY_NAME`, `BUNDLE_ID`, `PLUGIN_MANUFACTURER_CODE` and `PLUGIN_CODE` in `CMakeLists.txt` with your own unique values.
- To sell it, VST3 is covered by Steinberg's open licence (register as a VST3 developer); JUCE needs a commercial licence unless you release under AGPLv3.
- AU plugins must be code-signed and notarised for distribution on macOS.

## Signal flow

```
input gain -> [upsample] -> gain computer (soft knee, stereo link)
  -> lookahead hold -> dual-stage release -> attack smoothing
  -> x delayed audio -> style colour -> DC block -> output clipper
  -> [downsample] -> ceiling safety clip -> meters -> unity/delta -> dither
```

## Project layout

```
CMakeLists.txt
Source/
  LimiterStyles.h     10 style voicings
  LimiterEngine.h     limiter DSP (no JUCE dependency, unit-testable)
  LoudnessMeter.h     BS.1770 / R128 loudness
  PluginProcessor.*   parameters, oversampling, routing, state
  PluginEditor.*      UI
```

## Known limitations / ideas for next steps

- Changing Lookahead, Oversampling or True Peak resets the limiter state and changes latency; some hosts only pick up new latency on stop/start.
- Dragging Attack re-initialises the smoothing window, which can click on audio that is being limited hard.
- Possible additions: separate transient/release link controls, preset browser, loudness target helper, resizable UI, noise-shaped dither.

## Automatic builds (GitHub Actions)

Every push to `main` builds the plugins on a GitHub-hosted Mac (`.github/workflows/build-mac.yml`).
The installer appears under the run's **Artifacts** on the Actions tab.

To publish a permanent download, bump `VERSION` in `CMakeLists.txt`, then push a tag:

```bash
git tag v1.1.0
git push origin v1.1.0
```

The `.pkg`, a zip of the raw plugins and the manual are attached to a new entry on the **Releases** page.
