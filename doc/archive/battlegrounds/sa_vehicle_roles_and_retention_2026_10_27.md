# Strand of the Ancients: șoferi, porturi și apărare

Corecții din 27 octombrie 2026.

## Șoferi și obiective

Șoferul păstrează vehiculul sănătos când un jucător trece prin apropiere.
Predarea voluntară către un jucător cere selectarea vehiculului, distanța de
interacțiune, aceeași fază și o echipă prietenoasă. Prioritatea jucătorului la
un vehicul liber continuă să fie respectată.

Vehiculele cu mai puțin de 15% viață sunt abandonate și excluse din alegerea
următorului vehicul până când devin din nou utilizabile. Astfel, expirarea
cooldownului nu retrimite botul în același vehicul aproape distrus.

Vehiculele atacatoare primesc porți ca obiective. Steagurile sunt alese numai
pentru infanterie, inclusiv când ID-ul unui șofer coincide cu rolul de capturator.
Când relicva devine accesibilă, echipajele coboară și continuă obiectivul pe jos.

## Mount și alegerea vehiculului

Un timer scurt blochează montarea lângă obiectiv și în timpul apropierii de
vehicul. Cooldownul de preluare/predare blochează și el montarea; starea se
resetează între runde. Călătoria pe distanțe mari poate folosi mountul normal.

Atacatorii SA caută vehicule până la 300 m, astfel încât vehiculele de la
celălalt port sunt luate în considerare. Înainte să aleagă un vehicul aflat
la distanță, verifică întregul drum, inclusiv toate prefixele Detour și porțile
intacte. Când nu există o opțiune disponibilă și accesibilă, căutarea returnează
controlul tactic deplasării pe jos spre front. Nu există ordin de a aștepta la port.

Apropierea nu reemite inutil același waypoint. Îmbarcarea păstrează mecanismul
nativ asincron `VehicleJoinEvent`; lipsa temporară a pasagerului imediat după
`EnterVehicle` nu este interpretată ca o îmbarcare eșuată.

## Apărare și tunuri

Poziția implicită a infanteriei este în fața porții active, pe partea de asalt.
Două roluri din fiecare grup logic de cinci pot folosi cele două tunuri native
ale porții respective. Dacă postul preferat este ocupat, verifică al doilea.
Dacă ambele sunt indisponibile, păstrează poziția de infanterie din fața porții.
Vindecătorii rămân pe jos.

Selecția tunurilor este limitată la perechea porții active. După schimbarea
frontului, un bot nu este trimis din nou spre tunurile unei porți depășite.
Camera Ancient păstrează apărarea la intrare, cu obiectele native disponibile.
Deplasarea și coborârile continuă să verifice coliziunea; nu există trecere
specială prin porțile intacte pentru apărători.

## Verificări

- `sa_vehicle_retention.py`: execută codul real de păstrare a vehiculului,
  cerere a jucătorului, selecție/îmbarcare, cooldownuri și mount; verifică
  vehiculele de la celălalt port, opțiunile ocupate/inaccesibile și evenimentul
  asincron de îmbarcare.
- `sa_objectives.py`: verifică faptul că niciun rol aflat într-un vehicul
  nu primește steaguri, păstrând prioritatea relicvei și capturile infanteriei.
- `sa_gate_movement.py`: pozițiile din fața porților, ambele posturi de tun,
  postul alternativ și revenirea la infanterie când tunurile sunt indisponibile.
- `sa_gate_collision_data.py`: modele WMO/vmaps/mmaps reale, 768 traversări
  pentru cele 64 stări ale porților, 17 drumuri către fronturi/tunuri și
  12 apropieri de vehicule de la spawn și ambele porturi.
- Regresiile pregătirii ambelor runde, deplasării SA și teleporterelor IC.
- Compilarea și linkarea țintei `worldserver`; `git diff --check`.

Nu sunt necesare SQL sau configurații noi. Verificarea vizuală a unei partide
complete rămâne de făcut în client.
