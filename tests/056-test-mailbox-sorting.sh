#!/bin/bash
# 056-test-mailbox-sorting.sh - proves the project mailbox checks, keeps,
# sorts and forwards readers' notes, with real rmail daemons.
#
# For a general audience: the project's mailbox receives notes from the
# documentation pages, cleans them, copies everything to the owner's main
# mailbox, and files the notes readers made public under the website. This
# builds two throwaway mailboxes in RAM with the project's own scripts
# (one made like the project's, one standing in for the owner's), connects
# them, starts both, and sends four things as
# the pages would: a private note, a public note, a note the gatekeeper must
# refuse, and an edit of the first note carrying bytes that would take
# over a terminal. Then it checks what each mailbox holds. The real
# mailboxes are never touched.
#
# Needs: lua5.4, luajit, rmail checked out (its daemon and its test contact).
# Usage: tests/056-test-mailbox-sorting.sh [PROJECT_DIR]

DIR="/mnt/mtwo/games/azeroth-core/custom-client"
if [ -n "$1" ]; then
    DIR="$1"
fi

RMAIL="/home/ritz/programs/r-mail"
WORK="${DIR}/tmp/shared-memory/sorting-test"
PROJECT_BOX="${WORK}/project"
OWNER_BOX="${WORK}/owner"
PROJECT_PORT=5596
OWNER_PORT=5595
FAILED=0

# {{{ ok / fail
ok()   { echo "  ok    $1"; }
fail() { echo "  FAIL  $1"; FAILED=1; }
# }}}

# {{{ wait_for
# Waits up to $1 seconds for the command in the rest of the arguments to
# succeed; returns its last status.
wait_for() {
    local seconds="$1"
    shift
    local tries=$((seconds * 5))
    for try in $(seq 1 ${tries}); do
        if "$@"; then return 0; fi
        sleep 0.2
    done
    return 1
}
# }}}

listening() { [ -n "$(ss -ltnH "sport = :$1")" ]; }

for port in ${PROJECT_PORT} ${OWNER_PORT}; do
    if listening ${port}; then
        echo "  FAIL  port ${port} is already in use"
        exit 1
    fi
done

rm -rf "${WORK}"
mkdir -p "${OWNER_BOX}/inbox" "${OWNER_BOX}/outbox"

# The project's mailbox, made by the project's own script.
"${DIR}/scripts/make-project-mailbox" "${DIR}" --mailbox "${PROJECT_BOX}" --port ${PROJECT_PORT} > "${WORK}/make.log"

# The owner's, a plain mailbox with no hooks.
cat > "${OWNER_BOX}/config" <<EOF
# Made by tests/056 for one run: stands in for the owner's main mailbox.
name = test-owner
port = ${OWNER_PORT}
on_receive_raw = ""
on_receive     = ""
on_package     = ""
on_send        = ""
on_delete      = ""
on_update      = ""
EOF
echo "// Made by tests/056 for one run." > "${OWNER_BOX}/contacts"

"${DIR}/scripts/connect-owner-mailbox" "${DIR}" --mailbox "${PROJECT_BOX}" --owner "${OWNER_BOX}" > "${WORK}/connect.log"
if [ $? -ne 0 ]; then
    fail "the two mailboxes could not be connected:"
    cat "${WORK}/connect.log"
    exit 1
fi
ok "made both mailboxes with the project's scripts and connected them"

"${DIR}/scripts/run-project-mailbox" "${DIR}" --mailbox "${PROJECT_BOX}" > "${WORK}/project.out" 2>&1 &
PROJECT_PID=$!
"${DIR}/scripts/run-project-mailbox" "${DIR}" --mailbox "${OWNER_BOX}" > "${WORK}/owner.out" 2>&1 &
OWNER_PID=$!

# {{{ stop
stop() {
    kill ${PROJECT_PID} ${OWNER_PID} 2> /dev/null
    wait ${PROJECT_PID} ${OWNER_PID} 2> /dev/null
}
# }}}

