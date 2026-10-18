#!/usr/bin/env lua5.4
-- 053-unseal-note-frame.lua - opens a captured note the way the project
-- mailbox would, and hands it to the mailbox's gatekeeper.
--
-- For a general audience: the documentation pages seal each reader's note
-- with the mailbox's key and post it. This takes the bytes a page actually
-- sent (captured by tests/052), steps past the wrapping the browser adds,
-- unseals the note with rmail's own decryption code and the key, reads
-- the request inside, and runs the gatekeeper on it exactly as rmail would.
-- If any step fails it says which, and exits non-zero.
--
-- Lua 5.4 rather than LuaJIT, by necessity: rmail's decryption library is
-- a compiled module built for the Lua 5.4 rmail runs under.
--
-- Usage: tests/053-unseal-note-frame.lua CAPTURE TOKEN [PROJECT_DIR]
--   CAPTURE  the file of raw bytes the listener received
--   TOKEN    the key the test page sealed with

local RMAIL = "/home/ritz/programs/r-mail"
local capture_path, token = arg[1], arg[2]
local DIR = arg[3] or "/mnt/kaun/custom-client"
package.cpath = RMAIL .. "/libs/?.so;" .. package.cpath
package.path = RMAIL .. "/libs/?.lua;" .. package.path

local crypto = require("rmail_crypto")
local json = require("dkjson")

-- {{{ local function fail
local function fail(why)
    print("  FAIL  " .. why)
    os.exit(1)
end
-- }}}

-- {{{ local function quote
local function quote(text)
    return "'" .. text:gsub("'", "'\\''") .. "'"
end
-- }}}

if capture_path == nil or token == nil then fail("usage: 053-unseal-note-frame.lua CAPTURE TOKEN [PROJECT_DIR]") end

local handle = io.open(capture_path, "rb")
if handle == nil then fail("no capture at " .. capture_path) end
local raw = handle:read("a")
handle:close()

-- The browser's own request wraps the frame: its headers end at the first
-- blank line, and every byte after it is the frame the page built.
local header_end = raw:find("\r\n\r\n", 1, true)
if raw:sub(1, 5) ~= "POST " or header_end == nil then fail("the capture is not a browser POST") end
local frame = raw:sub(header_end + 4)
print(string.format("  ok    the browser wrapped the frame in %d bytes of its own request", header_end + 3))

local length = string.unpack(">I4", frame)
if length ~= #frame - 4 then
    fail(string.format("the frame says it is %d bytes long but %d follow its length", length, #frame - 4))
end
print(string.format("  ok    the frame's length prefix (%d) matches the bytes after it", length))

local plaintext = crypto.aes_gcm_decrypt(crypto.sha256(token), frame:sub(5, 16), frame:sub(17))
if plaintext == nil then fail("rmail's decryption would not open the frame with the key") end
print("  ok    rmail's decryption opens it with the key")

local request_line = plaintext:match("^([^\r\n]+)")
if request_line ~= "POST /deliver HTTP/1.1" then fail("the sealed request is not POST /deliver: " .. tostring(request_line)) end
local inner_end = plaintext:find("\r\n\r\n", 1, true)
local declared = tonumber(plaintext:match("\r\nContent%-Length: (%d+)\r\n"))
local body = plaintext:sub(inner_end + 4)
if declared ~= #body then fail(string.format("Content-Length says %s, the body is %d bytes", tostring(declared), #body)) end
print("  ok    inside is POST /deliver with an honest Content-Length")

local data = json.decode(body)
if type(data) ~= "table" then fail("the body is not JSON") end
if data.type ~= "message" then fail("type is " .. tostring(data.type) .. ", not message") end
if type(data.subject) ~= "string" or not data.subject:match("^feedback%-") then fail("the subject is not a feedback subject") end
if type(data.message_id) ~= "string" or #data.message_id < 16 then fail("no message id") end
if type(data.mtime) ~= "number" then fail("no mtime") end
print("  ok    a message: subject " .. data.subject)

local pipe = io.popen("W_CLIENT_DIR=" .. quote(DIR) .. " " .. DIR .. "/scripts/check-docs-feedback docs-feedback "
                      .. quote(data.subject) .. " " .. quote(data.body))
local checked = pipe:read("a")
pipe:close()
if not checked:match("^checked: ") then fail("the gatekeeper refused the page's own note:\n" .. checked) end
print("  ok    the gatekeeper keeps it, as:")
for line in checked:gmatch("[^\n]+") do print("          " .. line) end
