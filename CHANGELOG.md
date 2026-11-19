# Muudatused

## v0.4.1 – 2026-11-19
- Telefoni veebileht: päises SimplDash logo (uus `docs/logo_web.svg` ilma tausta ja ääreta); logo ja menüü nupud
  on laias aknas samal joonel lehe sisuga.

## v0.4 – 2026-11-19
**NB:** välkmälu jaotus muutus (kaks püsivara pesa) – v0.3-lt uuendades laadi terve `simpldash-v0.4.bin` USB kaudu
aadressile 0x0 (see lähtestab püsimälu). Edaspidi saab uuendada telefonist (fail `simpldash.bin`), seaded ja läbisõit jäävad alles.
- **GPS 10 Hz:** moodul seadistatakse käivitusel (115200 baudi, RMC + GGA, 10 Hz; vajab GPS RX juhet), kiiruse
  automaatne tuvastus, mõõdetud sagedus SEADED → INFO.
- **Sõidulogi SD-kaardile** (TF-pesa): iga sõit eraldi CSV-faili, 20 Hz, ~27 ECU kanalit + GPS + hoiatused + ringid.
- **Ringiaeg:** jooksev vahe parima ringiga, sektorid S1–S3 (parimad sektorid), uus vaade **RADA** (raja kaart ja auto
  asukoht); rada jäetakse meelde ja ajavõtt algab samal rajal ise.
- **Hoiatused:** õlirõhu miinimum pöörete järgi (kõver), suur vilkuv täisekraani hoiatus (vesi, õlirõhk, kütuse rõhk,
  KNOCK; puudutus = kinnita), ECU veakood, 0,3 s viivitus piikide vastu, hoiatuste logi (40 viimast, püsimälus).
- **SEADED vaade** (vahekaardid ECU, SEADED, SÕIT, HOIATUSED, INFO; jaotiste kiirnupud, vaikeseaded): piirid, õlirõhu kõver, shift-light, pihustid, ühikud (°C/°F, bar/psi), ajavöönd, kiiruse allikas,
  logimine, Wi-Fi, ECU saatmine, vaadete peitmine – muudetavad ekraanil ja salvestatakse püsimällu; **SÕIT** (läbisõit,
  mootoritunnid, reis ja keskmine kütusekulu), **HOIATUSED** (logi), **INFO**.
- **Wi-Fi** (plaadi ESP32-C6): pääsupunkt „SimplDash“, telefoni veebileht http://192.168.4.1 – seaded, logifailide
  allalaadimine, hoiatuste logi, odomeeter, **püsivara uuendus Wi-Fi kaudu**.
- **MaxxECU:** kohandatud CAN-kanalid (kuni 8, `DASH_CUSTOM_CAN`) ja SEADED vaate esimene vahekaart **ECU** – nupud
  (LAUNCH, ANTI-LAG, tühikäigu tõstmine, SEADE, BOOST, VEOJÕUD), mis saadavad MaxxECU-le CAN-kaadri (MTune CAN inputs; vaikimisi väljas).
- KÜTUSE RÕHK hoiatuse vaikimisi piir 2,0 bar (oli 2,5 – tühikäigu vaakumiga tuli valehoiatus).
- Libistamine vahetab vaadet ka nuppude ja loendite pealt; puudutus nupul ei muuda heledust.
- Simulaator: nupud kahes reas, **Knock** (lüliti) ja **Käivitusekraan**; käivitusekraan ka lehe avamisel;
  telefoni veebileht aadressil /web; seaded ja logid jäävad meelde (`tools/sim/data/`).

### Teadaolevad piirangud
- Wi-Fi, SD-kaart, GPS-i seadistuskäsud ja CAN-i saatmine on testitud simulaatoris ja kompileerimisega, **mitte veel plaadil**.
- Wi-Fi vajab, et plaadi ESP32-C6-l oleks esp-hosted püsivara (Guition saadab selle tavaliselt kaasa).

## v0.3 – 2026-11-19
- Uus vaade **KLASSIKA**: 90ndate Jaapani sportauto stiilis osutitega kellad – spidomeeter (0–320 km/h),
  tahhomeeter (0–11 000 r/min, kollane/punane tsoon), vesi, hetke kütusekulu (L/100km, seistes L/h) ja õlirõhk.
  Käik ja kiirus väikestes akendes, DEMO/GPS märgutuled spidomeetris.
  Kütusekulu arvutatakse pihustite töötsüklist (`DASH_INJ_CC_MIN`, `DASH_CYLINDERS`; vaikimisi RB25DET 6 × 630 cc).
