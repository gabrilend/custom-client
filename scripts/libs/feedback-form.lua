-- feedback-form.lua - what a reader's note about a documentation page may say.
--
-- For a general audience: every page of the HTML documentation ends with a
-- box where a reader can write a note about that page and tick a few boxes
-- (a bug report, a feature request, fan mail). The note travels to the project's
-- own mailbox. This file is the one list of those boxes and the limits on
-- the note. Two programs read it: the page builder, which draws the boxes
-- from it, and the mailbox's checker, which refuses any note that claims a
-- box not listed here. Keeping the list in one place is what stops the two
-- from disagreeing.
--
-- The note as it travels (the message body), one "key: value" per line,
-- then a line of three dashes, then the reader's own words:
--
--     page: docs/045-datapath.md      the source file the page was built from
--     tag: bug                        zero or more, each a key from TAGS below
--     public: no                      "yes" or "no", always present
--     ---
--     the reader's text
--
-- The header is written by the page, never by the reader, but a stranger
-- can send any bytes at all, so the checker trusts none of it.

local form = {}

-- The boxes, in the order they are drawn. key (string): what travels in a
-- "tag:" line, lowercase letters and dashes only; label (string): what the
-- reader sees.
form.TAGS = {
    { key = "bug",        label = "bug report" },
    { key = "feature",    label = "feature request" },
    { key = "fan-mail",   label = "fan mail" },
}

-- Whether the reader asked for the note to be shown to other readers. The
-- note is private either way until the public browser exists; the answer
-- is kept so that browser can be built from notes already received.
form.PUBLIC_VALUES = { yes = true, no = true }

-- Limits, in bytes of the reader's text (number). The page refuses to
-- send past MAX_TEXT; the checker refuses past it too, because the page
-- is not the only thing that can send. Kept far below the 128 KiB a
-- single program argument may hold on Linux, which is how the mailbox
-- hands a note to its checker: a note past that would never reach the
-- checker at all and would be saved unchecked.
form.MAX_TEXT   = 8000
form.MAX_HEADER = 40      -- header lines (number)

-- The contact name the mailbox knows the pages by (string). Every note
-- arrives "from" it; notes from anyone else are not feedback and the
-- checker leaves them alone.
form.SENDER = require("project-mailbox").PAGES

-- {{{ function form.tag_set
-- The keys of TAGS as a set, for checking a "tag:" line in one lookup.
function form.tag_set()
    local set = {}
    for _, tag in ipairs(form.TAGS) do set[tag.key] = true end
    return set
end
-- }}}

return form
