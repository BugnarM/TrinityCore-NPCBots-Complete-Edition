local _, addon = ...
local strips = {}
local drawnQuests = {}
local inWorld = false
local mapDirty = true
local questsDirty = true
local changingMap = false
local updatingQuests = false
local lastGeometry
local outdoorDiameter = { [0] = 1400 / 3, 400, 1000 / 3, 800 / 3, 200, 400 / 3 }
local indoorDiameter = { [0] = 300, 240, 180, 120, 80, 50 }

local function HideStrips()
    for _, strip in ipairs(strips) do
        strip.frame:Hide()
    end
end

local function GetStrip(index)
    if not strips[index] then
        local name = "AzerothQuestMinimapStrip" .. index
        local frame = CreateFrame("ScrollFrame", name, Minimap, "AzerothQuestMinimapStripTemplate")
        local canvas = frame:GetScrollChild()
        local blobs = _G[name .. "CanvasBlobs"]
        frame:EnableMouse(false)
        blobs:EnableMouse(false)
        WorldMapBlobFrame_OnLoad(blobs)
        strips[index] = { frame = frame, canvas = canvas, blobs = blobs }
    end
    return strips[index]
end

local function GetDiameter()
    local zoom = Minimap:GetZoom()
    local outsideZoom = tonumber(GetCVar("minimapZoom"))
    local insideZoom = tonumber(GetCVar("minimapInsideZoom"))
    local indoors
    if outsideZoom ~= insideZoom then
        indoors = zoom == insideZoom
    elseif IsIndoors then
        indoors = IsIndoors()
    else
        indoors = IsInInstance()
    end
    return (indoors and indoorDiameter or outdoorDiameter)[zoom]
end

local function GetMapDimensions(mapID, floor)
    local map = addon.mapDimensions[mapID]
    if not map then return end
    if floor > 0 and DungeonUsesTerrainMap() and map[0] then
        floor = floor - 1
    end
    return map[floor]
end

local function GetQuestIDs()
    local questIDs = {}
    updatingQuests = true
    local count = QuestMapUpdateAllQuests()
    QuestPOIUpdateIcons()
    updatingQuests = false
    for index = 1, count or 0 do
        local questID, logIndex = QuestPOIGetQuestIDByVisibleIndex(index)
        if questID and questID > 0 and logIndex and logIndex > 0 then
            local _, _, _, _, _, _, complete = GetQuestLogTitle(logIndex)
            if complete ~= 1 then
                questIDs[questID] = true
            end
        end
    end
    return questIDs
end

local function DrawQuests(questIDs, stripCount)
    for index, strip in ipairs(strips) do
        for questID in pairs(drawnQuests) do
            strip.blobs:DrawQuestBlob(questID, false)
        end
        if index <= stripCount then
            for questID in pairs(questIDs) do
                strip.blobs:DrawQuestBlob(questID, true)
            end
        end
    end
    drawnQuests = questIDs
end

local function LayoutStrips(width, height, zoneWidth, zoneHeight, square)
    local bands = square and 1 or 11
    local stripCount = 0
    -- Scroll frames clip the native render to inscribed horizontal bands on a round minimap.
    for band = 1, bands do
        local top = (1 - math.cos(math.pi * (band - 1) / bands)) / 2
        local bottom = (1 - math.cos(math.pi * band / bands)) / 2
        local distance = math.max(math.abs(2 * top - 1), math.abs(2 * bottom - 1))
        local fraction = square and 1 or math.sqrt(math.max(0, 1 - distance * distance))
        if fraction > 0 then
            stripCount = stripCount + 1
            local strip = GetStrip(stripCount)
            strip.left = width * (1 - fraction) / 2
            strip.top = height * top
            strip.frame:ClearAllPoints()
            strip.frame:SetPoint("TOPLEFT", Minimap, "TOPLEFT", strip.left, -strip.top)
            strip.frame:SetWidth(width * fraction)
            strip.frame:SetHeight(height * (bottom - top))
            strip.canvas:SetWidth(width * fraction)
            strip.canvas:SetHeight(height * (bottom - top))
            strip.blobs:SetWidth(zoneWidth)
            strip.blobs:SetHeight(zoneHeight)
            strip.frame:Show()
        end
    end
    for index = stripCount + 1, #strips do
        strips[index].frame:Hide()
    end
    return stripCount
