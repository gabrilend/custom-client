# 110f — More Render Threads

*Parent: [110](./110-the-ceramic-shape.md).*

## Current behavior

One window, one OpenGL context, drawn by the main thread.

## Intended behavior

A render thread per context, each with its own view and mailbox:

- **A windowless render thread** drawing pictures into files: the map
  browser's previews of maps and models, world-edit-to-execute W04's
  comparisons, the test scenes (109). Its context draws into an
  offscreen surface (EGL, needing no window).
- **A second view of the world** (picture in picture): another camera
  on the same state.

Contexts are made in one **share group**, so textures and buffers
uploaded once serve all of them; objects that are not shared (vertex
arrays, framebuffers) are made per context.

Two limits the work must face:

- **raylib's drawing layer (rlgl) keeps one global state**: one batch,
  one current shader, one matrix stack, for the whole process. Two
  threads drawing through it at once would corrupt each other. So either
  the extra render threads draw through a small layer of this client's
  own that keeps its state per context, or raylib is patched so rlgl's
  state is per context (kept as a reversible patch, not a fork).
- **The graphics card runs one context's work at a time.** Several
  contexts let several threads prepare drawing commands at once; they do
  not make the card itself faster. A second view drawn into a picture
  within one context (render to texture) costs no second thread and no
  second context.

So the second view is built both ways and measured; the windowless
thread is built regardless, because it must run while the window draws.

## Suggested implementation steps

1. The windowless context (EGL) in the same share group; a test that
   draws a model to a file while the window draws.
2. The per-context drawing state: this client's own layer, or the rlgl
   patch; decided with the owner after 1 shows what is needed.
3. The second view both ways; the measurements written here.

## Acceptance

- A windowless picture drawn while the window runs, in one process.
- The second view's two ways measured and the choice recorded.

## Open questions

1. Our own small drawing layer, or a patched rlgl, for the extra contexts?

## Related

- [109](./109-test-scenes-client-half.md), [110b](./110b-the-window-render-thread.md),
  [110d](./110d-the-asset-table-and-loading-stations.md); world-edit-to-execute W04.
