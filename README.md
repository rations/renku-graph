# Renku Graph

> This repository is the source of the `renkugraph` package for RenkuOS. Built
> packages are on its [Releases](https://github.com/rations/renku-graph/releases) page, and
> [renku-audio](https://github.com/rations/renku-audio) lists the whole audio stack.

An audio and MIDI patchbay for RenkuOS. It brings what Cortex, PatchBay and
the Media preferences do for routing into one native window:

- Every Media Kit audio node and every MIDI endpoint is a box on one canvas.
  A sound card is shown as two boxes, **capture** on the left and
  **playback** on the right, so the signal flows from left to right.
- Every jack is labelled with what it is: "Capture 1+2", "Playback 3+4",
  "Free Input", "Mixer Output 2", "MIDI". Hover over a jack for its raw name
  and format.
- **Connect** by dragging a wire from a jack to a jack. **Disconnect** by
  right-clicking the wire, or click a connected jack for a list of its
  wires and pick the one to remove (easier on a touchpad).
- Rename any box or jack (right-click). The names are kept between sessions.
- Double-click a box to open its controls (its parameter web).
- **Settings** (File ▸ Settings…): default audio output, output channel and
  input, and per sound card the sample rates and the buffer size, plus
  Restart media services.

The buffer size setting is offered by multi_audio for USB audio interfaces in
RenkuOS versions that include it; elsewhere the control is greyed out.

## Notes on the Media Kit

- A Media Kit jack carries a whole bus (for example a stereo pair), not one
  channel, and takes one wire. A producer that can feed several consumers
  offers a new free jack once one is used (the system mixer's
  "Free Output").
- MIDI jacks take any number of wires.

## Build

On RenkuOS (x86_64):

- From source: `./build-from-source.sh`, or just `make`.
- Package: `packaging/make-hpkg.sh` builds `renkugraph-0.1-3-x86_64.hpkg`;
  install it with `pkgman install ./renkugraph-*.hpkg`.

## License

MIT, see [LICENSE](LICENSE).
