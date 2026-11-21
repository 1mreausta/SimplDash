# Nissan R33 Skyline GTS-t: originaal näidikupaneeli eemaldamine

Märkmed SimplDashi paigaldamiseks R33 GTS-t (RB25DET) autosse, kui originaal näidikupaneel võetakse ära.

> **NB:** need on üldteadmised, mitte kontrollitud selle konkreetse auto elektriskeemi järgi. Enne juhtmete
> muutmist vaata hooldusraamatu elektriskeemidest (EL peatükk) oma auto mudeli ja aasta järgi.

## Mis mõjutab auto tööd

### 1. Generaatori laadimine (kõige olulisem)
Nissanil käib generaatori L-klemmi ergutus tavaliselt läbi paneelis oleva laadimise märgutule (aku tuli).
Ilma paneelita ei pruugi generaator laadima hakata → aku tühjeneb sõidu ajal.

- **Lahendus:** asenda märgutuli takistiga (~100 Ω, 2 W) või LED + takistiga samas juhtmes.
- **Kontroll:** pärast paigaldust mõõda aku pinget mootori töötades (peaks olema ~13,8–14,4 V).

### 2. Kiiruse signaal (VSS)
R33-l läheb käigukasti kiiruseanduri signaal enamasti läbi spidomeetri. Spidomeeter annab kiiruse signaali edasi
originaal ECU-le ja teistele seadmetele (nt HICAS, automaatkasti mudelitel kasti juhtplokk).

- MaxxECU puhul ühenda kiiruseandur otse ECU-sse (dash saab kiiruse ECU-lt CAN-i kaudu või GPS-ilt).
- Kontrolli, kas mõni teine seade (HICAS, püsikiirusehoidja, kasti juhtplokk) ootab kiiruse signaali paneelilt.

### 3. Kütusepaagi andur
Paagi andur on ühendatud ainult paneeliga.

- Et dash näitaks PAAK, vii anduri juhe MaxxECU analoogsisendisse (koos pull-up takistiga), seadista MTune-is
  kalibreerimine ja ECU saadab väärtuse CAN-i kaudu dashile.
- Originaal temperatuuri näidiku andur jääb kasutuks – dash võtab vee temperatuuri ECU andurilt.

## Mis kaob, aga auto töötab edasi

Märgutuled: õlirõhk, käsipidur / pidurivedelik, ABS, SRS, HICAS, turvavöö, uksed, kaugtuled, suunatuled.

- Õlirõhu ja vee hoiatused on SimplDashis olemas.
- Kaugtulede ja suunatulede märgutuled ning läbisõit on dashi tänavarežiimis (vt allpool).
- ABS, SRS ja pidurivedeliku tule kadumisel ei näe enam nende süsteemide vigu – vajadusel lisa eraldi LED-id.

## Seadus ja ülevaatus

- Juhil peavad olema nähtaval **spidomeeter**, **suunatulede märgutuli** ja **kaugtulede märgutuli** – ilma
  nendeta ülevaatus tõenäoliselt ei läbi. (Eesti nõudeid pole täpselt kontrollitud – küsi ülevaatuspunktist.)
- Ülevaatusel kirjutatakse üles läbisõit. Pane enne paneeli eemaldamist kirja vana paneeli lõpunäit ja sisesta
  see dashi odomeetrisse (telefoni veebileht → läbisõit).

## Suunatuled, kaugtuled ja läbisõit dashis

SimplDashis on **tänavarežiim** (SEADED → VAATED → Režiim = TÄNAV): ANALOOG ja TÄNAV vaates suunatuled, kaugtuled ja
läbisõit. Suunatulede ja kaugtulede signaal võetakse originaal paneeli pistikust optroni kaudu otse dashi plaadile –
paneeli pistikut ei lõigata, nii et originaal paneeli saab tagasi panna. Vt [tanavareziim.md](tanavareziim.md).
