// docs-site.js - what moves on the W client's documentation pages.
//
// For a general audience: the pages are plain files; this adds four things
// to every one of them. A light/dark switch. A filter over the contents
// list. The note box at the foot of each page, which seals the reader's note
// with the project mailbox's key and posts it straight to the mailbox. And
// the charts, drawn from numbers the builder measured and wrote into the
// page; this file only draws them, it never measures anything.
//
// The mailbox's address and key come from site-config.js, which the builder
// writes beside this file (window.W_SITE). Copied into docs/HTML/ by
// scripts/build-docs; edit it in scripts/libs/, not there.

(function () {
  "use strict";
  var SITE = window.W_SITE;

  // {{{ theme
  // The reader's choice of light or dark, remembered in this browser only.
  // Storage can be missing (a private window, blocked site data), so every
  // touch of it is guarded and the page works without it.
  function savedTheme() {
    try { return localStorage.getItem("w-docs-theme"); } catch (e) { return null; }
  }
  function setTheme(name) {
    document.documentElement.setAttribute("data-theme", name);
    try { localStorage.setItem("w-docs-theme", name); } catch (e) { /* not remembered */ }
  }
  var initial = savedTheme();
  if (initial) document.documentElement.setAttribute("data-theme", initial);

  function wireTheme() {
    var button = document.getElementById("theme");
    if (!button) return;
    button.addEventListener("click", function () {
      var now = document.documentElement.getAttribute("data-theme") === "light" ? "dark" : "light";
      setTheme(now);
    });
  }
  // }}}

  // {{{ contents filter
  // Typing in the box hides every contents entry whose title and path lack
  // the typed words; a section with no entries left closes, one with
  // matches opens. Clearing the box puts the contents back as it was.
  function wireFilter() {
    var box = document.getElementById("filter");
    if (!box) return;
    var sections = Array.prototype.slice.call(document.querySelectorAll("#contents details"));
    var wasOpen = sections.map(function (d) { return d.open; });
    box.addEventListener("input", function () {
      var words = box.value.toLowerCase().split(/\s+/).filter(Boolean);
      sections.forEach(function (section, index) {
        var shown = 0;
        section.querySelectorAll("a.entry").forEach(function (a) {
          var hay = (a.textContent + " " + a.getAttribute("data-path")).toLowerCase();
          var keep = words.every(function (w) { return hay.indexOf(w) !== -1; });
          a.classList.toggle("hidden", !keep);
          if (keep) shown++;
        });
        section.open = words.length ? shown > 0 : wasOpen[index];
      });
    });
    // "/" from anywhere on the page jumps to the filter, as in many readers.
    document.addEventListener("keydown", function (e) {
      if (e.key === "/" && document.activeElement.tagName !== "TEXTAREA" && document.activeElement !== box) {
        e.preventDefault();
        box.focus();
      }
    });
  }
  // }}}

  // {{{ note: sealing and sending
  // rmail's frame, as a contact's own daemon builds it (rmail's
  // docs/protocol.md): [4-byte big-endian length][12-byte nonce]
  // [AES-256-GCM ciphertext + 16-byte tag], the key SHA-256 of the shared
  // token, the sealed text an HTTP-style "POST /deliver" request with a JSON
  // body. The browser wraps the frame in an HTTP request of its own, which
  // the daemon steps past (rmail 6d2ede2, 2026-10-08). The sealing itself is
  // docs-seal.js, not the browser's: pages served over plain http get no
  // built-in encryption (docs/055, "Sealing on plain http").
  var enc = new TextEncoder();
  var SEAL = window.W_SEAL;

  function sealFrame(token, requestText) {
    var nonce = SEAL.randomBytes(12);
    var sealed = SEAL.sealGcm(SEAL.sha256(enc.encode(token)), nonce, enc.encode(requestText));
    var length = 12 + sealed.length;
    var frame = new Uint8Array(4 + length);
    new DataView(frame.buffer).setUint32(0, length);
    frame.set(nonce, 4);
    frame.set(sealed, 16);
    return frame;
  }

  function deliverRequest(subject, body) {
    var json = JSON.stringify({
      type: "message",
      subject: subject,
      message_id: SEAL.uuid(),
      body: body,
      mtime: Math.floor(Date.now() / 1000)
    });
    return "POST /deliver HTTP/1.1\r\n" +
           "Host: " + SITE.host + ":" + SITE.port + "\r\n" +
           "Content-Type: application/json\r\n" +
           "Content-Length: " + enc.encode(json).length + "\r\n" +
           "Connection: close\r\n\r\n" + json;
  }

  // The note's text, in the shape scripts/check-docs-feedback accepts
  // (scripts/libs/feedback-form.lua describes it line by line).
  function noteBody(page, tags, isPublic, words) {
    var lines = ["page: " + page];
    tags.forEach(function (t) { lines.push("tag: " + t); });
    lines.push("public: " + (isPublic ? "yes" : "no"));
    lines.push("---");
    return lines.join("\n") + "\n" + words;
  }

  // A subject unique enough that two notes never share an inbox file name:
  // the page, the moment to the second, and six random letters.
  function noteSubject(page) {
    var stem = page.replace(/^.*\//, "").replace(/\.[^.]*$/, "").replace(/\.info$/, "-info");
    var t = new Date();
    function two(n) { return (n < 10 ? "0" : "") + n; }
    var when = t.getFullYear() + two(t.getMonth() + 1) + two(t.getDate()) + "-" +
               two(t.getHours()) + two(t.getMinutes()) + two(t.getSeconds());
    var letters = "";
    var random = SEAL.randomBytes(6);
    for (var i = 0; i < 6; i++) letters += "abcdefghijklmnopqrstuvwxyz"[random[i] % 26];
    return "feedback-" + stem + "-" + when + "-" + letters;
  }
  // }}}

  // {{{ note: the form
  function wireNote() {
    var form = document.getElementById("note");
    if (!form) return;
    var words = form.querySelector("textarea");
    var count = form.querySelector(".count");
    var status = form.querySelector(".status");
    var button = form.querySelector("button");
    var page = document.body.getAttribute("data-source");

    function say(text, kind) {
      status.textContent = text;
      status.className = "status" + (kind ? " " + kind : "");
    }
    function measure() {
      var bytes = enc.encode(words.value).length;
      count.textContent = bytes + " / " + SITE.maxText;
      count.style.color = bytes > SITE.maxText ? "var(--warn)" : "";
      return bytes;
    }
    words.addEventListener("input", measure);
    measure();

    form.addEventListener("submit", function (e) {
      e.preventDefault();
      if (words.value.trim() === "") { say("Write something first.", "bad"); return; }
      if (measure() > SITE.maxText) { say("That is longer than the mailbox accepts.", "bad"); return; }

      var tags = Array.prototype.slice.call(form.querySelectorAll("input[name=tag]:checked"))
        .map(function (box) { return box.value; });
      var isPublic = form.querySelector("input[name=public]").checked;
      var body = noteBody(page, tags, isPublic, words.value);

      button.disabled = true;
      say("Sending…");
      var frame = sealFrame(SITE.token, deliverRequest(noteSubject(page), body));
      // no-cors: the page never reads an answer, so the browser sends the
      // request as it is, without first asking the mailbox's permission.
      fetch("http://" + SITE.host + ":" + SITE.port + "/", {
        method: "POST", mode: "no-cors", body: frame
      }).then(function () {
        // All the browser can know: the mailbox took the bytes. Whether the
        // gatekeeper kept or refused the note shows only in its inbox.
        say("Sent to the project mailbox. Thank you.", "good");
        words.value = "";
        measure();
      }).catch(function (err) {
        say("The mailbox at " + SITE.host + ":" + SITE.port + " did not take the note (" + err.message +
            "). Is it running?", "bad");
      }).then(function () { button.disabled = false; });
    });
  }
  // }}}

  // {{{ charts
  // Charts are declared in the page as <div class="chart" data-chart="KIND">
  // holding a <script type="application/json"> of numbers the builder
  // measured. Each KIND is drawn by the function of that name below (a
  // table of drawers, so a new chart is a new entry, not a new branch).
  var SVG = "http://www.w3.org/2000/svg";
  var tooltip;

  function el(name, attrs, parent) {
    var node = document.createElementNS(SVG, name);
    for (var key in attrs) node.setAttribute(key, attrs[key]);
    if (parent) parent.appendChild(node);
    return node;
  }
  function tip(node, text) {
    node.addEventListener("mousemove", function (e) {
      tooltip.textContent = text;
      tooltip.style.display = "block";
      tooltip.style.left = (e.clientX + 14) + "px";
      tooltip.style.top = (e.clientY + 10) + "px";
    });
    node.addEventListener("mouseleave", function () { tooltip.style.display = "none"; });
  }
  function cssColor(name) {
    return getComputedStyle(document.documentElement).getPropertyValue(name).trim();
  }

  // Stacked horizontal bars: one row per entry, its parts side by side.
  // data: { rows: [{ label, href, parts: { key: count } }],
  //         keys: [{ key, label, color (a CSS variable name) }] }
  function stackedBars(holder, data, visibleKeys) {
    var old = holder.querySelector("svg");
    if (old) old.remove();
    var keys = data.keys.filter(function (k) { return visibleKeys[k.key]; });
    var max = 1;
    data.rows.forEach(function (r) {
      var sum = 0;
      keys.forEach(function (k) { sum += r.parts[k.key] || 0; });
      if (sum > max) max = sum;
    });
    var rowH = 26, labelW = 230, width = 760, barW = width - labelW - 50;
    var svg = el("svg", { viewBox: "0 0 " + width + " " + (data.rows.length * rowH + 6), role: "img" });
    data.rows.forEach(function (r, i) {
      var y = i * rowH + 4;
      var link = el("a", { href: r.href || "#" }, svg);
      var label = el("text", { x: labelW - 10, y: y + 15, "text-anchor": "end", "class": "bar-label" }, link);
      label.textContent = r.label;
      var x = labelW, total = 0;
      keys.forEach(function (k) {
        var count = r.parts[k.key] || 0;
        if (!count) return;
        var w = count / max * barW;
        var rect = el("rect", { x: x, y: y + 2, width: Math.max(w - 1.5, 1), height: rowH - 9, rx: 2,
                                fill: cssColor(k.color) }, svg);
        tip(rect, r.label + ": " + count + " " + k.label);
        x += w; total += count;
      });
      var value = el("text", { x: x + 6, y: y + 15, "class": "bar-value" }, svg);
      value.textContent = total;
    });
    holder.appendChild(svg);
  }

  // {{{ the notes viewer
  // Readers' public notes as folders of files, like a file browser: the
  // reader picks what the folders are (pages, kinds, months) and the order
  // of the files; clicking a file shows the note beside the tree. Every
  // note's text is a stranger's, so it reaches the screen only through
  // textContent, never as markup.
  // data: { notes: [{ file, when, page, href, tags: [keys], text }],
  //         tags: [{ key, label }] }
  function notesViewer(holder, data) {
    var labels = {};
    data.tags.forEach(function (t) { labels[t.key] = t.label; });
    var FOLDERS = {
      page:  function (n) { return [n.page]; },
      kind:  function (n) { return n.tags.length ? n.tags.map(function (k) { return labels[k] || k; }) : ["no kind ticked"]; },
      month: function (n) { return [n.when.slice(0, 7)]; }
    };
    var ORDERS = {
      newest: function (a, b) { return a.when < b.when ? 1 : -1; },
      oldest: function (a, b) { return a.when < b.when ? -1 : 1; }
    };
    var state = { folders: "page", order: "newest" };

    var controls = document.createElement("div");
    controls.className = "controls";
    function choice(group, values, wording) {
      var span = document.createElement("span");
      span.textContent = wording + " ";
      values.forEach(function (v) {
        var b = document.createElement("button");
        b.textContent = v;
        b.className = state[group] === v ? "on" : "";
        b.addEventListener("click", function () {
          state[group] = v;
          span.querySelectorAll("button").forEach(function (o) { o.className = o === b ? "on" : ""; });
          draw();
        });
        span.appendChild(b);
      });
      controls.appendChild(span);
    }
    choice("folders", ["page", "kind", "month"], "folders by");
    choice("order", ["newest", "oldest"], "files");
    holder.appendChild(controls);

    var frame = document.createElement("div");
    frame.className = "note-browser";
    var tree = document.createElement("div");
    tree.className = "note-tree";
    var view = document.createElement("div");
    view.className = "note-view";
    view.textContent = data.notes.length ? "Pick a note on the left." : "";
    frame.appendChild(tree);
    frame.appendChild(view);
    holder.appendChild(frame);

    function show(note) {
      view.textContent = "";
      var head = document.createElement("div");
      head.className = "reader-note-head";
      head.textContent = note.when + (note.tags.length ? " · " + note.tags.map(function (k) { return labels[k] || k; }).join(", ") : "");
      var about = document.createElement("div");
      about.className = "reader-note-head";
      about.appendChild(document.createTextNode("about "));
      var link = document.createElement(note.href ? "a" : "span");
      if (note.href) link.href = note.href;
      link.textContent = note.page;
      about.appendChild(link);
      var text = document.createElement("pre");
      text.className = "reader-note-text";
      text.textContent = note.text;
      view.appendChild(head);
      view.appendChild(about);
      view.appendChild(text);
    }

    function draw() {
      tree.textContent = "";
      var folders = {}, names = [];
      data.notes.slice().sort(ORDERS[state.order]).forEach(function (n) {
        FOLDERS[state.folders](n).forEach(function (f) {
          if (!folders[f]) { folders[f] = []; names.push(f); }
          folders[f].push(n);
        });
      });
      names.sort();
      names.forEach(function (name) {
        var d = document.createElement("details");
        d.open = true;
        var s = document.createElement("summary");
        s.textContent = name + " (" + folders[name].length + ")";
        d.appendChild(s);
        folders[name].forEach(function (n) {
          var b = document.createElement("button");
          b.className = "note-file";
          b.textContent = n.when + "  " + n.text.split("\n")[0].slice(0, 60);
          b.addEventListener("click", function () {
            tree.querySelectorAll(".note-file.on").forEach(function (o) { o.classList.remove("on"); });
            b.classList.add("on");
            show(n);
          });
          d.appendChild(b);
        });
        tree.appendChild(d);
      });
    }
    draw();
  }
  // }}}

  var DRAW = {
    notes: notesViewer,

    // The progress page: issues per phase, by status, with a button per
    // status to show or hide it.
    phases: function (holder, data) {
      var visible = {};
      var controls = document.createElement("div");
      controls.className = "controls";
      // Each button wears its status's colour, so the row of buttons is
      // also the chart's key: filled while shown, hollow while hidden.
      data.keys.forEach(function (k) {
        visible[k.key] = true;
        var b = document.createElement("button");
        var colour = cssColor(k.color);
        b.textContent = k.label;
        b.style.borderColor = colour;
        function paint() {
          b.style.background = visible[k.key] ? colour : "transparent";
          b.style.color = visible[k.key] ? cssColor("--ground") : colour;
        }
        paint();
        b.addEventListener("click", function () {
          visible[k.key] = !visible[k.key];
          paint();
          stackedBars(holder, data, visible);
        });
        controls.appendChild(b);
      });
      holder.appendChild(controls);
      stackedBars(holder, data, visible);
    },

    // The module map: lines of code per source module (its .c and .h),
    // with a slider hiding the small ones and buttons choosing the order.
    modules: function (holder, data) {
      var controls = document.createElement("div");
      controls.className = "controls";
      controls.innerHTML = '<button class="on" data-order="reading">reading order</button>' +
                           '<button data-order="size">largest first</button>' +
                           '<label>at least <input type="range" min="0" step="10"> <span></span> lines</label>';
      holder.appendChild(controls);
      var slider = controls.querySelector("input");
      var shownMin = controls.querySelector("span");
      var biggest = 0;
      data.rows.forEach(function (r) { biggest = Math.max(biggest, r.parts.c + r.parts.h); });
      slider.max = biggest;
      slider.value = 0;
      var order = "reading";

      function redraw() {
        var min = Number(slider.value);
        shownMin.textContent = min;
        var rows = data.rows.filter(function (r) { return r.parts.c + r.parts.h >= min; });
        if (order === "size") {
          rows = rows.slice().sort(function (a, b) { return (b.parts.c + b.parts.h) - (a.parts.c + a.parts.h); });
        }
        stackedBars(holder, { rows: rows, keys: data.keys }, { c: true, h: true });
      }
      controls.querySelectorAll("button").forEach(function (b) {
        b.addEventListener("click", function () {
          order = b.getAttribute("data-order");
          controls.querySelectorAll("button").forEach(function (o) { o.classList.toggle("on", o === b); });
          redraw();
        });
      });
      slider.addEventListener("input", redraw);
      redraw();
    }
  };

  function drawCharts() {
    tooltip = document.createElement("div");
    tooltip.className = "tooltip";
    document.body.appendChild(tooltip);
    document.querySelectorAll(".chart[data-chart]").forEach(function (holder) {
      var draw = DRAW[holder.getAttribute("data-chart")];
      var source = holder.querySelector("script[type='application/json']");
      if (!draw || !source) {
        holder.textContent = "This chart has no drawer or no numbers: " + holder.getAttribute("data-chart");
        return;
      }
      draw(holder, JSON.parse(source.textContent));
    });
    // Redraw in the new colours when the theme changes.
    new MutationObserver(function () {
      document.querySelectorAll(".chart[data-chart]").forEach(function (holder) {
        // Everything the drawer made goes; only the numbers stay.
        Array.prototype.slice.call(holder.children).forEach(function (n) {
          if (n.tagName !== "SCRIPT") n.remove();
        });
        var source = holder.querySelector("script[type='application/json']");
        DRAW[holder.getAttribute("data-chart")](holder, JSON.parse(source.textContent));
      });
    }).observe(document.documentElement, { attributes: true, attributeFilter: ["data-theme"] });
  }
  // }}}

  document.addEventListener("DOMContentLoaded", function () {
    wireTheme();
    wireFilter();
    wireNote();
    drawCharts();
    // The contents list scrolls to the page being read.
    var here = document.querySelector("#contents a.here");
    if (here) here.scrollIntoView({ block: "center" });
  });
})();