if ! wait_for 15 listening ${PROJECT_PORT} || ! wait_for 15 listening ${OWNER_PORT}; then
    fail "the mailboxes did not start listening"
    cat "${WORK}/project.out" "${WORK}/owner.out"
    stop
    exit 1
fi
ok "both daemons are listening (started without the router programs)"

# The pages' key, read from the contacts file the project's script made.
KEY_LINE=$(grep -E '^docs-feedback\.token' "${PROJECT_BOX}/contacts")
PAGES_KEY=$(echo "${KEY_LINE}" | cut -d'"' -f2)

# Four sends, as the pages' contact, through rmail's own test contact.
cat > "${WORK}/send.lua" <<'LUA'
local rmail, port, key, stamp = arg[1], tonumber(arg[2]), arg[3], arg[4]
package.path = rmail .. "/scripts/lib/?.lua;" .. package.path
local pages = require("fake-contact").new(rmail, "127.0.0.1", port, key)
local function note(subject, id, public, words)
    return pages:post_json("/deliver", { type = "message", subject = subject, message_id = id, mtime = os.time(),
        body = "page: docs/045-datapath.md\ntag: bug\npublic: " .. public .. "\n---\n" .. words })
end
print("private", note("feedback-private-" .. stamp, "id-private-" .. stamp, "no", "a private note " .. stamp))
print("public",  note("feedback-public-" .. stamp, "id-public-" .. stamp, "yes", "a public note <script>alert(1)</script>"))
print("refused", pages:post_json("/deliver", { type = "message", subject = "feedback-junk-" .. stamp,
    message_id = "id-junk-" .. stamp, mtime = os.time(), body = "public: yes\njunk with no form at all " .. stamp }))
print("edit",    pages:post_json("/deliver", { type = "update", message_id = "id-private-" .. stamp,
    body = "page: docs/045-datapath.md\npublic: no\n---\nedited\27]0;owned\7 words" }))
LUA
STAMP=$(date +%H%M%S)
SENT=$(lua5.4 "${WORK}/send.lua" "${RMAIL}" ${PROJECT_PORT} "${PAGES_KEY}" "${STAMP}")
echo "${SENT}" > "${WORK}/send.log"
# The three notes are taken; the edit is refused by rmail itself (403):
# the pages' contact is listen-only (rmail issue 420, commit 9ec702e) and
# may only send plain messages. Before that, the edit reached the inbox
# and only the gatekeeper on rmail's on_update hook stood in its way.
if [ $(echo "${SENT}" | grep -c $'\t200\t') -ne 3 ] || ! echo "${SENT}" | grep -q $'^edit\t403\t'; then
    fail "the mailbox did not take the three notes and refuse the edit:"
    echo "${SENT}"
else
    ok "rmail took the three notes and refused the edit (403: the pages' contact is listen-only)"
fi

P_INBOX="${PROJECT_BOX}/inbox"
P_OUTBOX="${PROJECT_BOX}/outbox"
PRIVATE="feedback-private-${STAMP}"
PUBLIC="feedback-public-${STAMP}"
JUNK="feedback-junk-${STAMP}"

# What the project mailbox kept.
wait_for 10 test -f "${P_INBOX}/${JUNK}"
FIRST=$(head -1 "${P_INBOX}/${PUBLIC}")
case "${FIRST}" in
    "checked: "*) ok "the public note was checked and kept" ;;
    *) fail "the public note was not kept as checked: ${FIRST}" ;;
esac
FIRST=$(head -1 "${P_INBOX}/${JUNK}")
case "${FIRST}" in
    "refused by "*) ok "the formless note was kept only as a refusal" ;;
    *) fail "the formless note was not refused: ${FIRST}" ;;
esac
if grep -q "a private note ${STAMP}" "${P_INBOX}/${PRIVATE}" && ! grep -q "edited" "${P_INBOX}/${PRIVATE}"; then
    ok "the private note is as it arrived; the refused edit changed nothing"
