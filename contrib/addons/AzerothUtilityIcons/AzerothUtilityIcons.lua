local icons = {
    [70901] = "Interface\\Icons\\INV_Misc_Rune_06",
    [70902] = "Interface\\Icons\\INV_Misc_EngGizmos_swissArmy",
}

local function IconFromLink(link)
    local itemID = link and link:match("item:(%d+)")
    return itemID and icons[tonumber(itemID)]
end

local function UpdateBagIcons(frame)
    local name, bag = frame:GetName(), frame:GetID()
    for index = 1, frame.size do
        local button = _G[name .. "Item" .. index]
        local icon = IconFromLink(GetContainerItemLink(bag, button:GetID()))
        if icon then
            SetItemButtonTexture(button, icon)
        end
    end
end

local function UpdateBankIcon(button)
    local icon = IconFromLink(GetInventoryItemLink("player", button:GetInventorySlot()))
    if icon then
        SetItemButtonTexture(button, icon)
    end
end

local function UpdateActionIcon(button)
    if not button.action then return end
    local actionType, itemID = GetActionInfo(button.action)
    local icon = actionType == "item" and icons[tonumber(itemID)]
    if icon then
        _G[button:GetName() .. "Icon"]:SetTexture(icon)
    end
end

hooksecurefunc("ContainerFrame_Update", UpdateBagIcons)
hooksecurefunc("BankFrameItemButton_Update", UpdateBankIcon)
hooksecurefunc("ActionButton_Update", UpdateActionIcon)