- RING vaates 4 + 4 näidikut külgedel (vaikimisi vesi, õli temp/rõhk, kütuse rõhk | kiirus, ülerõhk, AFR, aku;
  muuda `DASH_LAP_LEFT` / `DASH_LAP_RIGHT`). Hoiatusriba on RING vaates keskel.

## v0.2.1 – 2026-11-19 (algselt v0.21)
- GPS ikoon on igas vaates paremas servas, DEMO märk sellest vasakul.
- 0–100: GPS-i hüpped ja katkestused (kiirendus üle 60 km/h/s või üle 0,5 s vahe) ei anna enam vale tulemust.
- README pildid 0–100 ja RING vaadetest näidisandmetega.
- Simulaatoris (demo + GPS) märgitakse start/finiš joon automaatselt ja vaadetes on kohe varasemad
  demotulemused; demosõidu kiirendused ja ringid lisanduvad jooksvalt.

## v0.2 – 2026-11-19

**GPS** (valikuline): nt Ebyte E108-GN04G-TTL UART-i kaudu (pinnid `idf.py menuconfig` → SimplDash, vaikimisi väljas).
- GPS tugi (NMEA, nt Ebyte E108-GN04G-TTL): kiiruse allikas ECU / GPS / AUTO, GPS ikoon, kell.
- Uued vaated (ainult GPS-iga): 0–100 km/h aeg ja ringiaja taimer.
- Simulaatorites võlts-GPS (ovaalne rada) ning nupud GPS-i ja kiiruse allika jaoks.
- Parandus: mitu kiiret vaate vahetust (nt käivitusekraani ajal) liigutas ainult ühe vaate võrra.

### Teadaolevad piirangud
- GPS-i tugi on testitud simulaatorites (päris NMEA lausetega), aga **mitte veel päris GPS-mooduliga**.
- Mooduli uuendussageduse (nt 10 Hz) seadistamist veel pole – kasutatakse mooduli vaikeseadet.

## v0.1 – 2026-11-19

Esimene reliis.

**Riistvara:** Guition JC1060P470C-I-W-Y (ESP32-P4, 7" 1024×600 puuteekraan) + SN65HVD230 CAN-transiiver
(CAN TX = GPIO45, RX = GPIO46, valikuline vaate nupp GPIO47).

### Mis sees on
- **MaxxECU CAN:** MaxxECU Default 1.2/1.3 protokoll, 500 kbit/s; kõik 0x520–0x542 kaadrid, olekulipud
  (LAUNCH, ALS, LIMIT, KNOCK, LOG), ECU pöördepiir; bus-off korral taastub ise.
- **Viis vaadet**, vahetus libistades või nupuga, sujuv üleminek, viimane vaade jäetakse meelde:
  RACE, KELLAD, KAAR, TÄNAV, ANDMED (min/max tipud).
- **Hoiatused** (vesi, õli temp/rõhk, aku, kütuse rõhk, EGT): ribateade ja punane raam kõigis vaadetes.
- **Shift light'id** ja tahhomeetrid (sinine → kollane → punane, käiguvahetuse hetkel vilgub siniselt).
- **Käivitusekraan** SimplDashi logoga (2 s) ja tulede testiga.
- **Heledus** puudutusega (100/50/20 %), **demorežiim** (hoia 3 s), "ECU ühendus puudub" teade.
- Kõik seaded ühes failis: `main/dash_config.h`.
- **Simulaatorid:** brauseris (https://1mreausta.github.io/SimplDash/), Macis (`tools/sim/run.sh`) ja
  piltide eelvaade (`tools/preview/build.sh`).

### Teadaolevad piirangud
- Päris plaadil ja autos **pole veel testitud** – kood on kontrollitud kompileerimise, simulaatorite ja
  dekoodri testidega. Esimesel käivitusel jälgi `idf.py monitor` väljundit.
- Ainult MaxxECU Default CAN protokoll (kohandatud CAN-kaadreid veel ei toetata).
