# Muudatused

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
