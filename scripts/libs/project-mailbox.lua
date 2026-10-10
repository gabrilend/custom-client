-- project-mailbox.lua - the names and places of the project's own mailbox.
--
-- For a general audience: the project has its own rmail mailbox
-- (project-rmail/), which receives readers' notes from the documentation
-- site, sends a copy of everything to the owner's main mailbox, and keeps
-- the notes readers asked to make public in its outbox, addressed to the
-- website, until the site is next built. Several scripts need the same
-- names for these: the mailbox maker, the gatekeeper, the sorter, the
-- connecting script and the site builder. This is the one place they are
-- written, so the scripts cannot disagree.
--
-- Contact names are this mailbox's own labels: rmail never sends them
-- anywhere, and the owner's mailbox knows this one by a name of its own
-- (OWNER_SIDE_NAME below).

local box = {}

box.NAME = "w-client-docs"          -- the mailbox's own name (string), never sent
box.PORT = 5557                     -- where it listens (number); readers' pages post here

-- The contacts (strings), as written in project-rmail/contacts:
box.PAGES   = "docs-feedback"       -- the documentation pages; its key is in every page
box.OWNER   = "owner"               -- the owner's main mailbox; everything is forwarded to it
box.WEBSITE = "website"             -- where public notes are addressed (wow.ritzmenardi.com)

-- The name the owner's mailbox gives this one in its own contacts file.
box.OWNER_SIDE_NAME = "w-client-docs"

-- {{{ function box.folder
-- The mailbox folder for a project directory (string -> string).
function box.folder(dir)
    return dir .. "/project-rmail"
end
-- }}}

return box
