#!/usr/bin/env luajit
-- 100-layouts-match-converter.lua - the tables' layouts this client reads
-- by (src/097-table-layouts.c) against the ones world-edit-to-execute's
-- converter writes by (its src/wowbuild/dbc.lua LAYOUTS), table by table.
--
-- For a general audience: the map editor's converter writes WoW's data
-- tables (Map, the creatures' looks, the light ...) and this client reads
-- them. Each side keeps its own list of what every column of a table is;
-- if the two lists drift, the client reads a column as something it isn't.
-- This checks the two lists agree, from the converter's own code and its
-- own written bytes rather than from either side's memory: for every
-- table both know, it has the converter write one row with a different
-- value in every column, then reads that row back through this client's
-- own table reader (016) by this client's layout (097) and compares,
-- column by column: the width (fields and the bytes a row), each column's
-- name where the names line up, its kind (a whole number, a signed one, a
-- decimal, a text, a localised text's sixteen texts and its mask), the
-- value read back, the localised texts' mask, and that every column the
-- converter says this client reads (its `seen`) is one 097 marks as read.
-- The tables only one side knows are listed: the converter's that 097
-- lacks are a failure (the client couldn't read them by a layout); 097's
-- that the converter doesn't write keep widths no file of the converter's
-- checks.
--
-- Usage: luajit tests/100-layouts-match-converter.lua [PROJECT_DIR] [--build DIR] [--wete DIR]
--   PROJECT_DIR  the W client (default below)
--   --build DIR  where libwreaders.so and wreaders.h are (default PROJECT_DIR/tmp/tmp/build)
--   --wete DIR   a world-edit-to-execute tree (its src/ is enough: an
--                exported commit works). With none given, or one with no
--                src/wowbuild/dbc.lua, the check is skipped: said, exit 0.
-- Exit status 0 when every table agrees (or skipped), 1 when one doesn't.

-- {{{ set_dir
local DIR = "/mnt/kaun/custom-client"
local WETE
local args, i = {}, 1
while i <= #arg do
    local a = arg[i]
    if a == "--build" then args.build = arg[i + 1]; i = i + 2
    elseif a == "--wete" then WETE = arg[i + 1]; i = i + 2
    else DIR = a; i = i + 1 end
end
args.build = args.build or (DIR .. "/tmp/tmp/build")
-- }}}

local function exists(path) local f = io.open(path, "rb"); if f then f:close() end; return f ~= nil end
if not WETE or not exists(WETE .. "/src/wowbuild/dbc.lua") then
    print("skipped: no world-edit-to-execute tree given (--wete DIR, with src/wowbuild/dbc.lua)")
    os.exit(0)
end

package.path = WETE .. "/src/?.lua;" .. WETE .. "/src/?/init.lua;" .. package.path
local ffi = require("ffi")
local dbc = require("wowbuild.dbc")
local W = dofile(DIR .. "/scripts/libs/wreaders.lua")
local L = W.load(args.build)

-- {{{ the client's side: 097's layouts, column by column
local KIND = { [0] = "u32", "i32", "f32", "str", "loc" }   -- TableType's order
local function client_layout(name)
    local l = L.table_layout(name)
    if l == nil then return nil end
    local cols = {}
    local c = ffi.new("TableColumn")
    for k = 1, l.fields do
        if L.table_layout_column(l, k, c) == 0 then break end
        cols[k] = { name = ffi.string(c.name), kind = KIND[tonumber(c.type)], read = c.read_by ~= nil,
                    read_by = c.read_by ~= nil and ffi.string(c.read_by) or nil, def = tonumber(c.def) }
    end
    return { fields = tonumber(l.fields), cols = cols }
end
local function client_tables()
    local out = {}
    for k = 0, 999 do
        local l = L.table_layout_at(k)
        if l == nil then break end
        out[#out + 1] = ffi.string(l.name)
    end
    return out
end
-- }}}

-- {{{ the converter's side: a row with a different value in every column
-- (numbers from the column's place; texts naming it), written by dbc.write
local function made_row(name)
    local row, want = {}, {}
    for k, f in ipairs(dbc.LAYOUTS[name].fields) do
        local n, kind = f[1], f[2]
        if kind == "u32" then row[n] = 1000 + k
        elseif kind == "i32" then row[n] = -(1000 + k)
        elseif kind == "f32" then row[n] = k + 0.5
        else row[n] = name .. "." .. n end                  -- str, loc
        want[n] = row[n]
    end
    return row, want
end
-- }}}

-- {{{ compare one table
local function compare(name)
    local problems = {}
    local function say(fmt, ...) problems[#problems + 1] = string.format(fmt, ...) end
    local conv = dbc.LAYOUTS[name]
    local cols = dbc.columns(name)                 -- the converter's 4-byte columns ({ name, kind[, i] })
    local mine = client_layout(name)
    if not mine then say("097 has no layout for it"); return problems end
    if conv.count ~= mine.fields or #cols ~= mine.fields then
        say("width: the converter's %d fields (%d columns), 097's %d", conv.count, #cols, mine.fields)
    end
    local row, want = made_row(name)
    local bytes = dbc.write(name, { row })
    local h = dbc.header(bytes, name)
    if h.fields ~= mine.fields or h.row_len ~= mine.fields * 4 then
        say("the written file: %d fields, %d bytes a row; 097: %d, %d", h.fields, h.row_len, mine.fields, mine.fields * 4)
    end
    -- read back through this client's own reader (016), by 097's layout
    local buf = ffi.new("unsigned char[?]", #bytes)
    ffi.copy(buf, bytes, #bytes)
    local d, why = ffi.new("Dbc"), ffi.new("WWhy")
    if L.dbc_open(d, buf, #bytes, why) ~= 0 then say("016 refuses the written file: %s", ffi.string(why.text)); return problems end
    local fits = L.table_layout_fits(L.table_layout(name), d.fields, "the converter's written row", why)
    if fits ~= 0 then say("097 refuses its width: %s", ffi.string(why.text)) end
    local n = math.min(#cols, mine.fields)
    for k = 1, n do
        local c, m = cols[k], mine.cols[k]
        local cname, ckind = c[1], c[2]
        -- the name: a localised text's columns are <name>_<lang> and <name>_mask in 097
        local base = m.name:match("^(.-)_[%w]+$") or m.name
        local is_loc_part = (ckind == "locstr" or ckind == "locmask")
        local name_ok = is_loc_part and base == cname or (not is_loc_part and m.name == cname)
        if not name_ok then say("column %d: the converter's %s, 097's %s", k, cname, m.name) end
        local want_kind = (ckind == "locstr" and "str") or (ckind == "locmask" and "u32") or ckind
        if m.kind ~= want_kind then say("column %d (%s): the converter's kind %s, 097's %s", k, cname, ckind, m.kind) end
        -- the value read back, as 097's kind reads it
        local u = L.dbc_u32(d, 0, k)
        local got
        if m.kind == "str" then got = ffi.string(L.dbc_text(d, 0, k))
        elseif m.kind == "f32" then got = tonumber(ffi.cast("float *", ffi.new("uint32_t[1]", u))[0])
        elseif m.kind == "i32" then got = u >= 2 ^ 31 and u - 2 ^ 32 or u
        else got = tonumber(u) end
        local expect
        if ckind == "locmask" then expect = dbc.LOC_MASK
        else expect = want[cname] end
        if got ~= expect then say("column %d (%s): wrote %s, read back %s as 097's %s", k, cname, tostring(expect), tostring(got), m.kind) end
        if ckind == "locmask" and m.def ~= dbc.LOC_MASK then say("column %d (%s): the mask written 0x%X, 097's 0x%X", k, cname, dbc.LOC_MASK, m.def) end
    end
    -- the columns the converter says this client reads are ones 097 says are read
    for k, sname in pairs(conv.seen or {}) do
        local m = mine.cols[k]
        if not m then say("seen column %d (%s) is past 097's width", k, sname)
        elseif m.name ~= sname then say("seen column %d: the converter's %s, 097's %s", k, sname, m.name)
        elseif not m.read then say("seen column %d (%s): the converter says %s reads it; 097 marks it unread", k, sname, conv.seen_by or "the client") end
    end
    -- and the other way: columns 097 marks read that the converter's
    -- `seen` doesn't list (a note, not a failure: `seen` is the
    -- converter's account of this client, and may lag behind it)
    local notes = {}
    for k, m in ipairs(mine.cols) do
        if m.read and not (conv.seen and conv.seen[k]) and k <= #cols then notes[#notes + 1] = string.format("%d %s (%s)", k, m.name, m.read_by) end
    end
    return problems, notes
end
-- }}}

-- {{{ run
local failed, checked = 0, {}
local conv_names = {}
for name in pairs(dbc.LAYOUTS) do conv_names[#conv_names + 1] = name end
table.sort(conv_names)
for _, name in ipairs(conv_names) do
    local p, notes = compare(name)
    if notes and #notes > 0 then print(string.format("  note: %s: read here by 097, not in the converter's seen: %s", name, table.concat(notes, ", "))) end
    if #p == 0 then
        local mine = client_layout(name)
        print(string.format("  ok:   %s: %d fields, %d bytes a row; every column's name, kind and written value agree", name, mine.fields, mine.fields * 4))
        checked[#checked + 1] = name
    else
        failed = failed + 1
        print(string.format("  FAIL: %s: %d disagreement%s", name, #p, #p == 1 and "" or "s"))
        for _, s in ipairs(p) do print("          " .. s) end
    end
end
local only_client = {}
for _, name in ipairs(client_tables()) do if not dbc.LAYOUTS[name] then only_client[#only_client + 1] = name end end
print(string.format("agree: %d of %d tables the converter writes (%s)", #checked, #conv_names, table.concat(checked, ", ")))
print(string.format("097's tables the converter doesn't write (%d; their widths no converter file checks): %s", #only_client, table.concat(only_client, ", ")))
os.exit(failed == 0 and 0 or 1)
-- }}}
