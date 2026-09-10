# MaxMuse

MaxMuse brings [Lab Streaming Layer](https://labstreaminglayer.org) streams into Max. It was made for the Muse 2 EEG headband but works with any numeric LSL stream.

| Object | Output |
| --- | --- |
| `MaxMuse.receiver~` | one **signal** outlet per channel, resampled to the audio rate in real time |
| `MaxMuse.receiver` | one **float** outlet per channel, at the stream's own rate |

## Installation

Download `MaxMuse.zip` from the [releases](https://github.com/lched/MaxMuse/releases), unzip it into your Max `Packages` folder (`~/Documents/Max 9/Packages` or `Max 8/Packages`) and restart Max. The package contains universal macOS externals (Apple Silicon + Intel, macOS 10.15+) and Windows x64 externals. LSL is built in: nothing else to install.

On macOS the externals are ad-hoc signed but not notarised, so Gatekeeper quarantines them after a download. Clear the flag once:

    xattr -dr com.apple.quarantine ~/Documents/Max\ 9/Packages/MaxMuse

## Usage

Start a streamer first, e.g. [muselsl](https://github.com/alexandrebarachant/muse-lsl):

    muselsl stream --ppg --acc --gyro

Each Muse sensor is a separate LSL stream, so use one object per stream:

    [MaxMuse.receiver~ type EEG 5]    TP9, AF7, AF8, TP10, Right AUX @ 256 Hz
    [MaxMuse.receiver~ type PPG 3]    photoplethysmography @ 64 Hz
    [MaxMuse.receiver~ type ACC 3]    accelerometer x y z @ 52 Hz
    [MaxMuse.receiver~ type GYRO 3]   gyroscope x y z @ 52 Hz

Arguments are `property value [channels]`. The property can be any LSL field (`type`, `name`, `source_id`, `hostname`…); a single word is a shorthand for `type` (`[MaxMuse.receiver~ PPG 3]`). Stream types depend on your streamer: send `scan` to any MaxMuse object to list every stream on the network in the Max console.

The trailing channel count is optional but recommended. With it, the object is created immediately (with that many outlets) and connects whenever the stream appears, so patches can be opened before the headband is streaming. Without it, the object looks for the stream for up to 2 s and fails to instantiate if it is not there. In both cases the object follows the stream if it drops out and comes back.

### MaxMuse.receiver~

Messages: `status` (report stream, buffer, clock correction, dropouts in the console), `reset` (resynchronise to the live stream), `scan`.

Attributes:

- `@latency <ms>`: jitter-buffer depth. `0` (default) sizes it automatically from the observed Bluetooth packet timing (about 80 ms for Muse EEG).
- `@interp sinc | cubic | linear`: interpolation. `sinc` (default) is a 16-tap Kaiser-windowed sinc with images more than 75 dB down; `cubic` and `linear` trade quality for less look-ahead (sinc adds 8 input samples of latency: 31 ms for EEG, 150 ms for the 52 Hz streams).
- `@fillgaps 0 | 1`: conceal lost Bluetooth packets, detected with the LSL timestamps (default on).

How the resampling works: the headband's clock and the audio interface's clock never agree exactly, and Bluetooth delivers samples in bursts (12-sample packets for EEG). A fixed resampling ratio therefore makes any buffer slowly drift until it over- or under-flows, which is heard as periodic clicks. `MaxMuse.receiver~` keeps a small jitter buffer and continuously adjusts the ratio with a PI controller, so the output follows the device clock. On a dropout it holds the last value instead of jumping. If data arrives late in a burst, it resynchronises behind a 30 ms raised-cosine ramp. The audio thread is lock-free and allocation-free.

To get control-rate values at a steady rate, use `[MaxMuse.receiver~]` → `[snapshot~ 10]`.

### MaxMuse.receiver

Outputs every received sample as floats, right-to-left, on the scheduler thread. Samples arrive in Bluetooth-packet bursts. Messages: `status`, `flush`, `scan`.

## Building

Requirements: CMake ≥ 3.23 and a C++17 compiler, i.e. Xcode or the Command Line Tools on macOS, Visual Studio 2022 on Windows. Git is needed: liblsl fetches pugixml at configure time.

    git clone --recursive https://github.com/lched/MaxMuse.git
    cd MaxMuse
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release

The externals are written to `externals/`. On macOS the build is universal by default (`-DCMAKE_OSX_ARCHITECTURES=arm64` for a faster local build). Clone into your Max `Packages` folder, or copy the folder there, to use the build directly.

Tests (no Max SDK needed): a simulation of the resampler under Bluetooth-like jitter, clock drift, dropouts and packet loss, plus an end-to-end test with a local LSL outlet:

    cmake -S test -B build-test && cmake --build build-test && ctest --test-dir build-test --output-on-failure

GitHub Actions builds and verifies both platforms on every push. The macOS job checks the binaries are universal, link LSL statically and are signed; the Windows job checks there is no DLL dependency. The tests run on the three OSes and under Thread/Address sanitizers. Pushing a tag `vX.Y.Z` matching the version in `package-info.json.in` publishes `MaxMuse.zip` as a GitHub release.

## Acknowledgements

Built with the [Min-DevKit](https://github.com/Cycling74/min-devkit) and [liblsl](https://github.com/sccn/liblsl).
