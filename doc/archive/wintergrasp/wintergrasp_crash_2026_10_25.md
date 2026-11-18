# Crash Wintergrasp la terminarea bătăliei — 25 octombrie 2026

## Cauza identificată

Kernelul a înregistrat la **22:09:36** un general protection fault în `worldserver`, PID 977047:

```text
ip:558c2ade80d9 ... in worldserver[558c2a7da000+11e0000]
```

Segmentul executabil ELF al binarului incidentului începe la `0x439000`. Adresa relativă rezultată este:

```text
0x558c2ade80d9 - 0x558c2a7da000 + 0x439000 = 0xa470d9
```

`addr2line` rezolvă această adresă în **`BattlefieldWG::OnBattleEnd(bool)`, BattlefieldWG.cpp:737**, în bucla de curățare a vehiculelor. Disassembly-ul confirmă accesul la GUID prin iteratorul registrului de vehicule.

Bucla parcurgea direct `m_vehicles[team]`, un `unordered_set`. Pentru fiecare vehicul, apela `DespawnOrUnsummon()`. Pentru un vehicul temporar, lanțul de apeluri este:

```text
DespawnOrUnsummon
  -> TempSummon::UnSummon
  -> AddObjectToRemoveList
  -> CleanupsBeforeDelete / RemoveFromWorld
  -> BattlefieldWG::OnCreatureRemove
  -> FindAndRemoveVehicleFromList
  -> m_vehicles[team].erase(...)
```

Callback-ul șterge sincron chiar elementul folosit de bucla exterioară. Iteratorul devine invalid; incrementarea sau citirea lui ulterioară accesează memorie eliberată. Prezența vehiculelor construite de boți face această cale frecventă la încheierea luptei. Problema poate afecta și vehicule construite de jucători.

Acest incident este un acces invalid la memorie, nu o oprire prin `ASSERT`. Absența `TrinityCore.assert.log` este de așteptat: jurnalul adăugat anterior înregistrează aserțiunile, nu toate semnalele de proces. Apport nu a păstrat nici de această dată un core dump pentru executabilul neîmpachetat; localizarea de mai sus folosește adresa kernelului și simbolurile binarului original.

## Remediere

- Curățarea din `OnBattleEnd` parcurge o copie independentă a registrului, prin `WintergraspBotWar::VisitVehicleSnapshot`.
- Callback-urile native continuă să elimine vehiculele și să elibereze capacitatea în registrul real, fără a invalida parcurgerea copiei.
- La sfârșit, registrul este golit, contoarele ambelor echipe sunt resetate la zero, iar world state-urile sunt actualizate. Aceasta acoperă și GUID-uri rămase fără creatură sau eliminări amânate.
- Corecția este comună încheierii prin expirarea timpului și capturării fortăreței.

Nu au fost modificate configurația sau bazele de date pentru această remediere.

## Verificare offline

Testul nou `contrib/tests/wintergrasp_vehicle_cleanup.cpp` folosește direct helper-ul din producție. Verifică **5.808 configurații** cu 0–120 de vehicule, ordine aleatorie și callback-uri care șterg vehiculul curent, alte vehicule, întregul registru, provoacă rehash, repetă notificarea sau lansează curățare reentrantă.

Rezultate:

- Testul corectat trece cu `-Wall -Wextra -Werror`.
- Același test trece cu AddressSanitizer și UndefinedBehaviorSanitizer.
- Un executabil separat, cu modelul buclei vechi, reproduce `heap-use-after-free` în incrementarea iteratorului `unordered_set`, confirmând mecanismul defectului. Acest executabil nu este serverul și nu este instalat.
- Toate cele opt suite offline NPCBots trec.
- Nucleul `worldserver` este recompilat din sursa corectată.

Nu s-a pornit worldserver pentru aceste verificări. Retestarea unei bătălii complete rămâne necesară pentru verificarea integrării în joc.

## Dovezi și revenire

Arhiva incidentului: `/home/trinity/source-archives/wintergrasp-crash-20261025-k99ktlm4`.

Conține binarul original, logurile serverului, adresa kernelului și rezolvarea simbolică, sursele anterioare, patch-ul, rapoartele de test și manifestul instalării. Worldserver rămâne oprit; authserver-ul deja pornit de utilizator nu este modificat sau repornit.

Binarul corectat este instalat în `/home/trinity/server/bin/worldserver`. SHA256: `e0dbfdad2eb00dc90fc3ebff6e159e6ba351fa137782da140cf4a62f4082fe96`. Compilarea finală nu a emis avertismente.