else
    fail "the private note was changed by the refused edit"
fi
if grep -q $'\x1b' "${P_INBOX}/${PRIVATE}"; then
    fail "the edit's terminal-control byte reached the inbox"
else
    ok "the edit's terminal-control byte never reached the inbox"
fi

# How the sorter filed them.
wait_for 10 test -f "${P_OUTBOX}/${JUNK}"
for name in "${PRIVATE}" "${PUBLIC}" "${JUNK}"; do
    if [ -f "${P_OUTBOX}/${name}" ] && grep -q "^to: owner$" "${P_OUTBOX}/${name}"; then
        ok "${name%-*} is in the outbox for the owner"
    else
        fail "${name%-*} is not in the outbox for the owner"
    fi
done
if grep -q "^to: website$" "${P_OUTBOX}/${PUBLIC}"; then
    ok "the public note is also addressed to the website"
else
    fail "the public note is not addressed to the website"
fi
for name in "${PRIVATE}" "${JUNK}"; do
    if grep -q "^to: website$" "${P_OUTBOX}/${name}"; then
        fail "${name%-*} was addressed to the website"
    else
        ok "${name%-*} was not addressed to the website"
    fi
done

# What reached the owner: everything, in rmail's own time (a contact's
# first exchange may wait for its timer).
if wait_for 90 test -f "${OWNER_BOX}/inbox/${JUNK}" && test -f "${OWNER_BOX}/inbox/${PUBLIC}" \
   && test -f "${OWNER_BOX}/inbox/${PRIVATE}"; then
    ok "all three reached the owner's mailbox"
else
    fail "not everything reached the owner's mailbox; it holds: $(ls "${OWNER_BOX}/inbox" | tr '\n' ' ')"
fi

# What rmail did with the website's copy, which has nowhere to go.
cp "${P_OUTBOX}/${PUBLIC}" "${WORK}/public-outbox-file-after.txt"
NOTES=$(grep -c '^//' "${P_OUTBOX}/${PUBLIC}")
echo "  ..    the public note's outbox file, as rmail left it (${NOTES} note lines):"
sed -n '1,6p' "${P_OUTBOX}/${PUBLIC}" | sed 's/^/          /'

stop

# The site, built from this throwaway mailbox into its own folder: the
# public note is gathered, shown escaped, and nothing private shows.
SITE="${WORK}/site"
"${DIR}/scripts/build-docs" "${DIR}" --mailbox "${PROJECT_BOX}" --out "${SITE}" > "${WORK}/build.log" 2>&1
GATHERED=$(head -1 "${WORK}/build.log")
case "${GATHERED}" in
    *"; 1 public notes gathered)"*) ok "the site builder gathered exactly the one public note" ;;
    *) fail "the site builder did not gather exactly one public note: ${GATHERED}" ;;
esac
if grep -q "a public note" "${SITE}/notes.html" && grep -q "a public note" "${SITE}/docs-045-datapath.html"; then
    ok "the public note is in the notes viewer and at the foot of its page"
else
    fail "the public note is missing from the notes viewer or its page"
fi
if grep -q "<script>alert" "${SITE}/notes.html" "${SITE}/docs-045-datapath.html"; then
    fail "the note's markup reached a page unescaped"
else
    ok "the note's markup reaches the pages only escaped"
fi
# The private and refused notes' words carry this run's time, which no
# document or source file holds, so any page holding them is a leak.
LEAKS=$(grep -l -E "a private note ${STAMP}|junk with no form at all ${STAMP}" "${SITE}"/*.html)
if [ -n "${LEAKS}" ]; then
    fail "a private or refused note reached the site"
else
    ok "neither the private note nor the refused one reached the site"
fi

if [ ${FAILED} -ne 0 ]; then
    echo "some checks failed; logs in ${WORK} and /tmp/rmail-progress/"
    exit 1
fi
echo "all checks passed"
