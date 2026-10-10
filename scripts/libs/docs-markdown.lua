-- docs-markdown.lua - turns the project's Markdown, and its source code,
-- into HTML fragments for the documentation site.
--
-- For a general audience: the project's documents are written in Markdown,
-- a plain-text way of marking headings, lists, tables and code. Browsers
-- don't read Markdown, so this turns it into HTML. It also colours source
-- code (keywords one colour, comments another) so it reads like it does in
-- an editor. It knows nothing about which page links to which: when it
-- meets a link or a name that might be one, it asks the builder (the
-- "context" passed in), which knows every page. That split keeps this file
-- about the shape of text and the builder about the shape of the project.
--
-- Not a full Markdown: headings, paragraphs, nested lists, tables, quotes,
-- rules, fenced code, inline code, links, bold and italic. Enough for every
-- document here; a construct it does not know comes out as plain text,
-- never as raw HTML, because everything is escaped first.

local md = {}

-- {{{ function md.escape
function md.escape(text)
    return (text:gsub("&", "&amp;"):gsub("<", "&lt;"):gsub(">", "&gt;"):gsub('"', "&quot;"))
end
-- }}}

-- ---------------------------------------------------------------------------
-- Highlighting
-- ---------------------------------------------------------------------------

-- Per language: keywords (a set) and comment openers (patterns anchored at
-- the current position). One table per language rather than one merged
-- list, because the languages disagree: "--" is a comment in Lua and a
-- decrement in C, "#" is a comment in bash and a directive in C.
local LANGS = {
    c = {
        keywords = "if else for while do return struct static const void int char "
                .. "unsigned signed short long float double bool uint8_t uint16_t "
                .. "uint32_t uint64_t int8_t int16_t int32_t int64_t size_t switch "
                .. "case break continue typedef sizeof goto enum union extern inline "
                .. "volatile default true false NULL",
        comments = { "^/%*.-%*/", "^/%*.*$", "^//[^\n]*" },
        directive = true,
    },
    lua = {
        keywords = "local function end if then else elseif for in do while repeat "
                .. "until return break nil true false and or not goto",
        comments = { "^%-%-%[%[.-%]%]", "^%-%-[^\n]*" },
    },
    sh = {
        keywords = "if then else elif fi for in do done while until case esac "
                .. "function return exit local export echo set shift",
        comments = { "^#[^\n]*" },
    },
    sql = {
        keywords = "select from where insert into update delete create table and "
                .. "or not null join on as order by group limit values set",
        comments = { "^%-%-[^\n]*" },
        caseless = true,
    },
}
LANGS.h = LANGS.c
LANGS.bash = LANGS.sh

for _, lang in pairs(LANGS) do
    if type(lang.keywords) == "string" then
        local set = {}
        for word in lang.keywords:gmatch("%S+") do set[word] = true end
        lang.keywords = set
    end
end

-- {{{ function md.language_of
-- Which language a file is in, from its name or, for a script with no
-- extension, its first line. Returns a key of LANGS or "" (plain).
function md.language_of(name, text)
    local ext = name:match("%.(%w+)$")
    if ext and LANGS[ext] then return ext end
    local first = text:match("^#![^\n]*") or ""
    if first:match("lua") then return "lua" end
    if first:match("sh") then return "sh" end
    return ""
end
-- }}}

-- {{{ local function string_end
-- Where a quoted string starting at position i ends (the closing quote's
-- position), honouring backslash escapes and stopping at a newline.
local function string_end(code, i)
    local quote = code:sub(i, i)
    local j = i + 1
    while j <= #code do
        local c = code:sub(j, j)
        if c == "\\" then
            j = j + 2
        elseif c == quote then
            return j
        elseif c == "\n" then
            return j - 1
        else
            j = j + 1
        end
    end
    return #code
end
-- }}}

