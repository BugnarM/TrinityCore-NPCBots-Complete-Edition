-- Run with Lua 5.1+: lua contrib/tests/wintergrasp_vehicle_map.lua
local now, faction, mapID = 100, "Alliance", 502
local frames = {}
local function Frame(parent)
    local f = { parent = parent, shown = true, scripts = {}, children = {} }
    if parent then parent.children[#parent.children + 1] = f end
    function f:SetAllPoints() end
    function f:SetFrameLevel() end
    function f:GetFrameLevel() return 1 end
    function f:SetSize() end
    function f:EnableMouse() end
    function f:SetPoint(...) self.point = {...} end
    function f:ClearAllPoints() end
    function f:SetText(text) self.text = text end
    function f:SetTextColor() end
    function f:SetTexture(...) self.color = {...} end
    function f:SetScript(event, fn) self.scripts[event] = fn end
    function f:RegisterEvent() end
    function f:Show() self.shown = true end
    function f:Hide() self.shown = false end
    function f:IsShown() return self.shown end
    function f:GetWidth() return 1000 end
    function f:GetHeight() return 600 end
    function f:CreateTexture() return Frame() end
    function f:CreateFontString() return Frame() end
    return f
end
function CreateFrame(_, _, parent)
    local f = Frame(parent)
    frames[#frames + 1] = f
    return f
end
function GetTime() return now end
function UnitName() return "Tester" end
function UnitFactionGroup() return faction end
function GetCurrentMapAreaID() return mapID end
WorldMapFrame, WorldMapButton, BattlefieldMinimap = Frame(), Frame(), Frame()
GameTooltip = Frame()
function GameTooltip:SetOwner() end
function GameTooltip:AddLine() end

dofile("contrib/addons/WGVehicleMap/WGVehicleMap.lua")
local driver = frames[1]
local function Receive(message, prefix, sender)
    driver.scripts.OnEvent(driver, "CHAT_MSG_ADDON", prefix or "WGVM1", message, "WHISPER", sender or "Tester")
end
local function Tick(seconds)
    now = now + (seconds or 0.1)
    driver.scripts.OnUpdate(driver, seconds or 0.1)
end
local function Snapshot(seq, rows)
    Receive("B;" .. seq)
    Receive("V;" .. seq .. ";" .. rows)
    Receive("E;" .. seq)
    Tick()
end
local function WorldLayer() return WorldMapButton.children[1] end
local function VisiblePins()
    local count = 0
    local layer = WorldLayer()
    if layer and layer.shown then
        for _, pin in ipairs(layer.pins) do if pin.shown then count = count + 1 end end
    end
    return count
end

Snapshot(1, "0,1,2500,7500|1,3,10000,0|")
assert(VisiblePins() == 2, "both teams must appear")
local allied, enemy = WorldLayer().pins[1], WorldLayer().pins[2]
assert(allied.friendly and not enemy.friendly)
assert(allied.point[4] == 250 and allied.point[5] == -450, "map coordinates must use top-left origin")
assert(enemy.point[4] == 1000 and enemy.point[5] == 0)
assert(allied.label.text == "C" and enemy.label.text == "S")
assert(allied.fill.color[3] == 1 and enemy.fill.color[1] == 0.9)
allied.scripts.OnEnter(allied)
assert(GameTooltip.text == "Catapult")
allied.scripts.OnLeave(allied)
assert(not GameTooltip.shown)
assert(BattlefieldMinimap.children[1].shown, "battlefield map must show pins too")

-- Partial and mismatched snapshots must leave the committed map intact.
Receive("B;2")
Receive("V;2;0,2,1000,2000|")
Receive("E;3")
Tick()
assert(VisiblePins() == 2)
Receive("V;2;1,1,5000,6000|")
Receive("E;2")
Tick()
assert(VisiblePins() == 2 and WorldLayer().pins[1].label.text == "D")

-- New complete snapshot removes destroyed/despawned vehicles.
Snapshot(3, "1,2,5000,5000|")
assert(VisiblePins() == 1 and not WorldLayer().pins[2].shown)
faction = "Horde"
Tick()
assert(WorldLayer().pins[1].friendly, "colors must be relative to the viewer")

mapID = 501
Tick()
assert(VisiblePins() == 0, "pins must not leak onto other zones")
mapID = 502
Tick()
assert(VisiblePins() == 1)

Receive("B;4", "OtherPrefix")
Receive("E;4", "OtherPrefix")
Receive("B;5", nil, "AnotherPlayer")
Receive("E;5", nil, "AnotherPlayer")
Tick()
assert(VisiblePins() == 1, "other addon traffic must be ignored")

Snapshot(6, "0,1,10001,0|")
assert(VisiblePins() == 1, "out-of-bounds rows must reject snapshot")
Snapshot(7, "0,9,100,100|")
assert(VisiblePins() == 1, "unknown vehicle types must reject snapshot")
Snapshot(8, "0,1,100,100")
assert(VisiblePins() == 1, "truncated rows must reject snapshot")
Snapshot(9, "")
assert(VisiblePins() == 0, "empty snapshot must clear the map at battle end")

-- More than one V packet, with all 48 allowed vehicles.
Receive("B;10")
for batch = 1, 8 do Receive("V;10;" .. string.rep("0,2,5000,5000|", 6)) end
Receive("E;10")
Tick()
assert(VisiblePins() == 48)
Receive("B;11")
Receive("V;11;" .. string.rep("0,2,5000,5000|", 49))
Receive("E;11")
Tick()
assert(VisiblePins() == 48, "oversized snapshot must be rejected")
Tick(5.1)
assert(VisiblePins() == 0, "missing updates must expire pins")

Receive("B;12")
Tick(0.1)
Receive("V;12;0,1,5000,5000|")
Receive("E;12")
Tick()
assert(VisiblePins() == 1, "first snapshot must survive rendering between chunks")
driver.scripts.OnEvent(driver, "ZONE_CHANGED_NEW_AREA")
assert(VisiblePins() == 0, "leaving the zone must immediately clear pins")
Snapshot(13, "0,1,5000,5000|")
driver.scripts.OnEvent(driver, "PLAYER_ENTERING_WORLD")
assert(VisiblePins() == 0, "relogging must clear pins")
print("Wintergrasp vehicle map: all checks passed")
