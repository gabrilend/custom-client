#!/bin/bash
# 052-test-note-box-frame.sh - proves a documentation page's note box,
# served over plain http, sends a note the project mailbox can open and
# will keep.
#
# For a general audience: at the foot of every documentation page is a box
# for a note, which the page seals and posts to the project's mailbox. The
# site is meant to be served over plain http from a server, where browsers
# withhold their built-in encryption, so the page seals notes with its own
# (docs-seal.js). This test serves a real built page that way, from this
# machine's network address, with a test key and a test port; a stand-in
# mailbox (tests/054) records what arrives and answers as a server would;
# a hidden Firefox opens the page and a small script types a note, ticks two
# boxes and presses send. Then rmail's own decryption opens what arrived
# (tests/053) and the mailbox's gatekeeper runs on it. The real mailbox and
# its key are never touched.
#
# Needs: firefox, ncat, lua5.4, rmail checked out (for its libraries), and
# a network address other than 127.0.0.1 (the address is what makes the
# page "not secure" to the browser).
# Usage: tests/052-test-note-box-frame.sh [PROJECT_DIR] [--real-mailbox]
#   default         the stand-in mailbox (tests/054), no rmail daemon run
#   --real-mailbox  a throwaway rmail mailbox with the real daemon and the
#                   real gatekeeper, kept away from the router (see below)

DIR="/mnt/kaun/custom-client"
MODE="stand_in"
for given in "$@"; do
    case "${given}" in
        --real-mailbox) MODE="real_mailbox" ;;
        *) DIR="${given}" ;;
    esac
done

PAGE_PORT=5598
MAIL_PORT=5599
TOKEN="a-test-key-that-is-not-the-mailbox-key"
WORK="${DIR}/tmp/shared-memory/note-box-test"
CAPTURE="${WORK}/captured.bin"

# This machine's address on its network: the source address it would use
# to reach the outside world. 127.0.0.1 would count as secure and hide the
# very thing being tested, so no address is an error.
ROUTE=$(ip -4 route get 1.1.1.1)
LAN=$(echo "${ROUTE}" | grep -o 'src [0-9.]*' | cut -d' ' -f2)
if [ -z "${LAN}" ]; then
    echo "  FAIL  this machine has no network address to serve the page from"
    exit 1
fi

rm -rf "${WORK}"
mkdir -p "${WORK}/profile"

"${DIR}/scripts/build-docs" > "${WORK}/build.log"
if [ $? -ne 0 ]; then
    echo "  FAIL  the site did not build:"
    cat "${WORK}/build.log"
    exit 1
fi

# The page, with its scripts poured into it (the stand-in web server below
# serves one file): a test config in place of the mailbox's, the site's own
# sealing and page code unchanged, and a small script that plays the reader.
H="${DIR}/docs/HTML"
printf '<script>window.W_SITE = {"host":"%s","port":%s,"token":"%s","maxText":8000};</script>\n' \
    "${LAN}" "${MAIL_PORT}" "${TOKEN}" > "${WORK}/config.inline"
{ echo "<script>"; cat "${H}/docs-seal.js"; echo "</script>"; } > "${WORK}/seal.inline"
{ echo "<script>"; cat "${H}/docs-site.js"; cat <<'EOF'
// Plays the reader: types a note, ticks two boxes, presses send, and
// reports whether the page was secure and what it says afterwards.
document.addEventListener("DOMContentLoaded", function () {
  console.log("note-box-test: secure page: " + window.isSecureContext);

  // The sealing's known answers, published with the standards: SHA-256 of
  // "abc" (FIPS 180-4's example), and AES-256-GCM with an all-zero key and
  // nonce (the GCM specification's test cases 13 and 14: nothing, and one
  // block of zeros), ciphertext followed by tag.
  function hex(bytes) {
    return Array.prototype.map.call(bytes, function (b) { return (b < 16 ? "0" : "") + b.toString(16); }).join("");
  }
  var S = window.W_SEAL, zeroKey = new Uint8Array(32), zeroNonce = new Uint8Array(12);
  var known = [
    ["sha256 of abc", hex(S.sha256(new TextEncoder().encode("abc"))),
     "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"],
    ["gcm test case 13", hex(S.sealGcm(zeroKey, zeroNonce, new Uint8Array(0))),
     "530f8afbc74536b9a963b4f1c4cb738b"],
    ["gcm test case 14", hex(S.sealGcm(zeroKey, zeroNonce, new Uint8Array(16))),
     "cea7403d4d606b6e074ec5d3baf39d18d0d1c8a799996bf0265b98b5d48ab919"]
  ];
  known.forEach(function (k) {
    console.log("note-box-test: known answer " + (k[1] === k[2] ? "ok" : "WRONG") + ": " + k[0] +
                (k[1] === k[2] ? "" : " gave " + k[1]));
  });

  var form = document.getElementById("note");
  form.querySelector("textarea").value = "The second diagram lost me.\nCould it name the threads?";
  form.querySelector("input[value=bug]").checked = true;
  form.querySelector("input[value=fan-mail]").checked = true;
  form.requestSubmit();
  setTimeout(function () {
    console.log("note-box-test: page says: " + form.querySelector(".status").textContent);
  }, 3000);
});
EOF
echo "</script>"; } > "${WORK}/site.inline"

