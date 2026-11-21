# RecordAudio

Companion Geode mod for capturing Geometry Dash FMOD output for the Record recorder.

Target: Geode 6.0.0-beta.1, Geometry Dash 2.2091, Android arm64-v8a.

The DSP tap is designed to copy game PCM without blocking the FMOD mixer thread. The shared-memory bridge is documented in `src/AudioBridge.hpp`.
