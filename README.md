# RecordAudio

Companion Geode mod for **Record**.

- Target: Geode 6.0.0-beta.1
- Geometry Dash: 2.2091
- Platform: Android arm64-v8a
- Captures the game's FMOD master output (not the microphone).
- Uses a lock-free PCM ring in a shared file. The FMOD callback performs no file I/O, allocations, or mutex locking.

Shared ring:
`/storage/emulated/0/Android/media/com.geode.launcher/game/renders/RecordAudio.shared`

The companion currently provides the FMOD PCM capture endpoint. Record still needs its AAC input/muxer consumer before the captured PCM becomes an audio track inside the MP4. Until that consumer is merged and tested, this mod alone does not add sound to Record.mp4.
