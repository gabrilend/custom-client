#!/usr/bin/env luajit
-- 062-mdx-matches-lua.lua - WC3's models, read by this client's C reader
-- and by world-edit-to-execute's Lua reader, compared field by field.
--
-- For a general audience: Warcraft III's models are now read by two
-- programs, the original one in Lua (world-edit-to-execute's
-- parsers/mdx.lua, with assets/gpu.lua turning a model into pieces to
-- draw) and its C twin here (issue 401 step 2, src/061-mdx.c). This
-- hands both the same files and compares everything they give back:
-- every number of every point, triangle, picture, material layer,
-- animation, bone and animated value, which sequence each picks for a
-- name, what an animated value is at many moments, and the pieces each
-- would draw with their alpha. Models made here come first, built by
-- this script byte by byte (so the check runs with no game files); then,
-- when WC3's own archives are there, every model in them, split across
-- every processor core as tests/050-blp1-matches-lua.lua does.
--
-- Usage: luajit tests/062-mdx-matches-lua.lua [PROJECT_DIR] [--build DIR] [--made-only]
--                                             [--workers N] [--limit N]
--   PROJECT_DIR  the W client (default below)
--   --build DIR  where libwreaders.so and wreaders.h are (default PROJECT_DIR/tmp/tmp/build)
--   --made-only  only the models made here
--   --workers N  how many processes share WC3's models (default: the core count)
--   --limit N    only the first N models of each archive (a quick look)
-- Exit status 0 when every model agrees. With WC3's archives the report
-- also goes to PROJECT_DIR/tmp/shared-memory/mdx-matches-lua.txt.
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
    elseif a == "--build" then args.build = arg[i + 1]; i = i + 2
    elseif a == "--wete" then WETE = arg[i + 1]; i = i + 2
    elseif a == "--made-only" then args.made_only = true; i = i + 1
    elseif a == "--part" then args.part, args.nparts = tonumber(arg[i + 1]), tonumber(arg[i + 2]); i = i + 3
    else DIR = a; i = i + 1 end
end
args.build = args.build or (DIR .. "/tmp/tmp/build")
-- }}}

package.path = WETE .. "/src/?.lua;" .. WETE .. "/src/?/init.lua;" .. package.path
local ffi = require("ffi")
local bit = require("bit")
local mdx = require("parsers.mdx")
local gpu = require("assets.gpu")
local bw = require("parsers.binwrite")
local W = dofile(DIR .. "/scripts/libs/wreaders.lua")
local L = W.load(args.build)

