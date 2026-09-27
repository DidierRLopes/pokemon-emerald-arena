-- Emerald Arena voice bridge for desktop mGBA 0.10 (Tools > Scripting > File > Load script).
--
-- Connects to the local voice agent on 127.0.0.1:7355 and, every frame,
-- sends the memory ranges the agent asked to watch. The agent answers with
-- the GBA keys it wants held; they are added on top of your own keys, so the
-- keyboard keeps working. It retries once a second until the agent is up.
--
-- Agent to bridge, one line each:  W addr:len,addr:len,...  (watch list)
--                                  K mask                   (keys to hold)
--                                  B addr value             (write one byte)
--                                  L text                   (show in this console)
--                                  R path                   (load a save state: reset the fight)
--                                  H addr value             (write 16 bits: Player 2's keys)
-- Bridge to agent:                 F frame hex hex ...      (every frame)
local PORT = 7355
local sock, buffer, watch, held, frame, retry = nil, "", {}, 0, 0, 0
-- mGBA 0.10's socket close does not mark the socket closed, and collecting
-- the object closes the same descriptor again, which by then can belong to
-- a new connection. Every socket is closed exactly once and kept here, so it
-- is never collected.
local closed = {}

local function retire(s)
  s:close()
  closed[#closed + 1] = s
end

-- One string.format per 2000 bytes instead of one call per byte.
local function hex(s)
  local out, n = {}, #s
  for i = 1, n, 2000 do
    local j = math.min(i + 1999, n)
    out[#out + 1] = string.format(string.rep("%02x", j - i + 1), s:byte(i, j))
  end
  return table.concat(out)
end
-- The last bytes sent per watched range: unchanged ranges go as "=".
local sent = {}

local function release()
  if held ~= 0 then emu:clearKeys(held) end
  held = 0
end

local function stop()
  if sock then retire(sock) end
  sock, buffer, watch, sent = nil, "", {}, {}
  release()
end

local function handle(line)
  local cmd, rest = line:match("^(%S+)%s*(.*)$")
  if cmd == "W" then
    watch, sent = {}, {}
    for a, n in rest:gmatch("(%x+):(%d+)") do watch[#watch + 1] = {tonumber(a, 16), tonumber(n)} end
  elseif cmd == "K" then
    local mask = tonumber(rest) or 0
    emu:clearKeys(held & ~mask)
    emu:addKeys(mask)
    held = mask
  elseif cmd == "L" then
    console:log(rest)
  elseif cmd == "H" then
    local a, v = rest:match("(%x+)%s+(%d+)")
    if a then emu:write16(tonumber(a, 16), tonumber(v)) end
  elseif cmd == "R" then
    release()
    if emu:loadStateFile(rest) then console:log("voice bridge: fight reset") else console:log("voice bridge: could not load " .. rest) end
  elseif cmd == "B" then
    local a, v = rest:match("(%x+)%s+(%d+)")
    if a then emu:write8(tonumber(a, 16), tonumber(v)) end
  end
end

-- Handlers are bound to their own socket: a socket from an earlier session
-- can keep reporting errors, and must not tear down the current one.
local function received(s)
  while sock == s do
    local p, err = s:receive(4096)
    if p then
      buffer = buffer .. p
      while true do
        local line, rest = buffer:match("^(.-)\n(.*)$")
        if not line then break end
        buffer = rest
        handle(line)
      end
    else
      if err ~= socket.ERRORS.AGAIN then
        console:log("voice bridge: agent disconnected")
        stop()
      end
      return
    end
  end
end

local function connect()
  local s = socket.tcp()
  if not s:connect("127.0.0.1", PORT) then retire(s); return end
  sock = s
  s:add("received", function() received(s) end)
  -- mGBA's readiness check can report a spurious error on a reconnected
  -- socket, so only a failed read or send ends the connection.
  s:add("error", function() end)
  sock:send("H emerald-voice 1\n")
  console:log("voice bridge: connected to the voice agent")
end

callbacks:add("frame", function()
  frame = frame + 1
  if not sock then
    retry = retry - 1
    if retry <= 0 then retry = 60; connect() end
    return
  end
  sock:poll()
  if sock and #watch > 0 then
    local parts = {"F", tostring(frame)}
    for i, w in ipairs(watch) do
      local raw = emu:readRange(w[1], w[2])
      if raw == sent[i] then
        parts[#parts + 1] = "="
      else
        parts[#parts + 1] = hex(raw)
        sent[i] = raw
      end
    end
    local ok = sock:send(table.concat(parts, " ") .. "\n")
    if not ok then
      console:log("voice bridge: agent gone; retrying")
      stop()
    end
  end
end)
console:log("voice bridge: loaded, waiting for the voice agent on port " .. PORT)
