#!/usr/bin/env luajit
-- 051-test-docs-feedback-checker.lua - proves the mailbox's gatekeeper for
-- readers' notes lets good notes through and turns bad ones into refusals.
--
-- For a general audience: readers' notes about documentation pages arrive
-- at the project mailbox from anyone who has a page, so the gatekeeper
-- (scripts/check-docs-feedback) is the only thing between a stranger's
-- bytes and a file on this disk. This runs it the way the mailbox does,
-- with a list of notes, some honest and some hostile, and checks what
-- comes out for each. It prints one line per case and fails loudly.
--
-- Usage: tests/051-test-docs-feedback-checker.lua [PROJECT_DIR]

local DIR = arg[1] or "/mnt/kaun/custom-client"
local CHECKER = DIR .. "/scripts/check-docs-feedback"

-- {{{ local function quote
local function quote(text)
    return "'" .. text:gsub("'", "'\\''") .. "'"
end
-- }}}

-- {{{ local function check
-- Runs the checker as rmail does: sender, subject, body as three arguments.
local function check(sender, subject, body)
    local pipe = io.popen("W_CLIENT_DIR=" .. quote(DIR) .. " " .. CHECKER .. " "
                          .. quote(sender) .. " " .. quote(subject) .. " " .. quote(body))
    local out = pipe:read("*a")
    pipe:close()
    return out
end
-- }}}

local GOOD_HEAD = "page: docs/045-datapath.md\ntag: bug\npublic: no\n---\n"

-- Each case: a name, the arguments, and what the output must (or must not)
-- contain. "accepted" means the output starts with the "checked:" line, the shape
-- only the accepting path writes.
local CASES = {
    { name = "an honest note is kept, cleaned and stamped",
      body = GOOD_HEAD .. "the third paragraph lost me",
      accepted = true, has = { "tag: bug", "\npage: docs/045-datapath.md\n", "the third paragraph lost me" } },
    { name = "a note with no boxes ticked is fine",
      body = "page: docs/045-datapath.md\npublic: yes\n---\nthanks",
      accepted = true, has = { "public: yes" } },
    { name = "an empty note is still a note",
      body = GOOD_HEAD, accepted = true },
    { name = "an escape byte cannot reach the terminal",
      body = GOOD_HEAD .. "before\27]0;owned\7after",
      accepted = true, has = { "before]0;ownedafter" }, lacks = { "\27", "\7" } },
    { name = "a direction-reversing mark is removed",
      body = GOOD_HEAD .. "abc\226\128\174def",
      accepted = true, has = { "abcdef" } },
    { name = "a box that is not on the form is refused",
      body = "page: docs/045-datapath.md\ntag: delete-everything\npublic: no\n---\nx",
      refused = "not on the form" },
    { name = "a page outside the project is refused",
      body = "page: docs/../../../etc/passwd\npublic: no\n---\nx",
      refused = "climbs out" },
    { name = "a page that does not exist is refused",
      body = "page: docs/999-nothing.md\npublic: no\n---\nx",
      refused = "no such page" },
    { name = "a page outside the documented folders is refused",
      body = "page: assets/settings/Config.wtf\npublic: no\n---\nx",
      refused = "outside the documented folders" },
    { name = "a header key that is not on the form is refused",
      body = "page: docs/045-datapath.md\npublic: no\nto: someone-else\n---\nx",
      refused = "header key" },
    { name = "a note with no divider is refused",
      body = "just some words", refused = "three dashes" },
    { name = "a note missing its public line is refused",
      body = "page: docs/045-datapath.md\n---\nx", refused = "no public line" },
    { name = "the same box twice is refused",
      body = "page: docs/045-datapath.md\ntag: fan-mail\ntag: fan-mail\npublic: no\n---\nx",
      refused = "twice" },
    { name = "words past the limit are refused",
      body = GOOD_HEAD .. string.rep("a", 8001), refused = "over the limit" },
    { name = "a refusal never lets the escape byte through either",
      body = "junk\27[2J", refused = "three dashes", lacks = { "\27" } },
}

local failures = 0

-- {{{ local function run_case
local function run_case(case)
    local out = check(case.sender or "docs-feedback", "feedback-test", case.body)
    local problems = {}

    if out == "" then problems[#problems + 1] = "printed nothing (rmail would keep the original)" end
    if case.accepted and not out:match("^checked: ") then problems[#problems + 1] = "was not accepted" end
    if case.refused then
        if not out:match("^refused by ") then problems[#problems + 1] = "was not refused" end
        if not out:find(case.refused, 1, true) then problems[#problems + 1] = "reason lacks \"" .. case.refused .. "\"" end
    end
    for _, want in ipairs(case.has or {}) do
        if not out:find(want, 1, true) then problems[#problems + 1] = "lacks \"" .. want .. "\"" end
    end
    for _, unwanted in ipairs(case.lacks or {}) do
        if out:find(unwanted, 1, true) then problems[#problems + 1] = "still holds a forbidden byte" end
    end

    if #problems == 0 then
        print("  ok    " .. case.name)
    else
        failures = failures + 1
        print("  FAIL  " .. case.name .. ": " .. table.concat(problems, "; "))
    end
end
-- }}}

for _, case in ipairs(CASES) do run_case(case) end

-- A message from any other contact is not the checker's business: it must
-- come out byte for byte as it went in.
do
    local body = "anything\nat all\27 even this"
    local out = check("kuvalu", "hello", body)
    if out == body then
        print("  ok    mail from anyone else passes through untouched")
    else
        failures = failures + 1
        print("  FAIL  mail from anyone else was changed")
    end
end

-- An edit: rmail's on_update hook gives the inbox file's path where the
-- subject would be, and the new body; a hostile edit must still be refused.
do
    local out = check("docs-feedback", "/some/mailbox/inbox/feedback-x", "edited\27[2J into junk")
    if out:match("^refused by ") and not out:find("\27", 1, true) then
        print("  ok    an edit, called as rmail's on_update calls it, is checked like a new note")
    else
        failures = failures + 1
        print("  FAIL  an edit, called as rmail's on_update calls it, was not refused cleanly")
    end
end

local total = #CASES + 2
if failures > 0 then
    print(string.format("%d of %d cases failed", failures, total))
    os.exit(1)
end
print(string.format("all %d cases passed", total))
