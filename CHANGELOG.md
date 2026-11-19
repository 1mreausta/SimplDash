# Muudatused

## v0.3 – 2026-11-19
- Uus vaade **KLASSIKA**: 90ndate Jaapani sportauto stiilis osutitega kellad – spidomeeter (0–320 km/h),
  tahhomeeter (0–11 000 r/min, kollane/punane tsoon), vesi, hetke kütusekulu (L/100km, seistes L/h) ja õlirõhk.
  Käik ja kiirus väikestes akendes, DEMO/GPS märgutuled spidomeetris.
  Kütusekulu arvutatakse pihustite töötsüklist (`DASH_INJ_CC_MIN`, `DASH_CYLINDERS`; vaikimisi RB25DET 6 × 630 cc).
- RING vaates 4 + 4 näidikut külgedel (vaikimisi vesi, õli temp/rõhk, kütuse rõhk | kiirus, ülerõhk, AFR, aku;
  muuda `DASH_LAP_LEFT` / `DASH_LAP_RIGHT`). Hoiatusriba on RING vaates keskel.

## v0.21 – 2026-11-19
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
