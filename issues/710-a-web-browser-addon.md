# 710 — A Web Browser Addon

*Decided 2026-11-04. Came from: world-edit-to-execute issue 1001 (the
catalogue of maps and models), whose page must open in an ordinary web
browser, in that project's launcher, and in this client.*

## Current behavior

The catalogue of maps and models was planned (2026-10-07) as an HTML page
this client draws into a texture and shows on a flat pane (110f), its
moving parts (a search that narrows as one types, download progress, a box
where a model turns and plays its animations) scripted in Lua, inside this
client only. There is no addon system yet (Phase 7 not started), and no
way to show a web page.

## Intended behavior

The owner (2026-11-04): "Let's do javascript, and run a little browser
inside the client as an addon. We need feature parity with WoW's client,
so... we need addons, and plenty of them have created web-browsers inside
the client." Asked about Fengari (a Lua interpreter written in
JavaScript): "no to fengari."

So the catalogue is one ordinary web page with its moving parts in
JavaScript. An ordinary web browser opens it as it is. Inside this client
it opens in a **web browser addon**: an addon window with an address,
back and forward, and the page drawn into it, made the way any addon is
made (Phase 7: XML frames and Lua), so it is also the first test that
addons can bring whole programs into the client.

What the addon needs from this client that a 3.3.5a addon never had:

- **A browser frame**: a frame kind that lays out and draws a web page
  (HTML, CSS) into a texture (110f) and runs its JavaScript, with mouse
  and keyboard passed through.
- **Network access** for that frame only, to the addresses the user opens.
- **The live model box**: the page names a model; this client draws it
  into a texture (110f) inside the page, turned by the mouse.

These are additions to the addon interface; nothing an original addon
uses changes ("WoW addons must work as-is", docs/006).

## Suggested implementation steps

1. Choose the full browser engine (open question 1, answered): one that
   a C program can embed and that draws a page into memory rather than
   its own window, so the page lands in a texture; record the choice and
   its size in docs/002. world-edit-to-execute's launcher embeds the same
   one.
2. The browser frame kind: page into texture, input passed through.
3. The addon: window, address bar, back and forward, saved places (707).
4. The live model box.
5. Open world-edit-to-execute's catalogue page in it; the same page in an
   ordinary web browser shows the same lists.

## Acceptance

- The catalogue page opens in the addon and in an ordinary web browser;
  its search narrows as one types in both.
- A model in the page turns and animates inside the client.
- An original 3.3.5a addon loads unchanged beside it.

## Open questions

1. ~~**Which page engine?**~~ Answered 2026-11-04, the owner: "full
   browser engine." So the addon runs any website, not only pages written
   for it; step 1 chooses which full engine can be embedded in a C
   program and draw into a texture. The question as asked: a full browser
   engine is large (tens of megabytes) and runs everything; a small
   layout library plus a small JavaScript engine is light but runs only
   pages written for it. Which?
2. ~~**Addons that made browsers in the original client.**~~ Answered
   2026-11-04: there were none to copy. The owner: "oh, I was wrong.
   Well, let's build one then." The question as asked: as far as this
   project knows, a 3.3.5a addon has no network access and no page
   engine; which in-game browsers did the owner mean?

The page is shown, never handed off: neither this client nor
world-edit-to-execute's launcher opens the user's own web browser. The
owner: "the user could visit the page on their browser, or view it in
their client / launcher. the client / launcher don't open the browser."

## Related

- Blocked by: Phase 7 (the Lua environment 701, 702-705, the addon loader
  706), [110f](./110f-views-drawn-into-textures.md) (views into textures).
- world-edit-to-execute issue 1001 (the catalogue page), its
  `docs/wow-client-bridge.md`.
- [docs/006](../docs/006-ui-and-addons.md) (addons as-is).
