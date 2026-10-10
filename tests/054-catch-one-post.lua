#!/usr/bin/env lua5.4
-- 054-catch-one-post.lua - stands in for the mailbox during the note box
-- test: takes one posted note, keeps its raw bytes, and answers "200 OK".
--
-- For a general audience: tests/052 needs something at the mailbox's
-- address that records exactly what a page sends and answers the way a
-- server does, so the page's own "sent" message can be checked too. A
-- browser may open a spare connection before the real one and leave it
-- empty; this accepts every connection, keeps the first that carries a
-- request body, and stops. It gives up after TIMEOUT seconds.
--
-- Lua 5.4 for rmail's networking library (luasocket, compiled for 5.4).
--
-- Usage: tests/054-catch-one-post.lua PORT CAPTURE [TIMEOUT] [HOST]
--   HOST  the address to listen on (default 127.0.0.1)

local RMAIL = "/home/ritz/programs/r-mail"
package.cpath = RMAIL .. "/libs/?.so;" .. RMAIL .. "/libs/?/?.so;" .. package.cpath
package.path = RMAIL .. "/libs/?.lua;" .. package.path
local socket = require("socket")

local port, capture_path, timeout, host = tonumber(arg[1]), arg[2], tonumber(arg[3] or "20"), arg[4] or "127.0.0.1"
if port == nil or capture_path == nil then
    io.stderr:write("usage: 054-catch-one-post.lua PORT CAPTURE [TIMEOUT]\n")
    os.exit(2)
end

local server = assert(socket.bind(host, port))
local deadline = socket.gettime() + timeout

-- {{{ local function take
-- Reads one request from a connection: the header lines, then exactly
-- Content-Length bytes. Returns the raw bytes, or nil for a connection
-- that closed or stayed silent.
local function take(client)
    client:settimeout(5)
    local lines = {}
    while true do
        local line = client:receive("*l")
        if line == nil then return nil end
        lines[#lines + 1] = line
        if line == "" then break end
    end
    local head = table.concat(lines, "\r\n") .. "\r\n"
    local length = tonumber(head:match("\r\n[Cc]ontent%-[Ll]ength: (%d+)\r\n"))
    if length == nil or length == 0 then return nil end
    local body = client:receive(length)
    if body == nil then return nil end
    return head .. body
end
-- }}}

-- {{{ main
server:settimeout(1)
while socket.gettime() < deadline do
    local client = server:accept()
    if client then
        local raw = take(client)
        if raw then
            client:send("HTTP/1.1 200 OK\r\nContent-Length: 11\r\nConnection: close\r\n\r\n{\"ok\":true}")
            client:close()
            local handle = assert(io.open(capture_path, "wb"))
            handle:write(raw)
            handle:close()
            os.exit(0)
        end
        client:close()
    end
end
io.stderr:write("054-catch-one-post: nothing posted to port " .. port .. " within " .. timeout .. " s\n")
os.exit(1)
-- }}}
