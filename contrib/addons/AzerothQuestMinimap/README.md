# Azeroth Quest Minimap

Addon pentru WoW Wrath of the Lich King 3.3.5a (build 12340). Afișează pe minimapă zonele obiectivelor active din harta mare, cu forma și culoarea desenate de client. Contururile vin din datele Quest POI pe care realm-ul le trimite deja. Addonul folosește hărțile și texturile existente în client.

## Instalare

1. Închide clientul WoW.
2. Copiază folderul `AzerothQuestMinimap` în `World of Warcraft/Interface/AddOns/`.
3. La ecranul de selectare a personajului, activează **Azeroth Quest Minimap** din **AddOns**.
4. Activează afișarea obiectivelor pe harta mare și păstrează minimapa cu nordul sus, cu opțiunea **Rotate Minimap** dezactivată.

Intră în joc cu un quest activ care are zonă de obiectiv. Apasă **M**, selectează questul și verifică zona. Închide harta și apropie-te de zonă. Conturul apare când intră în zona vizibilă a minimapei.

Addonul folosește proiecția zonei sau etajului pentru hărțile cu dimensiuni disponibile. Pe minimapa standard rotundă, decuparea la margine este aproximată prin benzi orizontale. La deschiderea hărții mari, addonul ascunde suprapunerea pentru ca navigarea pe hartă să funcționeze normal.

## Referințe

- [Interfața hărții din buildul 3.3.5a](https://github.com/wowgaming/3.3.5-interface-files/blob/main/WorldMapFrame.lua)
- [Scările de zoom din Astrolabe pentru clientul 3.3.5a](https://github.com/locus313/WoW-3.3.5a-Addons/blob/main/Gatherer/Astrolabe/Astrolabe.lua)
- Dimensiunile zonei sunt calculate din fișierele locale `WorldMapArea.dbc` și `DungeonMap.dbc`.
