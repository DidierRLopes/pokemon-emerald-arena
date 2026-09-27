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
-- Bridge to agent:                 F frame hex hex ...      (every frame)
local PORT = 7355
local sock, buffer, watch, held, frame, retry = nil, "", {}, 0, 0, 0

local function hex(s)
  return (s:gsub(".", function(c) return string.format("%02x", c:byte()) end))
end

local function release()
  if held ~= 0 then emu:clearKeys(held) end
  held = 0
end

local function stop()
  if sock then sock:close() end
  sock, buffer, watch = nil, "", {}
  release()
end

local function handle(line)
  local cmd, rest = line:match("^(%S+)%s*(.*)$")
  if cmd == "W" then
    watch = {}
    for a, n in rest:gmatch("(%x+):(%d+)") do watch[#watch + 1] = {tonumber(a, 16), tonumber(n)} end
  elseif cmd == "K" then
    local mask = tonumber(rest) or 0
    emu:clearKeys(held & ~mask)
    emu:addKeys(mask)
    held = mask
  elseif cmd == "L" then
    console:log(rest)
  elseif cmd == "B" then
    local a, v = rest:match("(%x+)%s+(%d+)")
    if a then emu:write8(tonumber(a, 16), tonumber(v)) end
  end
end

local function received()
  while sock do
    local p, err = sock:receive(4096)
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
  if not s:connect("127.0.0.1", PORT) then return end
  sock = s
  sock:add("received", received)
  sock:add("error", function() console:log("voice bridge: connection error"); stop() end)
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
    for _, w in ipairs(watch) do parts[#parts + 1] = hex(emu:readRange(w[1], w[2])) end
    sock:send(table.concat(parts, " ") .. "\n")
  end
end)
console:log("voice bridge: loaded, waiting for the voice agent on port " .. PORT)
