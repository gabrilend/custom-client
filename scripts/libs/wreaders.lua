-- wreaders.lua - the W client's reader library (libwreaders.so) from LuaJIT.
--
-- For a general audience: the client's parts that read the original
-- game's files (its archives, tables, pictures, models) are also built as
-- a library that Lua programs can call directly (scripts/build makes it,
-- with a header of its declarations). This loads that library and offers
-- the archive chain in Lua's terms: open the client's archives, read a
-- file by name as a Lua string. Used by this project's tools and by
-- world-edit-to-execute (docs/001, "The reader library").
--
-- Usage:
--   local W = dofile("/mnt/kaun/custom-client/scripts/libs/wreaders.lua")
--   local chain = W.chain(CLIENT_DATA_DIR, "enUS")     -- errors with the reader's sentence if it can't
--   local bytes, from = chain:read("DBFilesClient\\Map.dbc")   -- nil when no archive holds it
--   chain:close()
-- The library and header are found in the W client's build output
-- (tmp/tmp/build/), or where W.load is told.

local ffi = require("ffi")

local W = {}
local lib

-- {{{ W.load: the library and its declarations, once
function W.load(build_dir)
    if lib then return lib end
    build_dir = build_dir or "/mnt/kaun/custom-client/tmp/tmp/build"
    local f = io.open(build_dir .. "/wreaders.h", "r")
    if not f then error("wreaders: no " .. build_dir .. "/wreaders.h: run the W client's scripts/build first") end
    ffi.cdef(f:read("*a"))
    f:close()
    ffi.cdef("void free(void *);")
    lib = ffi.load(build_dir .. "/libwreaders.so")
    return lib
end
-- }}}

-- {{{ W.chain: the client's archives, newest patch first
local Chain = {}
Chain.__index = Chain
function W.chain(data_dir, locale, build_dir)
    local L = W.load(build_dir)
    local c = ffi.new("MpqChain")
    local why = ffi.new("WWhy")
    if L.mpq_chain_open(c, data_dir, locale, why) ~= 0 then
        error("wreaders: " .. ffi.string(why.text))
    end
    return setmetatable({ c = c }, Chain)
end
-- a file's bytes (a Lua string) and the archive it came from; nil when none holds it
function Chain:read(name)
    local size = ffi.new("size_t[1]")
    local from = ffi.new("const char *[1]")
    local p = lib.mpq_chain_read(self.c, name, size, from)
    if p == nil then return nil end
    local s = ffi.string(p, size[0])
    ffi.C.free(p)
    return s, from[0] ~= nil and ffi.string(from[0]) or nil
end
function Chain:close() lib.mpq_chain_close(self.c) end
-- }}}

return W