end

local function Update()
    if not inWorld or not Minimap:IsVisible() or WorldMapFrame:IsShown()
        or not GetCVarBool("questPOI") or GetCVarBool("rotateMinimap") then
        HideStrips()
        lastGeometry = nil
        return
    end
    if mapDirty then
        mapDirty = false
        changingMap = true
        SetMapToCurrentZone()
        changingMap = false
        questsDirty = true
    end

    local mapID, floor = GetCurrentMapAreaID(), GetCurrentMapDungeonLevel() or 0
    local dimensions = GetMapDimensions(mapID, floor)
    local playerX, playerY = GetPlayerMapPosition("player")
    local diameter = GetDiameter()
    local width, height = Minimap:GetWidth(), Minimap:GetHeight()
    if not dimensions or not diameter or not playerX or not playerY
        or (playerX == 0 and playerY == 0) or width <= 0 or height <= 0 then
        HideStrips()
        lastGeometry = nil
        return
    end

    local zoneWidth = dimensions[1] * width / diameter
    local zoneHeight = dimensions[2] * height / diameter
    local square = GetMinimapShape and GetMinimapShape() == "SQUARE" or false
    local layoutChanged = not lastGeometry or lastGeometry.mapID ~= mapID or lastGeometry.floor ~= floor
        or lastGeometry.width ~= width or lastGeometry.height ~= height
        or lastGeometry.diameter ~= diameter or lastGeometry.square ~= square
    if layoutChanged then
        lastGeometry = {
            mapID = mapID, floor = floor, width = width, height = height, diameter = diameter, square = square,
            count = LayoutStrips(width, height, zoneWidth, zoneHeight, square),
        }
    end

    local x = width / 2 - playerX * zoneWidth
    local y = playerY * zoneHeight - height / 2
    for index = 1, lastGeometry.count do
        local strip = strips[index]
        strip.blobs:SetPoint("TOPLEFT", strip.canvas, "TOPLEFT", x - strip.left, y + strip.top)
    end
    if questsDirty or layoutChanged then
        questsDirty = false
        DrawQuests(GetQuestIDs(), lastGeometry.count)
    end
end

local controller = CreateFrame("Frame")
controller:RegisterEvent("PLAYER_ENTERING_WORLD")
controller:RegisterEvent("PLAYER_LEAVING_WORLD")
controller:RegisterEvent("ZONE_CHANGED_NEW_AREA")
controller:RegisterEvent("ZONE_CHANGED")
controller:RegisterEvent("ZONE_CHANGED_INDOORS")
controller:RegisterEvent("WORLD_MAP_UPDATE")
controller:RegisterEvent("QUEST_LOG_UPDATE")
controller:RegisterEvent("QUEST_POI_UPDATE")
controller:SetScript("OnEvent", function(_, event)
    if event == "PLAYER_LEAVING_WORLD" then
        inWorld = false
        HideStrips()
        lastGeometry = nil
    elseif event == "PLAYER_ENTERING_WORLD" then
        inWorld = true
        mapDirty = true
    elseif event == "QUEST_LOG_UPDATE" or event == "QUEST_POI_UPDATE" then
        if not updatingQuests then questsDirty = true end
    elseif not changingMap and not updatingQuests and not WorldMapFrame:IsShown() then
        mapDirty = true
    end
end)

WorldMapFrame:HookScript("OnShow", function()
    HideStrips()
    lastGeometry = nil
end)
WorldMapFrame:HookScript("OnHide", function() mapDirty = true end)

local elapsed = 0
controller:SetScript("OnUpdate", function(_, delta)
    elapsed = elapsed + delta
    if elapsed < 0.05 then return end
    elapsed = 0
    Update()
end)
