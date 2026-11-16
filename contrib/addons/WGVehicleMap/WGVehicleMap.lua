local PREFIX, MAP_ID, TIMEOUT = "WGVM1", 502, 5
local driver = CreateFrame("Frame")
local vehicles, pending, lastUpdate = {}, nil, nil
local layers = {}
local kinds = { "Catapult", "Demolisher", "Siege Engine" }
local letters = { "C", "D", "S" }

local function NewLayer(parent)
    local layer = CreateFrame("Frame", nil, parent)
    layer:SetAllPoints(parent)
    layer:SetFrameLevel(parent:GetFrameLevel() + 20)
    layer.pins = {}
    layer.legend = layer:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    layer.legend:SetPoint("TOPLEFT", layer, "TOPLEFT", 8, -8)
    layer.legend:SetText("WG: |cff3399ffAllies|r / |cffff3333Enemies|r  (C / D / S)")
    return layer
end

local function NewPin(layer)
    local pin = CreateFrame("Frame", nil, layer)
    pin:SetSize(16, 16)
    pin:EnableMouse(true)
    local border = pin:CreateTexture(nil, "BACKGROUND")
    border:SetAllPoints(pin)
    border:SetTexture(0, 0, 0, 1)
    pin.fill = pin:CreateTexture(nil, "ARTWORK")
    pin.fill:SetPoint("TOPLEFT", 1, -1)
    pin.fill:SetPoint("BOTTOMRIGHT", -1, 1)
    pin.label = pin:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    pin.label:SetPoint("CENTER")
    pin.label:SetTextColor(1, 1, 1)
    pin:SetScript("OnEnter", function(self)
        GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
        GameTooltip:SetText(kinds[self.data.kind])
        GameTooltip:AddLine(self.friendly and "Allied vehicle" or "Enemy vehicle",
            self.friendly and 0.2 or 1, self.friendly and 0.6 or 0.2, self.friendly and 1 or 0.2)
        GameTooltip:Show()
    end)
    pin:SetScript("OnLeave", function() GameTooltip:Hide() end)
    return pin
end

local function Render(layer)
    local team = UnitFactionGroup("player") == "Alliance" and 0 or 1
    for i, data in ipairs(vehicles) do
        local pin = layer.pins[i]
        if not pin then
            pin = NewPin(layer)
            layer.pins[i] = pin
        end
        pin.data, pin.friendly = data, data.team == team
        if pin.friendly then
            pin.fill:SetTexture(0.1, 0.45, 1, 1)
        else
            pin.fill:SetTexture(0.9, 0.1, 0.1, 1)
        end
        pin.label:SetText(letters[data.kind])
        pin:ClearAllPoints()
        pin:SetPoint("CENTER", layer, "TOPLEFT", data.x * layer:GetWidth(), -data.y * layer:GetHeight())
        pin:Show()
    end
    for i = #vehicles + 1, #layer.pins do layer.pins[i]:Hide() end
end

local function Clear()
    vehicles, pending, lastUpdate = {}, nil, nil
    for _, layer in pairs(layers) do layer:Hide() end
end

local function Receive(prefix, message, channel, sender)
    -- The server sends a private snapshot with the receiving player's GUID.
    if prefix ~= PREFIX or channel ~= "WHISPER" or sender ~= UnitName("player") then return end
    local op, sequence, payload = string.match(message, "^(%u);(%d+);?(.*)$")
    if not op then return end
    if op == "B" then
        pending = { sequence = sequence, rows = {}, started = GetTime() }
    elseif pending and pending.sequence == sequence and GetTime() - pending.started <= TIMEOUT then
        if op == "V" then
            if payload ~= "" and string.sub(payload, -1) ~= "|" then pending = nil; return end
            for row in string.gmatch(payload, "([^|]+)|") do
                local team, kind, x, y = string.match(row, "^([01]),([123]),(%d+),(%d+)$")
                x, y = tonumber(x), tonumber(y)
                if not team or not x or not y or x > 10000 or y > 10000 or #pending.rows >= 48 then
                    pending = nil
                    return
                end
                pending.rows[#pending.rows + 1] = { team = tonumber(team), kind = tonumber(kind), x = x / 10000, y = y / 10000 }
            end
        elseif op == "E" then
            vehicles, lastUpdate = pending.rows, GetTime()
            pending = nil
        end
    end
end

driver:RegisterEvent("CHAT_MSG_ADDON")
driver:RegisterEvent("PLAYER_ENTERING_WORLD")
driver:RegisterEvent("ZONE_CHANGED_NEW_AREA")
driver:SetScript("OnEvent", function(self, event, ...)
    if event == "CHAT_MSG_ADDON" then Receive(...) else Clear() end
end)

local elapsedSinceRender = 0
driver:SetScript("OnUpdate", function(self, elapsed)
    elapsedSinceRender = elapsedSinceRender + elapsed
    if elapsedSinceRender < 0.1 then return end
    elapsedSinceRender = 0
    if lastUpdate and GetTime() - lastUpdate > TIMEOUT then
        vehicles, lastUpdate = {}, nil
    end
    if pending and GetTime() - pending.started > TIMEOUT then pending = nil end
    local worldShown = WorldMapFrame and WorldMapFrame:IsShown() and GetCurrentMapAreaID() == MAP_ID
    if worldShown and not layers.world then layers.world = NewLayer(WorldMapButton) end
    local battleShown = BattlefieldMinimap and BattlefieldMinimap:IsShown() and GetCurrentMapAreaID() == MAP_ID
    if battleShown and not layers.battle then layers.battle = NewLayer(BattlefieldMinimap) end
    for key, layer in pairs(layers) do
        if (key == "world" and worldShown or key == "battle" and battleShown) and #vehicles > 0 then
            layer:Show()
            Render(layer)
        else
            layer:Hide()
        end
    end
end)
