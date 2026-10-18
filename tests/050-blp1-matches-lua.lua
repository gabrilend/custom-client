#!/usr/bin/env luajit
-- 050-blp1-matches-lua.lua - WC3's pictures, decoded by this client's C
-- reader and by world-edit-to-execute's Lua reader, compared byte by byte.
--
-- For a general audience: Warcraft III's pictures are now read by two
-- programs, the original one in Lua (world-edit-to-execute) and its C
-- twin here (issue 107). This opens WC3's own archives, decodes every
-- picture in them both ways, at full size and at the first smaller copy,
-- and counts how many come out identical to the last byte. Any picture
-- that differs, or that one reader accepts and the other refuses, is
-- named. The work is split across every processor core: each worker
-- takes every Nth picture, and the first process adds up their counts.
--
-- Usage: luajit tests/050-blp1-matches-lua.lua [PROJECT_DIR] [--workers N] [--limit N]
--   PROJECT_DIR  the W client (default below); its scripts/build must have run
--   --workers N  how many processes share the pictures (default: the core count)
--   --limit N    only the first N pictures of each archive (a quick look)
-- Exit status 0 when every picture agrees. The report also goes to
-- PROJECT_DIR/tmp/shared-memory/blp1-matches-lua.txt.
-- (internal: --part I N runs one worker and prints its counts)

-- {{{ set_dir
local DIR = "/mnt/kaun/custom-client"
local WETE = "/mnt/mtwo/programming/ai-stuff/world-edit-to-execute"
local ARCHIVES = { "war3.mpq", "War3x.mpq" }   -- WC3's base and Frozen Throne art
local args, i = { workers = nil, limit = nil }, 1
while i <= #arg do
    local a = arg[i]
    -- a dispatch over the options: each takes the next word as its value
    if a == "--workers" then args.workers = tonumber(arg[i + 1]); i = i + 2
    elseif a == "--limit" then args.limit = tonumber(arg[i + 1]); i = i + 2
    elseif a == "--part" then args.part, args.nparts = tonumber(arg[i + 1]), tonumber(arg[i + 2]); i = i + 3
    else DIR = a; i = i + 1 end
end
-- }}}

package.path = WETE .. "/src/?.lua;" .. WETE .. "/src/?/init.lua;" .. package.path
local ffi = require("ffi")

-- {{{ local function worker
-- One share of the pictures: every nparts-th, starting at part. Prints
-- one line per count and one per disagreement, for the first process.
local function worker(part, nparts)
    local mpq = require("mpq")
    local lua_blp = require("parsers.blp")
    local W = dofile(DIR .. "/scripts/libs/wreaders.lua")
    local L = W.load(DIR .. "/tmp/tmp/build")
    local img, why = ffi.new("BlpImage"), ffi.new("WWhy")
    local n, same, both_refused, differ = 0, 0, 0, 0
    for _, name in ipairs(ARCHIVES) do
        local a = mpq.open(WETE .. "/wc3-installs/frozen-throne/" .. name)
        local k = 0
        for _, f in ipairs(a:list()) do
            if f:lower():match("%.blp$") then
                k = k + 1
                if args.limit and k > args.limit then break end
                if k % nparts == part then
                    local bytes = a:extract(f)
                    if bytes:sub(1, 4) == "BLP1" then
                        for level = 0, 1 do
                            n = n + 1
                            local ok, lua_img = pcall(lua_blp.decode, bytes, level)
                            local e = L.blp_decode_level(bytes, #bytes, level, img, why)
                            local c_ok = e == 0
                            -- each pair lands in one of three outcomes: identical, both refused, or a disagreement
                            if ok and c_ok then
                                local c_rgba = ffi.string(img.rgba, img.width * img.height * 4)
                                if img.width == lua_img.width and img.height == lua_img.height and c_rgba == lua_img.rgba then
                                    same = same + 1
                                else
                                    differ = differ + 1
                                    print(string.format("differ\t%s\t%s\tlevel %d", name, f, level))
                                end
                            elseif not ok and not c_ok then
                                both_refused = both_refused + 1
                                print(string.format("refused\tlevel %d: %s", level, ffi.string(L.w_err_name(e))))
                            else
                                differ = differ + 1
                                print(string.format("differ\t%s\t%s\tlevel %d\tLua: %s\tC: %s", name, f, level,
                                    ok and "read" or tostring(lua_img), c_ok and "read" or ffi.string(why.text)))
                            end
                            if c_ok then L.blp_free(img) end
                        end
                    end
                end
            end
        end
        a:close()
    end
    print(string.format("counts\t%d\t%d\t%d\t%d", n, same, both_refused, differ))
end
-- }}}

-- {{{ local function cores
local function cores()
    local p = io.popen("nproc")
    local v = tonumber(p:read("*l"))
    p:close()
    return v
end
-- }}}

-- {{{ local function main
-- Starts the workers at once (each popen runs while the others do), then
-- reads them in turn and adds up.
local function main()
    if args.part then worker(args.part, args.nparts); return 0 end
    local nparts = args.workers or cores()
    local self = arg[0]
    local pipes = {}
    for p = 0, nparts - 1 do
        local cmd = string.format("luajit %q %q --part %d %d%s", self, DIR, p, nparts,
            args.limit and (" --limit " .. args.limit) or "")
        pipes[#pipes + 1] = io.popen(cmd)
    end
    local n, same, refused, differ, lines = 0, 0, 0, 0, {}
    local reported, refusals = 0, {}
    for _, pipe in ipairs(pipes) do
        for line in pipe:lines() do
            local a, b, c, d = line:match("^counts\t(%d+)\t(%d+)\t(%d+)\t(%d+)$")
            if a then
                n, same, refused, differ = n + a, same + b, refused + c, differ + d
                reported = reported + 1
            elseif line:match("^refused\t") then
                local why_refused = line:sub(9)
                refusals[why_refused] = (refusals[why_refused] or 0) + 1
            else
                lines[#lines + 1] = line
            end
        end
        pipe:close()
    end
    if reported ~= nparts then
        lines[#lines + 1] = string.format("ERROR: %d of %d workers reported (a worker failed; its error is above)", reported, nparts)
    end
    lines[#lines + 1] = string.format("%d WC3 pictures (BLP1, levels 0 and 1, %s): %d identical, %d refused by both, %d disagreeing",
        n, table.concat(ARCHIVES, " and "), same, refused, differ)
    -- the refusals both readers agree on, by level and reason (a picture
    -- with no smaller copy is refused at level 1 by both, rightly)
    local kinds = {}
    for k in pairs(refusals) do kinds[#kinds + 1] = k end
    table.sort(kinds)
    for _, k in ipairs(kinds) do lines[#lines + 1] = string.format("  refused by both, %s: %d", k, refusals[k]) end
    local report = table.concat(lines, "\n") .. "\n"
    io.write(report)
    local path = DIR .. "/tmp/shared-memory/blp1-matches-lua.txt"
    local f = io.open(path, "w")
    if not f then io.stderr:write("WARNING: the report could not be kept at " .. path .. " (run scripts/build, which makes tmp/)\n"); return 1 end
    f:write(report); f:close()
    return (differ == 0 and reported == nparts and n > 0) and 0 or 1
end
-- }}}

os.exit(main())