-- {{{ comparing
-- Each comparison adds a line naming the first fields that differ; a
-- model agrees when it adds none. Numbers are compared exactly (a float
-- read by either side is the same double); NaN agrees with NaN.
local diffs
local function differ(path, a, b)
    if #diffs < 12 then diffs[#diffs + 1] = string.format("%s: Lua %s, C %s", path, tostring(a), tostring(b)) end
    diffs.count = (diffs.count or 0) + 1
end
local function same(path, a, b)
    if type(a) == "number" and type(b) == "number" then
        if a == b or (a ~= a and b ~= b) then return end
    elseif a == b then return end
    differ(path, a, b)
end
local function cstr(c) return ffi.string(c) end
local function f3(path, lua, c) for k = 1, 3 do same(path .. "[" .. k .. "]", lua[k], c[k - 1]) end end
local function extent(path, lua, c)
    same(path .. ".radius", lua.radius, c.radius)
    f3(path .. ".min", lua.min, c.min)
    f3(path .. ".max", lua.max, c.max)
end

-- the C tracks of a span, by tag (the last of a tag, as the Lua keeps it)
local function c_tracks(cm, span)
    local by, n = {}, 0
    for k = 0, span.count - 1 do
        local t = cm.tracks + (span.first + k)          -- a pointer, so mdx_track's answer can be compared with it
        local tag = ffi.string(t.tag, 4)
        if not by[tag] then n = n + 1 end
        by[tag] = t
    end
    return by, n
end
local function value(t, v, k)                      -- a C key's value as the Lua holds it
    if t.width == 0 then return v.u[0] end
    if t.width == 1 then return v.f[0] end
    return v.f[k - 1]
end
local function tracks(path, cm, lua, span)
    local by, n = c_tracks(cm, span)
    local ln = 0
    for tag in pairs(lua) do ln = ln + 1 end
    same(path .. " track count", ln, n)
    for tag, lt in pairs(lua) do
        local ct = by[tag]
        local found = L.mdx_track(cm, span, tag)
        if ct and (found == nil or ffi.cast("uintptr_t", found) ~= ffi.cast("uintptr_t", ct)) then
            differ(path .. " mdx_track(" .. tag .. ")", "the last " .. tag, "another")
        end
        if not ct then differ(path .. "." .. tag, "present", "missing")
        else
            same(path .. "." .. tag .. ".interpolation", lt.interpolation, ct.interpolation)
            same(path .. "." .. tag .. ".global", lt.global or -1, ct.global)
            same(path .. "." .. tag .. " keys", #lt.keys, ct.keys.count)
            for k = 1, math.min(#lt.keys, ct.keys.count) do
                local lk, ck = lt.keys[k], cm.keys[ct.keys.first + k - 1]
                local p = path .. "." .. tag .. "[" .. k .. "]"
                same(p .. ".frame", lk.frame, ck.frame)
                local fields = { { "value", lk.value, ck.value } }
                if lt.interpolation > 1 then
                    fields[2] = { "inTan", lk.inTan, ck["in"] }
                    fields[3] = { "outTan", lk.outTan, ck.out }
                end
                for _, f in ipairs(fields) do
                    if type(f[2]) == "table" then
                        same(p .. "." .. f[1] .. " width", #f[2], ct.width)
                        for c = 1, #f[2] do same(p .. "." .. f[1] .. "[" .. c .. "]", f[2][c], value(ct, f[3], c)) end
                    else
                        same(p .. "." .. f[1], f[2], value(ct, f[3], 1))
                    end
                end
            end
        end
    end
end

local KIND_LISTS = { [0] = "bones", "helpers", "attachments", "lights", "particle_emitters", "particle_emitters2",
                     "ribbons", "events", "collision" }

-- {{{ local function compare_read: mdx.parse's model against the C model
local function compare_read(lm, cm)
    same("version", lm.version or 0, cm.version)
    same("name", lm.name or "", cstr(cm.name))
    same("animation_file", lm.animation_file or "", cstr(cm.animation_file))
    if lm.extent then extent("extent", lm.extent, cm.extent) end
    same("blend_time", lm.blend_time or 0, cm.blend_time)
    same("chunks", table.concat(lm.chunks, " "), (ffi.string(cm.chunks, cm.nchunks * 4):gsub("(....)", "%1 "):gsub(" $", "")))

    same("sequence count", #lm.sequences, cm.nsequences)
    for k = 1, math.min(#lm.sequences, cm.nsequences) do
        local l, c, p = lm.sequences[k], cm.sequences[k - 1], "sequences[" .. k .. "]"
        same(p .. ".name", l.name, cstr(c.name))
        same(p .. ".start", l.start, c.start); same(p .. ".finish", l.finish, c.finish)
        same(p .. ".move_speed", l.move_speed, c.move_speed)
        same(p .. ".non_looping", l.non_looping, c.non_looping == 1)
        same(p .. ".rarity", l.rarity, c.rarity); same(p .. ".sync_point", l.sync_point, c.sync_point)
        extent(p .. ".extent", l.extent, c.extent)
    end
    local glbs = lm.global_sequences or {}
    same("global sequence count", #glbs, cm.nglobal_sequences)
    for k = 1, math.min(#glbs, cm.nglobal_sequences) do same("global_sequences[" .. k .. "]", glbs[k], cm.global_sequences[k - 1]) end

    same("texture count", #lm.textures, cm.model.ntextures)
    for k = 1, math.min(#lm.textures, cm.model.ntextures) do
        local l, c, p = lm.textures[k], cm.model.textures[k - 1], "textures[" .. k .. "]"
        same(p .. ".replaceable", l.replaceable, c.kind == 0 and 0 or c.kind - L.M2_TEXTURE_WC3)
        same(p .. ".path", l.path, cstr(c.file))
        same(p .. ".flags", l.flags, c.flags)
    end

    same("material count", #lm.materials, cm.nmaterials)
    for k = 1, math.min(#lm.materials, cm.nmaterials) do
        local l, c, p = lm.materials[k], cm.materials[k - 1], "materials[" .. k .. "]"
        same(p .. ".priority_plane", l.priority_plane, c.priority_plane)
        same(p .. ".flags", l.flags, c.flags)
        same(p .. ".shader", l.shader or "", cstr(c.shader))
        same(p .. " layer count", #l.layers, c.layers.count)
        for j = 1, math.min(#l.layers, c.layers.count) do
            local ll, cl, q = l.layers[j], cm.layers[c.layers.first + j - 1], p .. ".layers[" .. j .. "]"
            for _, f in ipairs({ "filter_mode", "shading", "texture", "texture_animation", "coord", "alpha" }) do
                same(q .. "." .. f, ll[f], cl[f])
            end
            same(q .. ".emissive", ll.emissive or 0, cl.emissive)
            f3(q .. ".fresnel", ll.fresnel or { 0, 0, 0 }, cl.fresnel)
            same(q .. ".fresnel_opacity", ll.fresnel_opacity or 0, cl.fresnel_opacity)
            same(q .. ".fresnel_team", ll.fresnel_team or 0, cl.fresnel_team)
            tracks(q, cm, ll.tracks, cl.tracks)
        end
    end

    same("geoset count", #lm.geosets, cm.ngeosets)
    for k = 1, math.min(#lm.geosets, cm.ngeosets) do
        local l, c, p = lm.geosets[k], cm.geosets[k - 1], "geosets[" .. k .. "]"
        local nv = c.vertices.count
        same(p .. " point count", #l.vertices / 3, nv)
        same(p .. " normal count", #l.normals / 3, c.normals_given)
        for v = 0, math.min(#l.vertices / 3, nv) - 1 do
            local cv = cm.model.vertices[c.vertices.first + v]
            same(p .. ".vertices[" .. v .. "].x", l.vertices[v * 3 + 1], cv.x)
            same(p .. ".vertices[" .. v .. "].y", l.vertices[v * 3 + 2], cv.y)
            same(p .. ".vertices[" .. v .. "].z", l.vertices[v * 3 + 3], cv.z)
            if v < #l.normals / 3 then
                same(p .. ".normals[" .. v .. "].x", l.normals[v * 3 + 1], cv.nx)
                same(p .. ".normals[" .. v .. "].y", l.normals[v * 3 + 2], cv.ny)
                same(p .. ".normals[" .. v .. "].z", l.normals[v * 3 + 3], cv.nz)
            end
        end
        local lists = { { "face_types", "face_types" }, { "face_counts", "face_counts" }, { "faces", "faces" },
                        { "vertex_groups", "vertex_groups" }, { "group_sizes", "group_sizes" }, { "group_indices", "group_indices" } }
        for _, f in ipairs(lists) do
            local lt, span, arr = l[f[1]], c[f[2]], cm[f[2]]
            same(p .. "." .. f[1] .. " count", #lt, span.count)
            for j = 1, math.min(#lt, span.count) do same(p .. "." .. f[1] .. "[" .. j .. "]", lt[j], arr[span.first + j - 1]) end
        end
        same(p .. ".material", l.material, c.material)
        same(p .. ".selection_group", l.selection_group, c.selection_group)
        same(p .. ".selection_flags", l.selection_flags, c.selection_flags)
        same(p .. ".lod", l.lod or 0, c.lod)
        same(p .. ".lod_name", l.lod_name or "", cstr(c.lod_name))
        extent(p .. ".extent", l.extent, c.extent)
        same(p .. " sequence extent count", #l.sequence_extents, c.sequence_extents.count)
        for j = 1, math.min(#l.sequence_extents, c.sequence_extents.count) do
            extent(p .. ".sequence_extents[" .. j .. "]", l.sequence_extents[j], cm.sequence_extents[c.sequence_extents.first + j - 1])
        end
        same(p .. " uv set count", #l.uvs, c.uv_sets)
        for s = 1, math.min(#l.uvs, 2) do
            local set = l.uvs[s]
            same(p .. ".uvs[" .. s .. "] count", #set / 2, c.uv_given[s - 1])
            for v = 0, math.min(#set / 2, nv) - 1 do
                local cv = cm.model.vertices[c.vertices.first + v]
                same(p .. ".uvs[" .. s .. "][" .. v .. "].u", set[v * 2 + 1], s == 1 and cv.u or cv.u2)
                same(p .. ".uvs[" .. s .. "][" .. v .. "].v", set[v * 2 + 2], s == 1 and cv.v or cv.v2)
            end
        end
    end

    same("geoset animation count", #lm.geoset_animations, cm.ngeoset_animations)
    for k = 1, math.min(#lm.geoset_animations, cm.ngeoset_animations) do
        local l, c, p = lm.geoset_animations[k], cm.geoset_animations[k - 1], "geoset_animations[" .. k .. "]"
        same(p .. ".alpha", l.alpha, c.alpha); same(p .. ".flags", l.flags, c.flags)
        f3(p .. ".color", l.color, c.color); same(p .. ".geoset", l.geoset, c.geoset)
        tracks(p, cm, l.tracks, c.tracks)
    end

    -- nodes: the C keeps one list in the file's order; the Lua one a kind
    local by_kind = {}
    for k = 0, cm.nnodes - 1 do
        local n = cm.nodes[k]
        by_kind[n.kind] = by_kind[n.kind] or {}
        local t = by_kind[n.kind]
        t[#t + 1] = n
    end
    for kind, field in pairs(KIND_LISTS) do
        local ll, cl = lm[field] or {}, by_kind[kind] or {}
        same(field .. " count", #ll, #cl)
        for k = 1, math.min(#ll, #cl) do
            local l, c, p = ll[k], cl[k], field .. "[" .. k .. "]"
            same(p .. ".name", l.name, cstr(c.name)); same(p .. ".id", l.id, c.id)
            same(p .. ".parent", l.parent, c.parent); same(p .. ".flags", l.flags, c.flags)
            tracks(p, cm, l.tracks, c.tracks)
            if kind == L.MDX_BONE then
                same(p .. ".geoset", l.geoset, c.geoset); same(p .. ".geoset_animation", l.geoset_animation, c.geoset_animation)
            elseif kind == L.MDX_ATTACHMENT then
                same(p .. ".path", l.path, cstr(c.path)); same(p .. ".attachment_id", l.attachment_id, c.attachment_id)
            elseif kind == L.MDX_EVENT then
                same(p .. " has frames", l.frames ~= nil, c.has_frames == 1)
                if l.frames then
                    same(p .. ".global", l.global, c.event_global)
                    same(p .. " frame count", #l.frames, c.event_frames.count)
                    for j = 1, math.min(#l.frames, c.event_frames.count) do
                        same(p .. ".frames[" .. j .. "]", l.frames[j], cm.event_frames[c.event_frames.first + j - 1])
                    end
                end
            elseif kind == L.MDX_COLLISION then
                same(p .. ".shape", l.shape, c.shape)
                same(p .. " point count", #l.points, c.npoints)
                for j = 1, math.min(#l.points, c.npoints) do f3(p .. ".points[" .. j .. "]", l.points[j], c.points[j - 1]) end
                same(p .. ".radius", l.radius or 0, c.radius)
            end
        end
    end
    same("pivot count", #lm.pivots, cm.npivots)
    for k = 1, math.min(#lm.pivots, cm.npivots) do f3("pivots[" .. k .. "]", lm.pivots[k], cm.pivots[k - 1]) end
    local txan = lm.texture_animations or {}
    same("texture animation count", #txan, cm.ntexture_animations)
    for k = 1, math.min(#txan, cm.ntexture_animations) do tracks("texture_animations[" .. k .. "]", cm, txan[k].tracks, cm.texture_animations[k - 1]) end
end
-- }}}

-- {{{ local function compare_helpers: mdx.sequence, mdx.nodes, mdx.sample
local function compare_helpers(lm, cm)
    for _, name in ipairs({ "stand", "walk", "attack", "death", "stand ready", "Birth", "portrait", "no such" }) do
        local ls = mdx.sequence(lm, name)
        local idx = -1
        for k, s in ipairs(lm.sequences) do if s == ls then idx = k - 1 end end
        same("mdx.sequence(" .. name .. ")", idx, L.mdx_sequence(cm, name))
    end
    for id, n in pairs(mdx.nodes(lm)) do
        local k = L.mdx_node(cm, id)
        if k < 0 then differ("mdx.nodes[" .. id .. "]", n.name, "none")
        else same("mdx.nodes[" .. id .. "].name", n.name, cstr(cm.nodes[k].name)) end
    end
    same("mdx_node(no such id)", -1, L.mdx_node(cm, 0x7FFFFFF0))
    -- every track sampled at each key, between keys, outside them, with
    -- and without each sequence's bounds; rotations along the sphere
    local out, def = ffi.new("double[4]"), ffi.new("double[4]", { -7, -8, -9, -10 })
    local checked = 0
    local function sample_all(path, lua_tracks, span)
        local by = c_tracks(cm, span)
        for tag, lt in pairs(lua_tracks) do
            local ct = by[tag]
            if ct then
                local rotation = tag == "KGRT" or tag == "KTAR"
                local width = ct.width == 0 and 1 or ct.width
                local ldef
                if width == 1 then ldef = -7 else ldef = {} for c = 1, width do ldef[c] = def[c - 1] end end
                local frames = { -100, 0 }
                for _, k in ipairs(lt.keys) do
                    frames[#frames + 1] = k.frame
                    frames[#frames + 1] = k.frame + 0.5
                    frames[#frames + 1] = k.frame + 17
                end
                local bounds = { false }
                for _, s in ipairs(lm.sequences) do bounds[#bounds + 1] = s end
                for _, b in ipairs(bounds) do
                    for _, f in ipairs(frames) do
                        local lv = mdx.sample(lt, f, ldef, b and b.start, b and b.finish, rotation)
                        local n = L.mdx_sample(cm, ct, f, def, width, b and 1 or 0, b and b.start or 0, b and b.finish or 0, rotation and 1 or 0, out)
                        local p = string.format("mdx.sample(%s.%s, %s, %s)", path, tag, tostring(f), b and b.name or "unbounded")
                        if type(lv) == "table" then
                            same(p .. " width", #lv, n)
                            for c = 1, #lv do same(p .. "[" .. c .. "]", lv[c], out[c - 1]) end
                        else
                            same(p, lv, out[0])
                        end
                        checked = checked + 1
                    end
                end
            end
        end
    end
    for k, n in ipairs(lm.bones) do sample_all("bones[" .. k .. "]", n.tracks, (function()
        local seen = 0
        for j = 0, cm.nnodes - 1 do
            if cm.nodes[j].kind == L.MDX_BONE then seen = seen + 1; if seen == k then return cm.nodes[j].tracks end end
        end
        return { first = 0, count = 0 }
    end)()) end
    for k, n in ipairs(lm.helpers) do sample_all("helpers[" .. k .. "]", n.tracks, (function()
        local seen = 0
        for j = 0, cm.nnodes - 1 do
            if cm.nodes[j].kind == L.MDX_HELPER then seen = seen + 1; if seen == k then return cm.nodes[j].tracks end end
        end
        return { first = 0, count = 0 }
    end)()) end
    for k, a in ipairs(lm.geoset_animations) do
        if k <= cm.ngeoset_animations then sample_all("geoset_animations[" .. k .. "]", a.tracks, cm.geoset_animations[k - 1].tracks) end
    end
    for k, mat in ipairs(lm.materials) do
        if k <= cm.nmaterials then
            for j, layer in ipairs(mat.layers) do
                local cmat = cm.materials[k - 1]
                if j <= cmat.layers.count then sample_all("materials[" .. k .. "].layers[" .. j .. "]", layer.tracks, cm.layers[cmat.layers.first + j - 1].tracks) end
            end
        end
    end
    for k, t in ipairs(lm.texture_animations or {}) do
        if k <= cm.ntexture_animations then sample_all("texture_animations[" .. k .. "]", t.tracks, cm.texture_animations[k - 1]) end
    end
    return checked
end
-- }}}

-- {{{ local function compare_drawn: gpu.lua's parts against the C units
-- gpu.lua builds through a renderer and an asset source; here both are
-- stand-ins that keep what they're handed, and every picture is found
-- (the reader leaves a missing picture to the drawing).
local function stand_ins()
    local got = {}
    local render = {
        tex_create = function(w, h, rgba, wrap) return { rgba = rgba, wrap = wrap } end,
        mesh_create = function(verts, idx, skin) return { verts = verts, idx = idx, skin = skin } end,
        model_create = function(parts, groups) got.parts = parts; return 1 end,
    }
    local assets = { texture = function(self, path) return { width = 1, height = 1, rgba = "picture " .. path:lower() } end }
    return gpu.new(render, assets), got
end

local function compare_drawn(lm, cm)
    local cache, got = stand_ins()
    local id = cache:build(lm, "made")
    local parts = id and got.parts or {}
    local d = cm.model
    same("parts drawn", #parts, d.nunits)
    -- each Lua layer table's place in the C's one list of layers
    local layer_at = {}
    local at = 0
    for _, mat in ipairs(lm.materials) do for _, layer in ipairs(mat.layers) do layer_at[layer] = at; at = at + 1 end end
    local meta = id and cache.meta[id].parts or {}
    for k = 1, math.min(#parts, d.nunits) do
        local lp, u, p = parts[k], d.units[k - 1], "parts[" .. k .. "]"
        same(p .. ".geoset", meta[k].geoset, u.piece)
        same(p .. ".layer", layer_at[meta[k].layer] or -1, cm.unit_layer[k - 1])
        same(p .. ".filter", lp.filter, u.blend)
        same(p .. ".two_sided", lp.two_sided, bit.band(u.flags, 0x4) ~= 0)
        same(p .. ".unshaded", lp.unshaded, bit.band(u.flags, 0x1) ~= 0)
        same(p .. ".alpha", tonumber(ffi.new("float", lp.alpha)), u.alpha)
        local t = u.tex[0] >= 0 and d.textures[u.tex[0]] or nil
        local team = t and (t.kind == L.M2_TEXTURE_WC3 + 1 or t.kind == L.M2_TEXTURE_WC3 + 2) and 1 or 0
        same(p .. ".team", lp.team, team)
        if t and t.kind == 0 and t.file[0] ~= 0 then same(p .. " picture", lp.tex.rgba, "picture " .. cstr(t.file):lower()) end
        -- the mesh: gpu.lua's interleaved points and its geoset's own corner numbers
        local piece = d.pieces[u.piece]
        local g = cm.geosets[u.piece]
        local nv = #lp.mesh.verts / 32
        same(p .. " mesh points", nv, g.vertices.count)
        local fl = ffi.cast("const float *", lp.mesh.verts)
        for v = 0, math.min(nv, g.vertices.count) - 1 do
            local cv = d.vertices[g.vertices.first + v]
            local want = { cv.x, cv.y, cv.z, cv.nx, cv.ny, cv.nz, cv.u, cv.v }
            for c = 1, 8 do same(p .. ".verts[" .. v .. "][" .. c .. "]", fl[v * 8 + c - 1], want[c]) end
        end
        local ix = ffi.cast("const uint16_t *", lp.mesh.idx)
        same(p .. " mesh corners", #lp.mesh.idx / 2, piece.count)
        -- a corner past the geoset's points: gpu.lua hands it to the card
        -- as it is (a read past the mesh); the C takes the geoset's first
        -- point instead, as m2_read does (decided in issue 401 step 2)
        for c = 0, math.min(#lp.mesh.idx / 2, piece.count) - 1 do
            local want = ix[c] < g.vertices.count and ix[c] or 0
            if want ~= ix[c] then diffs.decided = (diffs.decided or 0) + 1 end
            same(p .. ".idx[" .. c .. "]", want, d.indices[piece.first + c] - g.vertices.first)
        end
    end
end
-- }}}

-- {{{ local function check_one: both readers on one file's bytes
-- "same" (every field agrees), "refused" (both refuse it), or "differ"
-- with the lines saying where.
local function check_one(bytes)
    diffs = {}
    local ok, lm = pcall(mdx.parse, bytes)
    local cm, why = ffi.new("MdxModel"), ffi.new("WWhy")
    local e = L.mdx_read(bytes, #bytes, cm, why)
    local samples = 0
    if ok and e == 0 then
        compare_read(lm, cm)
        samples = compare_helpers(lm, cm)
        local drawn_ok, err = pcall(compare_drawn, lm, cm)
        if not drawn_ok then differ("gpu.lua's build", "failed: " .. tostring(err), "read") end
        L.mdx_free(cm)
        return (diffs.count or 0) == 0 and "same" or "differ", diffs, samples
    elseif not ok and e ~= 0 then
        return "refused", { "Lua: " .. tostring(lm), "C: " .. ffi.string(why.text) }, 0
    end
    if e == 0 then L.mdx_free(cm) end
    return "differ", { "Lua: " .. (ok and "read" or tostring(lm)), "C: " .. (e == 0 and "read" or ffi.string(why.text)) }, 0
end
-- }}}
-- }}}

-- {{{ models made here
-- Writers for the format's pieces: a chunk is its tag, its byte count
-- and its body; a record with a size of its own counts itself.
local function u32(v) return bw.new():u32(v):done() end
local function i32(v) return bw.new():i32(v):done() end
local function f32(v) return bw.new():f32(v):done() end
local function u16(v) return bw.new():u16(v):done() end
local function fl(t) local b = bw.new() for _, v in ipairs(t) do b:f32(v) end return b:done() end
local function name(s, len) return s .. string.rep("\0", len - #s) end
local function chunk(tag, body) return tag .. u32(#body) .. body end
local function sized(body) return u32(#body + 4) .. body end
local function ext(r) return fl({ r, -r, -r, -r, r, r, r }) end
-- a track: tag, way, global (-1 none), keys { frame, value (number or table), in, out }
local function track(tag, way, global, keys, int)
    local parts = { tag, u32(#keys), u32(way), i32(global) }
    local function v(x)
        if int then return u32(x) end
        if type(x) == "table" then return fl(x) end
        return f32(x)
    end
    for _, k in ipairs(keys) do
        parts[#parts + 1] = i32(k[1]) .. v(k[2])
        if way > 1 then parts[#parts + 1] = v(k[3]) .. v(k[4]) end
    end
    return table.concat(parts)
end
local function node(nm, id, parent, flags, trk) return sized(name(nm, 80) .. i32(id) .. i32(parent) .. u32(flags) .. (trk or "")) end

-- One model with every chunk both readers know and two they don't
-- (CAMS, which only the Lua reads, and XXXX, which neither does).
-- `v` the version (800, the shipped one; 900 adds the Reforged fields
-- both read); `cut` the bytes to keep (a damaged copy).
local function made(v)
    local s = math.sqrt(0.5)
    local near_z = math.sqrt(1 - 0.99955 ^ 2)    -- two turns this close (0.9995 to 0.9996) go along the sphere, not a line
    local out = { "MDLX", chunk("VERS", u32(v)) }
    out[#out + 1] = chunk("MODL", name("Made", 80) .. name("", 260) .. ext(150) .. u32(150))
    local seqs = {
        { "Stand 1", 0, 1000, 0, 0 }, { "Walk", 1100, 2100, 270, 0 }, { "Stand Ready", 2200, 3000, 0, 0 },
        { "Death", 3100, 4000, 0, 1 }, { "Attack - 2", 4100, 4600, 0, 0 },
    }
    local sb = {}
    for _, q in ipairs(seqs) do
        sb[#sb + 1] = name(q[1], 80) .. u32(q[2]) .. u32(q[3]) .. f32(q[4]) .. u32(q[5]) .. f32(0.25) .. u32(0) .. ext(100)
    end
    out[#out + 1] = chunk("SEQS", table.concat(sb))
    out[#out + 1] = chunk("GLBS", u32(1500) .. u32(333))
    out[#out + 1] = chunk("TEXS",
        u32(1) .. name("", 260) .. u32(0) ..                                   -- team colour
        u32(0) .. name("Textures\\Made.blp", 260) .. u32(3) ..                 -- the skin, wrapping both ways
        u32(2) .. name("", 260) .. u32(0) ..                                   -- team glow
        u32(31) .. name("", 260) .. u32(1) ..                                  -- a tree's leaves
        u32(0) .. name("Textures\\Glow.blp", 260) .. u32(2))
    local function layer(filter, shading, tex, txan, coord, alpha, trk)
        local extra = v > 800 and (f32(0.5) .. fl({ 1, 0.5, 0.25 }) .. f32(0.75) .. f32(0.125)) or ""
        return sized(u32(filter) .. u32(shading) .. u32(tex) .. i32(txan) .. u32(coord) .. f32(alpha) .. extra .. (trk or ""))
    end
    local function material(pp, flags, layers)
        return sized(i32(pp) .. u32(flags) .. (v > 800 and name("Shader_HD", 80) or "") .. "LAYS" .. u32(#layers) .. table.concat(layers))
    end
    out[#out + 1] = chunk("MTLS",
        material(0, 0, {
            layer(0, 0, 0, -1, 0, 1),                                          -- team colour under
            layer(2, 0, 1, -1, 0, 0.8, track("KMTA", 1, -1, { { 0, 0.6 }, { 1000, 0.2 }, { 2200, 0.9 } })),
        }) ..
        material(1, 1, {
            layer(3, 1 + 16 + 32, 4, 0, 1, 1, track("KMTF", 0, -1, { { 0, 4 }, { 500, 1 } }, true) ..
                  track("KMTA", 0, -1, { { 0, 0.3 } }) .. track("KMTA", 0, -1, { { 0, 0.7 } })),   -- one tag twice: the last counts
        }) ..
        material(-1, 0, {
            layer(5, 2 + 64 + 128, 3, -1, 0, 0.5, track("KMTA", 2, 0, { { 100, 0.1, 0, 0 }, { 900, 0.9, 1, 1 } })),
            layer(6, 0, 7, 3, 0, 1),                                           -- a picture that isn't there
            layer(4, 16, 2, -1, 2, 1),
        }))
    -- geosets: points, normals, faces, groups, material, then picture positions
    local function geoset(g)
        local nv = #g.v / 3
        local b = { "VRTX" .. u32(nv) .. fl(g.v), "NRMS" .. u32(#g.n / 3) .. fl(g.n) }
        local types, counts = {}, {}
        for _, f in ipairs(g.faces) do types[#types + 1] = u32(f[1]); counts[#counts + 1] = u32(#f[2]) end
        local idx = {}
        for _, f in ipairs(g.faces) do for _, k in ipairs(f[2]) do idx[#idx + 1] = u16(k) end end
        b[#b + 1] = "PTYP" .. u32(#types) .. table.concat(types)
        b[#b + 1] = "PCNT" .. u32(#counts) .. table.concat(counts)
        b[#b + 1] = "PVTX" .. u32(#idx) .. table.concat(idx)
        local groups = {}
        for _, k in ipairs(g.groups) do groups[#groups + 1] = string.char(k) end
        b[#b + 1] = "GNDX" .. u32(#groups) .. table.concat(groups)
        b[#b + 1] = "MTGC" .. u32(#g.sizes) .. table.concat((function() local t = {} for _, k in ipairs(g.sizes) do t[#t + 1] = u32(k) end return t end)())
        b[#b + 1] = "MATS" .. u32(#g.mats) .. table.concat((function() local t = {} for _, k in ipairs(g.mats) do t[#t + 1] = u32(k) end return t end)())
        b[#b + 1] = u32(g.material) .. u32(g.sel or 0) .. u32(g.selflags or 0)
        if v > 800 then b[#b + 1] = u32(1) .. name("LOD one", 80) end
        b[#b + 1] = ext(50) .. u32(2) .. ext(10) .. ext(20)
        if v > 800 then b[#b + 1] = "TANG" .. u32(nv) .. string.rep(fl({ 1, 0, 0, 1 }), nv) .. "SKIN" .. u32(nv * 8) .. string.rep("\1", nv * 8) end
        b[#b + 1] = "UVAS" .. u32(#g.uv)
        for _, set in ipairs(g.uv) do b[#b + 1] = "UVBS" .. u32(#set / 2) .. fl(set) end
        return sized(table.concat(b))
    end
    out[#out + 1] = chunk("GEOS",
        geoset({ v = { 0, 0, 0, 10, 0, 0, 10, 10, 0, 0, 10, 0 }, n = { 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1 },
                 faces = { { 4, { 0, 1, 2, 0, 2, 3 } } }, groups = { 0, 0, 1, 1 }, sizes = { 1, 2 }, mats = { 0, 0, 1 },
                 material = 0, sel = 3, selflags = 4, uv = { { 0, 0, 1, 0, 1, 1, 0, 1 } } }) ..
        -- a strip (kind 5) before its triangles, two sets of picture
        -- positions, fewer normals than points, a corner past its points
        geoset({ v = { 0, 0, 5, 5, 0, 5, 5, 5, 5 }, n = { 1, 0, 0 },
                 faces = { { 5, { 0, 1, 2 } }, { 4, { 2, 1, 0, 0, 1, 9 } } }, groups = { 0, 0, 0 }, sizes = { 1 }, mats = { 1 },
                 material = 1, uv = { { 0, 0, 1, 0, 1, 1 }, { 0.5, 0.5, 0.25, 0.25, 0.75, 0.75 } } }) ..
        geoset({ v = { 1, 2, 3, 4, 5, 6, 7, 8, 9 }, n = { 0, 1, 0, 0, 1, 0, 0, 1, 0 },
                 faces = { { 4, { 0, 1, 2, 1 } } }, groups = { 0, 0, 0 }, sizes = { 1 }, mats = { 0 },
                 material = 2, uv = { { 0, 0, 0, 0, 0, 0 } } }) ..
        geoset({ v = { 1, 1, 1, 2, 2, 2 }, n = { 0, 0, 1, 0, 0, 1 },                -- too few corners: nothing drawn
                 faces = { { 4, { 0, 1 } } }, groups = { 0, 0 }, sizes = { 1 }, mats = { 0 },
                 material = 0, uv = { { 0, 0, 1, 1 } } }) ..
        geoset({ v = { 3, 3, 3, 4, 4, 4, 5, 5, 3 }, n = { 0, 0, 1, 0, 0, 1, 0, 0, 1 },  -- no such material: one plain layer
                 faces = { { 4, { 0, 1, 2 } } }, groups = { 0, 0, 0 }, sizes = { 1 }, mats = { 0 },
                 material = 9, uv = { { 0, 0, 1, 0, 1, 1 } } }))
    out[#out + 1] = chunk("GEOA",
        sized(f32(1) .. u32(0) .. fl({ 1, 1, 1 }) .. u32(1) .. track("KGAO", 0, -1, { { 0, 0 }, { 1100, 1 }, { 3100, 1 }, { 3500, 0 } })) ..
        sized(f32(0.5) .. u32(1) .. fl({ 1, 0.5, 0.25 }) .. u32(2) .. track("KGAC", 1, -1, { { 0, { 1, 0, 0 } }, { 1000, { 0, 1, 0 } } })) ..
        sized(f32(0.25) .. u32(0) .. fl({ 1, 1, 1 }) .. u32(0)) ..
        sized(f32(0.75) .. u32(0) .. fl({ 1, 1, 1 }) .. u32(2)))      -- a second for geoset 2: the last decides
    out[#out + 1] = chunk("BONE",
        node("root", 0, -1, 256, track("KGTR", 1, -1, { { 1100, { 0, 0, 0 } }, { 2100, { 100, 0, 0 } } })) .. i32(0) .. i32(-1) ..
        node("arm", 1, 0, 256,
            track("KGRT", 1, -1, { { 0, { 0, 0, 0, 1 } }, { 500, { 0, 0, s, s } }, { 1000, { 0, 0, 1, 0 } },
                                    { 1100, { 0, 0, 0, 1 } }, { 1101, { 0, 0, 0.0001, 0.99999999 } },
                                    { 1102, { 0, 0, near_z, 0.99955 } }, { 1103, { 0, 0, 0.6, -0.8 } } }) ..   -- the last two face apart: the shorter way round
            track("KGSC", 2, 1, { { 0, { 1, 1, 1 }, { 0, 0, 0 }, { 0, 0, 0 } }, { 333, { 2, 2, 2 }, { 1, 1, 1 }, { 1, 1, 1 } } }))
            .. i32(1) .. i32(0))
    out[#out + 1] = chunk("HELP", node("hand ref", 2, 1, 0, track("KGTR", 0, -1, { { 0, { 0, 0, 1 } } }) ..
        track("KGRT", 3, -1, { { 2200, { 0, 0, 0, 1 }, { 0, 0, 0, 1 }, { 0, 0, 0, 1 } }, { 3000, { 0, 0, -s, s }, { 0, 0, 0, 1 }, { 0, 0, 0, 1 } } })))
    out[#out + 1] = chunk("ATCH", sized(node("Origin Ref", 3, -1, 2048) .. name("", 260) .. u32(0) .. track("KATV", 0, -1, { { 0, 1 }, { 3100, 0 } })))
    out[#out + 1] = chunk("PRE2", sized(node("dust", 4, 0, 4096, track("KGTR", 1, -1, { { 0, { 0, 0, 0 } } })) .. string.rep("\7", 40)))
    out[#out + 1] = chunk("EVTS", node("SNDxDEAT", 5, -1, 1024) .. "KEVT" .. u32(2) .. i32(-1) .. u32(3100) .. u32(3500) ..
                                  node("FTPxLEFT", 6, 1, 1024))
    out[#out + 1] = chunk("CLID", node("box", 7, -1, 8192) .. u32(0) .. fl({ -1, -1, -1, 1, 1, 1 }) ..
                                  node("ball", 8, 0, 8192) .. u32(2) .. fl({ 0, 0, 50 }) .. f32(40) ..
                                  node("can", 9, 0, 8192) .. u32(3) .. fl({ 0, 0, 0, 0, 0, 90 }) .. f32(20))
    out[#out + 1] = chunk("CAMS", sized(name("Portrait", 80) .. fl({ 100, 0, 80 }) .. f32(0.7) .. f32(1000) .. f32(8) .. fl({ 0, 0, 60 })))
    out[#out + 1] = chunk("TXAN", sized(track("KTAT", 1, -1, { { 0, { 0, 0, 0 } }, { 1000, { 1, 0, 0 } } })) ..
                                  sized(track("KTAR", 1, 0, { { 0, { 0, 0, 0, 1 } }, { 1500, { 0, 0, 1, 0 } } })) ..
                                  sized(track("KTAS", 0, -1, { { 0, { 1, 1, 1 } } })) ..
                                  sized(""))
    out[#out + 1] = chunk("XXXX", "anything at all")
    local piv = {}
    for k = 0, 9 do piv[#piv + 1] = fl({ k, k * 2, k * 3 }) end
    out[#out + 1] = chunk("PIVT", table.concat(piv))
    return table.concat(out)
end

-- The made models: both versions whole, and damaged copies both must refuse
local function made_models()
    local m800, m900 = made(800), made(900)
    local list = {
        { "version 800, every chunk", m800, "same" },
        { "version 900 (Reforged's added fields)", m900, "same" },
        { "an empty model (MDLX alone)", "MDLX", "same" },
        { "a model that is only VERS and MODL", "MDLX" .. chunk("VERS", u32(800)) .. chunk("MODL", name("bare", 80) .. name("", 260) .. ext(1) .. u32(0)), "same" },
        { "not a model", "MDLY" .. m800:sub(5), "refused" },
        { "a track no record has (KZZZ)", (m800:gsub("KGAC", "KZZZ")), "refused" },
        { "a geoset whose NRMS is misnamed", (m800:gsub("NRMS", "NRMX", 1)), "refused" },
        { "a material without LAYS", (m800:gsub("LAYS", "LAYX", 1)), "refused" },
    }
    -- cut short at every 97th byte: every one is refused by both (the
    -- C names where; the Lua reads past its end)
    for cut = 9, #m800 - 1, 97 do list[#list + 1] = { "cut to " .. cut .. " bytes", m800:sub(1, cut), "either" } end
    return list
end
-- }}}

-- {{{ local function made_run: the models made here, each to its expected outcome
local function made_run()
    local n, good, samples = 0, 0, 0
    for _, case in ipairs(made_models()) do
        n = n + 1
        local outcome, lines, s = check_one(case[2])
        samples = samples + s
        local ok = outcome == case[3] or (case[3] == "either" and outcome ~= "differ")
        if ok then good = good + 1 end
        print(string.format("  %s %s (%s%s)", ok and "ok:  " or "FAIL:", case[1], outcome,
            lines.decided and string.format("; %d corner%s past its points, taken as decided", lines.decided, lines.decided == 1 and "" or "s") or ""))
        if not ok or outcome == "differ" then for _, l in ipairs(lines) do print("        " .. l) end end
    end
    print(string.format("%d of %d made models as expected (%d samples of animated values compared)", good, n, samples))
    return good == n
end
-- }}}

-- {{{ local function worker: one share of WC3's models, every nparts-th
local function worker(part, nparts)
    local mpq = require("mpq")
    local n, same_, refused, differ_ = 0, 0, 0, 0
    for _, an in ipairs(ARCHIVES) do
        local a = mpq.open(WETE .. "/wc3-installs/frozen-throne/" .. an)
        local k = 0
        for _, f in ipairs(a:list()) do
            if f:lower():match("%.mdx$") then
                k = k + 1
                if args.limit and k > args.limit then break end
                if k % nparts == part then
                    local bytes = a:extract(f)
                    n = n + 1
                    local outcome, lines = check_one(bytes)
                    if outcome == "same" then same_ = same_ + 1
                    elseif outcome == "refused" then
                        refused = refused + 1
                        print(string.format("refused\t%s\t%s\t%s", an, f, lines[2]))
                    else
                        differ_ = differ_ + 1
                        print(string.format("differ\t%s\t%s\t%s", an, f, table.concat(lines, "; ")))
                    end
                end
            end
        end
        a:close()
    end
    print(string.format("counts\t%d\t%d\t%d\t%d", n, same_, refused, differ_))
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
local function main()
    if args.part then worker(args.part, args.nparts); return 0 end
    print("models made here")
    local made_ok = made_run()
    local have = io.open(WETE .. "/wc3-installs/frozen-throne/" .. ARCHIVES[1], "rb")
    if have then have:close() end
    if args.made_only or not have then
        if not have and not args.made_only then
            print("WC3's archives aren't at " .. WETE .. "/wc3-installs/frozen-throne/: only the models made here were compared")
        end
        return made_ok and 0 or 1
    end
    -- WC3's own: the workers start at once, then are read in turn
    local nparts = args.workers or cores()
    local pipes = {}
    for p = 0, nparts - 1 do
        local cmd = string.format("luajit %q %q --build %q --wete %q --part %d %d%s", arg[0], DIR, args.build, WETE, p, nparts,
            args.limit and (" --limit " .. args.limit) or "")
        pipes[#pipes + 1] = io.popen(cmd)
    end
    local n, same_, refused, differ_, lines, reported = 0, 0, 0, 0, {}, 0
    for _, pipe in ipairs(pipes) do
        for line in pipe:lines() do
            local a, b, c, d = line:match("^counts\t(%d+)\t(%d+)\t(%d+)\t(%d+)$")
            if a then
                n, same_, refused, differ_ = n + a, same_ + b, refused + c, differ_ + d
                reported = reported + 1
            else
                lines[#lines + 1] = line
            end
        end
        pipe:close()
    end
    if reported ~= nparts then
        lines[#lines + 1] = string.format("ERROR: %d of %d workers reported (a worker failed; its error is above)", reported, nparts)
    end
    lines[#lines + 1] = string.format("%d WC3 models (MDX, %s): %d identical, %d refused by both, %d disagreeing",
        n, table.concat(ARCHIVES, " and "), same_, refused, differ_)
    local report = table.concat(lines, "\n") .. "\n"
    io.write(report)
    local path = DIR .. "/tmp/shared-memory/mdx-matches-lua.txt"
    local f = io.open(path, "w")
    if not f then io.stderr:write("WARNING: the report could not be kept at " .. path .. " (run scripts/build, which makes tmp/)\n"); return 1 end
    f:write(report); f:close()
    return (made_ok and differ_ == 0 and reported == nparts and n > 0) and 0 or 1
end
-- }}}

os.exit(main())
