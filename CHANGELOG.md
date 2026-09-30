# Changelog

All notable changes to WetEQ are recorded here. The published notes for each
release are on the [Releases page](https://github.com/yonie/WetEQ/releases);
this file is the portable copy that travels with the source.

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and
this project uses [semantic versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.2.1] - 2026-09-30

### Fixed
- Linux: no more crash when the editor opens in Carla, or in any host that hands its event loop over through the plug-in window.
- UI Zoom now sits directly in the right-click menu, so it also shows in hosts that leave out a plug-in's submenus (Studio One).

### Changed
- The window no longer shows resize arrows it could not act on; UI Zoom changes the size.
- The sound and saved settings are unchanged.

## [1.2.0] - 2026-09-26

### Added
- An Audio Unit for Logic and GarageBand, next to the VST3.
- Mono tracks: all four layouts (mono or stereo in, mono or stereo out). A mono
  input feeds both sides; a mono output is the average of left and right.

### Changed
- The macOS builds are signed and notarised by Apple.
- The sound is unchanged, and saved projects reload with the same settings.

## [1.1.1] - 2026-09-04

### Added
- Power user mode: hold Shift while dragging, scrolling or using the arrow keys
  for four times the resolution - 0.469 dB on a band, 0.625 dB on GAIN.

### Fixed
- Saved state now carries the detent count, so a later change of grid cannot
  move a stored setting. v1.1.0 read v1.0.0 sessions on the wrong grid and moved
  every control on the panel.

## [1.1.0] - 2026-09-01

### Changed
- Seventeen positions per knob instead of nine, on user feedback, so the
  smallest move on a band is 1.875 dB rather than 3.75 dB and GAIN steps in
  2.5 dB.

## [1.0.0] - 2026-08-30

Initial release.

### Added
- Four bands plus high-pass, low-pass and input drive; eleven stepped controls.
- Input and output peak metering.
- Full VST3 parameter automation.
- Modelled as a circuit: state-variable stages in topology-preserving form,
  saturation inside the filter loop where the op-amp is, and bandwidth derived
  from the boost/cut pot damping a gyrator tank.
- -40 dB channel bleed distributed across all six stages, and 2.5% component
  tolerance per channel.