-- {{{ function md.highlight
-- Code as HTML with spans: k keyword, c comment, s string, n number,
-- p a C directive line (#include, #define). Unknown language: escaped only.
function md.highlight(code, language)
    local lang = LANGS[language or ""]
    if lang == nil then return md.escape(code) end

    local out = {}
    local i = 1
    local n = #code
    local line_start = true

    while i <= n do
        local rest = code:sub(i, i + 200)   -- enough for any word, number or directive start
        local piece, class

        for _, pattern in ipairs(lang.comments) do
            piece = code:match(pattern, i)
            if piece then
                -- An unterminated block comment runs to the end of the code.
                if pattern == "^/%*.*$" then piece = code:sub(i) end
                class = "c"
                break
            end
        end

        if piece == nil and lang.directive and line_start and rest:match("^%s*#") then
            piece = code:match("^%s*#[^\n]*", i)
            class = "p"
        end

        if piece == nil then
            local c = code:sub(i, i)
            if c == '"' or c == "'" then
                piece = code:sub(i, string_end(code, i))
                class = "s"
            end
        end

        if piece == nil then
            local word = rest:match("^[%a_][%w_]*")
            if word then
                piece = word
                local probe = lang.caseless and word:lower() or word
                class = lang.keywords[probe] and "k" or nil
            end
        end

        if piece == nil then
            local number = rest:match("^%d[%w%.]*")
            if number then piece = number; class = "n" end
        end

        if piece == nil then
            piece = code:sub(i, i)
        end

        if class then
            out[#out + 1] = '<span class="' .. class .. '">' .. md.escape(piece) .. "</span>"
        else
            out[#out + 1] = md.escape(piece)
        end

        line_start = piece:sub(-1) == "\n" or (piece:match("^%s+$") ~= nil and line_start)
        i = i + #piece
    end

    return table.concat(out)
end
-- }}}

-- ---------------------------------------------------------------------------
-- Inline text
-- ---------------------------------------------------------------------------

-- {{{ local function inline
-- One run of text (a paragraph, a list item, a table cell) as HTML.
-- The order matters: code spans are set aside first so nothing below
-- rewrites their contents; then the links the author wrote; then bold and
-- italic; then the context's automatic links, which are guesses and must
-- never be made inside something already a link or code. Set-aside pieces
-- wait as \1<n>\2 markers, bytes no document contains.
local function inline(text, ctx)
    local held = {}

    local function hold(html)
        held[#held + 1] = html
        return "\1" .. #held .. "\2"
    end

    text = text:gsub("`([^`]+)`", function(code)
        local href = ctx.code_link and ctx.code_link(code)
        if href then
            return hold('<a href="' .. href .. '"><code>' .. md.escape(code) .. "</code></a>")
        end
        return hold("<code>" .. md.escape(code) .. "</code>")
    end)

    text = text:gsub("%[([^%]]+)%]%(([^%)%s]+)%)", function(label, target)
        local href = ctx.resolve(target)
        local label_html = md.escape(label)
        if href == nil then
            -- Shown, not dropped: a link that quietly becomes plain text is a
            -- link nobody fixes.
            return hold('<span class="dead" title="this link goes nowhere: '
                        .. md.escape(target) .. '">' .. label_html .. "</span>")
        end
        return hold('<a href="' .. md.escape(href) .. '">' .. label_html .. "</a>")
    end)

    text = md.escape(text)
    text = text:gsub("%*%*([^%*]+)%*%*", "<strong>%1</strong>")
    text = text:gsub("%f[%w%*]%*([^%s%*][^%*]-)%*%f[^%w%*]", "<em>%1</em>")   -- may span the lines of one paragraph

    if ctx.autolink then text = ctx.autolink(text) end

    text = text:gsub("\1(%d+)\2", function(which) return held[tonumber(which)] end)
    return text
end
-- }}}

-- ---------------------------------------------------------------------------
-- Blocks
-- ---------------------------------------------------------------------------

-- {{{ local function slug
-- A heading's anchor: lowercase words joined by dashes. Kept stable so a
-- link to page.html#the-clock written today still lands tomorrow.
local function slug(text)
    local s = text:lower():gsub("<[^>]+>", ""):gsub("&%w+;", "")
    s = s:gsub("[^%w]+", "-"):gsub("^%-+", ""):gsub("%-+$", "")
    if s == "" then s = "section" end
    return s
end
-- }}}

-- {{{ local function list_html
-- A run of list lines as nested lists. Each item carries its indent
-- (spaces before its marker); a deeper indent opens a list inside the item
-- above, a shallower one closes back out.
local function list_html(items, ctx)
    local out = {}
    local stack = {}   -- { indent = number, tag = "ul"|"ol" }

    for _, item in ipairs(items) do
        while #stack > 0 and item.indent < stack[#stack].indent do
            out[#out + 1] = "</li></" .. stack[#stack].tag .. ">"
            stack[#stack] = nil
        end
        if #stack == 0 or item.indent > stack[#stack].indent then
            stack[#stack + 1] = { indent = item.indent, tag = item.tag }
            out[#out + 1] = "<" .. item.tag .. ">"
        else
            out[#out + 1] = "</li>"
        end
        out[#out + 1] = "<li>" .. inline(item.text, ctx)
    end

    while #stack > 0 do
        out[#out + 1] = "</li></" .. stack[#stack].tag .. ">"
        stack[#stack] = nil
    end
    return table.concat(out)
end
-- }}}

-- {{{ local function table_html
local function table_html(rows, ctx)
    local html = { '<div class="table-wrap"><table>' }
    local first = true
    for _, row in ipairs(rows) do
        -- The second line of a Markdown table only sets alignment.
        if not row:match("^%s*|[%s%-:|]+|?%s*$") then
            local cells = {}
            local body = row:gsub("^%s*|", ""):gsub("|%s*$", "")
            for cell in (body .. "|"):gmatch("([^|]*)|") do
                cells[#cells + 1] = cell:gsub("^%s+", ""):gsub("%s+$", "")
            end
            local tag = first and "th" or "td"
            html[#html + 1] = "<tr>"
            for _, cell in ipairs(cells) do
                html[#html + 1] = "<" .. tag .. ">" .. inline(cell, ctx) .. "</" .. tag .. ">"
            end
            html[#html + 1] = "</tr>"
            first = false
        end
    end
    html[#html + 1] = "</table></div>"
    return table.concat(html)
end
-- }}}

-- What starts a block other than a paragraph. A paragraph runs until one of
-- these, or a blank line.
local function is_structural(line)
    return line:match("^#+%s") or line:match("^%s*|") or line:match("^%s*```")
        or line:match("^%s*~~~") or line:match("^>") or line:match("^%s*[%-%*+]%s")
        or line:match("^%s*%d+[%.%)]%s") or line:match("^%-%-%-+%s*$")
        or line:match("^%*%*%*+%s*$")
end

-- {{{ function md.render
-- A whole Markdown document as HTML. ctx (table) supplies:
--   resolve(target) -> href or nil      for [label](target)
--   code_link(code) -> href or nil      for `code` that names a page
--   autolink(escaped text) -> text      issue numbers and file names in prose
-- Returns the HTML (string) and the headings (list of { level, text, id }).
function md.render(text, ctx)
    local lines = {}
    for line in (text:gsub("\r\n", "\n") .. "\n"):gmatch("([^\n]*)\n") do
        lines[#lines + 1] = line
    end

    local out = {}
    local headings = {}
    local used_ids = {}
    local i, n = 1, #lines

    while i <= n do
        local line = lines[i]
        local fence = line:match("^%s*(```+)") or line:match("^%s*(~~~+)")

        if fence then
            local language = line:match("^%s*[`~]+%s*([%w%+%-]*)") or ""
            local body = {}
            i = i + 1
            while i <= n and not lines[i]:match("^%s*" .. fence:gsub("%p", "%%%0")) do
                body[#body + 1] = lines[i]
                i = i + 1
            end
            i = i + 1
            local label = language ~= "" and ('<span class="lang">' .. md.escape(language) .. "</span>") or ""
            out[#out + 1] = '<pre class="code">' .. label .. "<code>"
                            .. md.highlight(table.concat(body, "\n"), language:lower())
                            .. "</code></pre>"

        elseif line:match("^#+%s") then
            local hashes, heading = line:match("^(#+)%s+(.-)%s*#*%s*$")
            local level = math.min(#hashes, 6)
            local html = inline(heading, ctx)
            local id = slug(heading)
            if used_ids[id] then
                used_ids[id] = used_ids[id] + 1
                id = id .. "-" .. used_ids[id]
            else
                used_ids[id] = 1
            end
            headings[#headings + 1] = { level = level, text = heading, id = id }
            out[#out + 1] = string.format('<h%d id="%s">%s<a class="here-link" href="#%s" title="link to this part">#</a></h%d>',
                                          level, id, html, id, level)
            i = i + 1

        elseif line:match("^%s*|") then
            local rows = {}
            while i <= n and lines[i]:match("^%s*|") do
                rows[#rows + 1] = lines[i]
                i = i + 1
            end
            out[#out + 1] = table_html(rows, ctx)

        elseif line:match("^%s*[%-%*+]%s") or line:match("^%s*%d+[%.%)]%s") then
            local items = {}
            while i <= n do
                local l = lines[i]
                local indent, marker, rest = l:match("^(%s*)([%-%*+])%s+(.*)$")
                local tag = "ul"
                if indent == nil then
                    indent, marker, rest = l:match("^(%s*)(%d+[%.%)])%s+(.*)$")
                    tag = "ol"
                end
                if indent ~= nil then
                    items[#items + 1] = { indent = #indent, tag = tag, text = rest }
                elseif #items > 0 and l:match("^%s+%S") then
                    -- A wrapped line belongs to the item above it.
                    items[#items].text = items[#items].text .. " " .. l:gsub("^%s+", "")
                else
                    break
                end
                i = i + 1
            end
            out[#out + 1] = list_html(items, ctx)

        elseif line:match("^>") then
            local body = {}
            while i <= n and lines[i]:match("^>") do
                body[#body + 1] = lines[i]:gsub("^>%s?", "")
                i = i + 1
            end
            out[#out + 1] = "<blockquote>" .. md.render(table.concat(body, "\n"), ctx) .. "</blockquote>"

        elseif line:match("^%-%-%-+%s*$") or line:match("^%*%*%*+%s*$") then
            out[#out + 1] = "<hr>"
            i = i + 1

        elseif line:match("^%s*$") then
            i = i + 1

        else
            local body = {}
            while i <= n and not lines[i]:match("^%s*$") and (#body == 0 or not is_structural(lines[i])) do
                body[#body + 1] = lines[i]:gsub("^%s+", "")
                i = i + 1
            end
            out[#out + 1] = "<p>" .. inline(table.concat(body, "\n"), ctx) .. "</p>"
        end
    end

    return table.concat(out, "\n"), headings
end
-- }}}

return md