sed -e '/<script src="site-config.js"><\/script>/{r '"${WORK}/config.inline" -e 'd}' \
    -e '/<script src="docs-seal.js"><\/script>/{r '"${WORK}/seal.inline" -e 'd}' \
    -e '/<script src="docs-site.js"><\/script>/{r '"${WORK}/site.inline" -e 'd}' \
    "${H}/docs-045-datapath.html" > "${WORK}/page.html"

SIZE=$(stat -c %s "${WORK}/page.html")
printf 'HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: %s\r\nConnection: close\r\n\r\n' \
    "${SIZE}" > "${WORK}/page.http"
cat "${WORK}/page.html" >> "${WORK}/page.http"

printf 'user_pref("devtools.console.stdout.content", true);\n' > "${WORK}/profile/user.js"



# {{{ the two mailboxes
# What receives the note, by mode (a table of functions: MODE picks the
# pair, no branching below). Each mode has a start and a finish; the
# finish prints its own ok/FAIL lines and returns non-zero on failure.

# stand-in: tests/054 keeps the raw bytes and answers 200; tests/053 then
# opens them with rmail's decryption and runs the gatekeeper.
stand_in_start() {
    lua5.4 "${DIR}/tests/054-catch-one-post.lua" ${MAIL_PORT} "${CAPTURE}" 25 "${LAN}" &
    RECEIVER=$!
}
stand_in_finish() {
    wait ${RECEIVER}
    if [ $? -ne 0 ] || [ ! -s "${CAPTURE}" ]; then
        echo "  FAIL  nothing was posted to port ${MAIL_PORT}"
        return 1
    fi
    lua5.4 "${DIR}/tests/053-unseal-note-frame.lua" "${CAPTURE}" "${TOKEN}" "${DIR}"
}

