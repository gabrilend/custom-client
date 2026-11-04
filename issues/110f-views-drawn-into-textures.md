# 110f — Views Drawn into Textures

*Parent: [110](./110-the-ceramic-shape.md). Was "more render threads"
until 2026-10-07; renamed when the owner chose textures: "I guess
displaying it on a texture is fine."*

## Current behavior

*Paused with its parent, 2026-10-07: blocked until soramech has timers
and standing tasks ([110](./110-the-ceramic-shape.md)).*

One window, one OpenGL context, drawn by the main thread. Pictures of a
scene for tests are taken from the window (`scripts/run --shot`).

## Intended behavior

Every extra view is drawn by the window's one render thread into a
texture (a framebuffer the card draws into instead of the screen), each
view with its own camera and its own mailbox:

- **A second view of the world** (picture in picture): the texture is
  then drawn onto the screen as a pane.
- **Pictures for files** (the map browser's previews of maps and models,
  world-edit-to-execute W04's comparisons, the test scenes of 109): the
  texture's pixels are read back and handed to a station that writes the
  file, so the render thread never waits on the disk.
- **The map browser's HTML page** (since 2026-11-04 opened by the web
  browser addon, [710](./710-a-web-browser-addon.md)) arrives the same way: drawn into a
  texture and shown on a pane.

A process with no visible window (a tool making previews) opens a hidden
window (raylib's hidden-window flag) for its one context; nothing else
changes.

Why one context: raylib's drawing layer (rlgl) keeps one global state
for the whole process, so a second drawing thread would need a layer of
our own or a patched raylib; and the card runs one context's work at a
time, so a second context buys parallel command preparation, not a
faster card. Textures need neither. More render threads stay possible
later, if a measurement shows the one render thread's command
preparation is what limits a frame.

## Suggested implementation steps

1. A view: camera, mailbox (110c), target texture, size, and how often
   it is drawn (every frame, or on request for pictures).
2. The pane: a texture drawn into the interface at a place and size.
3. Pictures: read back the texture, hand the pixels to a writing station.
4. A test: a model drawn to a file while the window draws the world.

## Acceptance

- A second view shown as a pane while the world draws.
- A picture written to a file during a running session, the same pixels
  as one taken alone.
- The frame-time cost of each extra view measured and written here.

## Related

- [109](./109-test-scenes-client-half.md), [110b](./110b-the-window-render-thread.md),
  [110d](./110d-the-asset-table-and-loading-stations.md); world-edit-to-execute W04, 609.