# real-mailbox: a throwaway rmail mailbox, the real daemon and the real
# gatekeeper, made by the project's own scripts/make-project-mailbox (so it
# is configured exactly as the project's) and started by
# scripts/run-project-mailbox, which keeps rmail from finding the router
# programs: rmail probes the router at every start whenever it can find
# them, and a test must not touch the router. Its pages' key is replaced by
# the test key the page was built with.
real_mailbox_start() {
    BOX="${WORK}/box"
    "${DIR}/scripts/make-project-mailbox" "${DIR}" --mailbox "${BOX}" --port ${MAIL_PORT} > "${WORK}/make.log"
    sed -i "s/^docs-feedback\.token *=.*/docs-feedback.token  = \"${TOKEN}\"/" "${BOX}/contacts"
    "${DIR}/scripts/run-project-mailbox" "${DIR}" --mailbox "${BOX}" > "${WORK}/daemon.out" 2>&1 &
    RECEIVER=$!
    # Ready when it listens; it may first spend a moment on its own checks.
    for try in $(seq 1 50); do
        if [ -n "$(ss -ltnH "sport = :${MAIL_PORT}")" ]; then return 0; fi
        sleep 0.2
    done
    echo "  FAIL  the throwaway mailbox never started listening; its output:"
    cat "${WORK}/daemon.out"
    return 1
}
real_mailbox_finish() {
    local kept=""
    for try in $(seq 1 50); do
        kept=$(find "${BOX}/inbox" -maxdepth 1 -name 'feedback-*' -print -quit)
        if [ -n "${kept}" ]; then break; fi
        sleep 0.2
    done
    kill ${RECEIVER}
    wait ${RECEIVER} 2> /dev/null
    if [ -z "${kept}" ]; then
        echo "  FAIL  no note reached the mailbox's inbox; the end of its log:"
        tail -20 /tmp/rmail-progress/log$(echo "${BOX}" | tr "/" "-")
        return 1
    fi
    echo "  ok    the real rmail daemon took the browser's request and saved the note: ${kept##*/}"
    if ! head -1 "${kept}" | grep -q "^checked: .* by scripts/check-docs-feedback"; then
        echo "  FAIL  the saved note was not shaped by the gatekeeper:"
        cat "${kept}"
        return 1
    fi
    echo "  ok    the gatekeeper shaped it on the way in:"
    sed 's/^/          /' "${kept}"
}
# }}}

RMAIL="/home/ritz/programs/r-mail"
MODE_START="${MODE}_start"

# A port still held (by an earlier run's leftover server, say) would serve
# something other than this run's page; that is a failure, not a detail.
for port in ${PAGE_PORT} ${MAIL_PORT}; do
    HELD=$(ss -ltnpH "sport = :${port}")
    if [ -n "${HELD}" ]; then
        echo "  FAIL  port ${port} is already in use:"
        echo "${HELD}"
        exit 1
    fi
done
MODE_FINISH="${MODE}_finish"

# The page's server: the same reply to every request.
timeout 40 ncat -l -k "${LAN}" ${PAGE_PORT} --sh-exec "cat '${WORK}/page.http'" &
PAGE_SERVER=$!

${MODE_START} || { kill ${PAGE_SERVER}; exit 1; }

FIREFOX_OUT=$(timeout 15 firefox --headless --no-remote --profile "${WORK}/profile" "http://${LAN}:${PAGE_PORT}/" 2>&1)
SECURE=$(echo "${FIREFOX_OUT}" | grep -o 'secure page: [a-z]*')
SAID=$(echo "${FIREFOX_OUT}" | grep "page says:")
SAID="${SAID#*page says: }"
kill ${PAGE_SERVER}

if [ "${SECURE}" != "secure page: false" ]; then
    echo "  FAIL  the page was not served as a plain-http page (${SECURE:-it never reported})"
    exit 1
fi
echo "  ok    the page was served over plain http from ${LAN}, which the browser does not count as secure"

KNOWN=$(echo "${FIREFOX_OUT}" | grep -o 'known answer [A-Za-z]*: .*' | sed 's/"$//')
if [ $(echo "${KNOWN}" | grep -c "known answer ok") -ne 3 ]; then
    echo "  FAIL  the page's sealing gave wrong known answers:"
    echo "${KNOWN}"
    exit 1
fi
echo "  ok    the page's sealing gives the standards' published answers (sha256, gcm cases 13 and 14)"

# The page's own word after the mailbox answered.
case "${SAID}" in
    *"Sent to the project mailbox"*)
        echo "  ok    the page reports the note sent" ;;
    *)
        echo "  FAIL  the page did not report the note sent; it said: ${SAID}"
        ${MODE_FINISH} > /dev/null
        exit 1 ;;
esac

${MODE_FINISH}
