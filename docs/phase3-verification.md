# Fase 3 — verifiche Checkpoint 3A, 3B, 3C e 3D

Stato attuale: **3D hardware eseguito con limiti documentati; attesa approvazione**.
Password AP fissa `12345678`. Risultati 3D nella sezione finale.
Le precedenti sezioni sono resoconti storici dei singoli checkpoint; i loro
NOT RUN e stop si riferiscono a quel momento. Le evidenze 3A/3B seguenti
sono conservate come cronologia; nessun upload della Fase 3 eseguito.

## Checkpoint 3A software

Data: 6 novembre 2026. Ambito autorizzato: solo 3A. Nessun upload, comando
hardware, scrittura/cancellazione delle credenziali del dispositivo, commit,
merge o push. 3B/3C/3D non avviati.

## Base e repository

Workspace effettivo `/Users/franc/apps/axio`, come nell'ambiente della sessione
(il prompt indicava anche `~/Documents/axio`). Branch già presente
`feat/wifi-connectivity`, HEAD esatto
`010b9a91bad5c9153c01aecd5fd260e23e0e358a`, working tree iniziale pulito.
Non è stato necessario creare branch o spostare modifiche locali.
Letti prima delle modifiche AGENTS.md, README, i due documenti Fase 2,
LegacyTransport, OscRuntime, diagnostica USB e sketch effettivi.

AGENTS.md ora autorizza esclusivamente 3A e richiede una nuova approvazione
per i checkpoint seguenti e per qualsiasi upload/commit/merge/push.
Architettura, limiti e comandi: [phase3-connectivity.md](phase3-connectivity.md).

## Implementato

- Manager STA separato dall'IMU: otto reti, priorità stabile, timeout 5 s,
  disconnessione con conferma, backoff 1–30 s, reconnect, rollover unsigned.
- Journal Wi-Fi indipendente a due slot, CRC e readback byte per byte prima
  del commit; validazione e attivazione solo dopo successo persistente.
- Migrazione della rete SDK senza sovrascriverla; uso volatile se il salvataggio
  fallisce, recupero esplicito da journal invalido, elenco vuoto persistente.
- Comandi USB WIFI GET/SET/DELETE, nessuna password nelle risposte. STATUS,
  INFO e METRICS estesi senza output periodico o cambi al protocollo v1.
- WiFiManager e portale esclusi dal percorso attivo. Bootstrap Wi-Fi solo
  nel setup; tick senza attese applicative o scansioni esplicite.
- OSC sospeso senza STA/IPv4, listener legato all'IP STA, chiuso alla perdita
  e ricreato al ritorno/cambio IP; retry bind limitato. Metriche separate per
  sospensione STA/listener e fallimenti UDP. Schema/journal/scheduler OSC invariati.

## Verifiche eseguite

Comandi diretti, senza concatenazioni o redirezioni shell:

```sh
bash scripts/compile.sh
python3 -B -m unittest discover -s tests -v
git diff --check
git diff --exit-code -- reference/HandTrackerV1_1.ino tests/fixtures/phase0-Axio.ino firmware/Axio/hardware_config.h firmware/Axio/src/core firmware/Axio/src/osc
git rev-parse HEAD
git status --short --branch
```

Output finale conservato attraverso gli strumenti della sessione in
`.build/phase3-3a-compile.log` e `.build/phase3-3a-tests.log`, ignorati da Git.
La suite crea i propri eseguibili sotto `.build/tests/`; nessuna installazione.
Toolchain invariata: CLI 1.5.1, esp32 3.3.12, BNO055 1.6.4, BusIO 1.17.4,
Unified Sensor 1.1.15, OSC 4.0.0, WiFiManager 2.0.17 verificato dal lock.
La verifica versione WiFiManager non implica che sia linkato nel firmware 3A.

| Verifica | Esito | Evidenza e limite |
| --- | --- | --- |
| Branch, HEAD e stato iniziale | PASS | Branch richiesto, commit B esatto, tree iniziale pulito |
| Compilazione XIAO ESP32C3 | PASS | Stessa partizione, opzioni e dipendenze; nessun upload |
| Suite completa finale | PASS | **41 test**, 28 regressioni e 13 nuovi; 6,708 s nell'ultima esecuzione |
| Matematica, reference e fixture | PASS | **1.345 vettori** invariati, baseline/core contro oracolo; controllo byte/hash storico |
| Core, pin e moduli OSC | PASS | Nessun diff in core/, osc/, hardware_config.h, reference e fixture |
| Priorità, parità e duplicati | PASS | Tentativi high/tie/low con stesso ordine a parità, duplicati rifiutati |
| Timeout/backoff/rollover | PASS | Confini 5.000 ms, 200 ms; backoff 1/2/4/8/16/30/30 s, rollover uint32, assenza catch-up |
| Perdita/ripresa STA | PASS | Driver fake, contatori e ripartenza, nessun tentativo in connessione stabile |
| Configurazioni invalide | PASS | Lunghezze, SSID vuoto/NUL, priorità, duplicati, PSK 64 hex, password corta, capacità |
| Journal e failure injection | PASS | Scrittura, commit e readback falliti, reboot di nuovi oggetti, CRC corrotto, recupero slot precedente |
| Migrazione | PASS | Commit fallito, fallback storico volatile, journal incompleto, elenco vuoto salvato; SDK fake invariato |
| USB e segreti | PASS | Codice runtime reale, SET/upsert/DELETE invalido, GET, reload, STATUS/INFO/METRICS senza password |
| Separazione storage Wi-Fi/OSC | PASS | Due adattatori reali su NVS fake con namespace distinti; reload mantiene entrambe le configurazioni |
| OSC senza STA | PASS | Su host: zero tentativi UDP e zero poll listener, contatore sospensione separato |
| OSC reconnect/cambio IPv4 | PASS | Runtime reale e socket fake: chiusura, un solo bind attivo stabile, ripresa senza CONFIG APPLY |
| Bind fallito e retry | PASS | Nessun errore UDP fittizio, retry dopo 1 s e ripresa senza APPLY |
| Diagnostica massima | PASS | Risposta JSON valida con otto reti e 16 messaggi OSC; nessuna password |
| Assenza attese applicative | PASS | Tick/stati compilati ed esercitati; controllo sorgenti contro delay/wait/scansioni/autoConnect |
| Whitespace e diff protetti | PASS | git diff --check e diff --exit-code sui percorsi protetti |
| Upload, USB e migrazione XIAO reali | NOT RUN | Non autorizzati in 3A; NVS fisica mai toccata |
| Reconnect Wi-Fi, DHCP e listener LAN reali | NOT RUN | Fake driver/socket non misurano la radio o il router |
| Heap libero/minimo, jitter, rate IMU/OSC | NOT RUN | Nessuna misura hardware 3A |
| AP protetto e OSC su AP-only reale | NOT RUN | Funzione AP esclusa dal Checkpoint 3A |
| mDNS e discovery LAN | NOT RUN | Funzioni escluse fino a 3C |
| Power-cut, NVS piena fisica, endurance | NOT RUN | Solo fault injection host, nessuna prova distruttiva |

FAIL intermedi corretti: la prima compilazione ha rilevato una collisione del
nome `Network` con il core ESP32; tipo rinominato `WifiNetwork`. Un test iniziale
pretendeva un vecchio slot ancora disponibile dopo averlo sovrascritto con una
scrittura non committata: il caso di corruzione è stato isolato su un journal
nuovo con due generazioni committate. Il limite della doppia anomalia è
documentato, senza attribuire al journal un recupero che non garantisce.
**Nessun FAIL finale irrisolto.**

Warning osservato nella build 3A: `OSCMessage.cpp:897`, implicit fallthrough
nella dipendenza OSC 4.0.0, già noto. La build incrementale finale non lo
ristampa. Nessun warning Axio irrisolto osservato; nessuna dipendenza modificata.

## Memoria e confronto B

| Misura | Checkpoint B finale | 3A build finale | Differenza |
| --- | ---: | ---: | ---: |
| Flash | 1.163.029 B | **1.034.109 B** | **−128.920 B** |
| RAM statica | 94.116 B | **98.116 B** | **+4.000 B** |
| RAM residua statica | 233.564 B | **229.564 B** | −4.000 B |
| Heap libero/minimo hardware | 133.084 / 129.068 B | NOT RUN | Non deducibile dalla RAM statica |
| Acquisizioni IMU hardware | 99,947 Hz | NOT RUN | Nessuna equivalenza dichiarata |
| OSC Mac, cattura 12 s | 99,942 pacchetti/s | NOT RUN | Nessuna equivalenza dichiarata |

Flash 78% di 1.310.720 B secondo Arduino CLI, margine 276.611 B; RAM globale
29% di 327.680 B. La riduzione flash è coerente con l'esclusione del portale
WiFiManager, mentre configurazioni, buffer journal e risposte Wi-Fi aumentano
la RAM statica. Il saldo non misura heap, frammentazione, stack o tempi del loop.
Non sono trasferite alla Fase 3 le prestazioni reali misurate nella Fase 2.

## Rischi e decisioni successive

- Firmware intermedio senza AP: se nessuna rete è utilizzabile si configura
  via USB. Il vecchio portale aperto non viene avviato; 3B richiede approvazione.
- Migrazione verificata su host e contro il codice del core installato, ancora
  da confermare su XIAO senza alterare la copia SDK originale.
- Bootstrap SDK e NVS sono sincroni. Nessuna attesa intenzionale nel tick,
  ma latenza, contesa radio, singolo retry interno SDK e impatto IMU richiedono
  misure reali. Il bootstrap iniziale può attendere fino a 1 s prima del loop.
- Nessuna preemption per priorità/RSSI dopo connessione; reti enterprise/WEP
  escluse. Otto reti e budget 5 s sono limiti iniziali documentati, da valutare
  nell'uso reale senza aumentare l'aggressività dei tentativi.
- Fallimento di disconnessione trattiene lo stato di assestamento, evitando
  tentativi sovrapposti; un driver bloccato resta diagnosticabile via USB.
- Scelta/provisioning/recupero della password AP sicura resta una decisione
  3B. Nessuna password AP vuota o derivata da identificatore pubblico introdotta.
- Identità univoca/mDNS/discovery e limiti multicast da implementare solo in 3C.
  Non c'è ancora isolamento di una subnet AP simultanea da verificare in 3A.

## Stato Git e inventario

HEAD e branch invariati. Modifiche solo nel working tree, nessun file staged,
commit, merge o push. `.build/` e captures esclusi dall'indice.

Modificati (11):

- `AGENTS.md`, `README.md`, `firmware/Axio/src/README.md`.
- `firmware/Axio/Axio.ino`.
- `firmware/Axio/src/diagnostics/DiagnosticService.h`.
- `firmware/Axio/src/transport/LegacyTransport.cpp`, `LegacyTransport.h`, `OscRuntime.cpp`.
- `tests/stubs/runtime/Preferences.h`, `WiFi.h`, `WiFiUdp.h`.

Nuovi (9):

- `firmware/Axio/src/connectivity/ConnectivityManager.h`, `ConnectivityRuntime.h`, `ConnectivityRuntime.cpp`.
- `tests/connectivity_harness.cpp`, `tests/connectivity_runtime_harness.cpp`, `tests/test_connectivity.py`.
- `tests/stubs/runtime/esp_wifi.h`.
- `docs/phase3-connectivity.md`, `docs/phase3-verification.md`.

Checkpoint 3A concluso sul software. Attendere approvazione prima di 3B.

## Checkpoint 3B — software completato dopo approvazione

Autorizzazione successiva dell'utente: «Continua con 3B». Lavoro eseguito nello
stesso workspace e branch `feat/wifi-connectivity`, conservando integralmente
le modifiche 3A non committate. HEAD resta
`010b9a91bad5c9153c01aecd5fd260e23e0e358a`.
AGENTS.md aggiornato al solo 3B, con stop prima di 3C/3D. Nessun upload,
operazione sul dispositivo, commit, merge o push. Le verifiche AP su host
non sono presentate come misure della radio XIAO.

### Funzioni implementate

- Fallback AP dopo 15 s offline, con STA multi-rete ancora gestita da 3A;
  avvio asincrono, timeout 3 s, retry 10 s e spegnimento dopo successo STA.
- SSID stabile `Axio-<MAC STA completo>`, WPA2-PSK/CCMP, massimo due client,
  password casuale 128 bit memorizzata in namespace `axio-ap-v1` separato.
- Segreto verificato su readback e CRC; NVS/record/profilo invalido lasciano
  AP disabilitato. Recupero esplicito USB AP CREDENTIALS, rotazione AP REKEY.
  Nessuna password in diagnostica pubblica o nelle pagine web.
- Profilo AP protetto preparato a radio ferma e ritorno a STA prima dello
  start radio. Evitate softAP/softAPdisconnect, che nel core ispezionato
  abilitano prima di configurare o cancellano il profilo.
- Provisioning separabile, bind solo su IPv4 AP, form salva/elimina una rete,
  stato ed elenco pubblici; nessuna dashboard, DNS captive, OTA o editor OSC.
- HTTP non bloccante con buffer/budget fissi, timeout assoluto, controllo
  Host/Origin/token, rifiuto richieste troppo grandi/duplicate/malformate.
- Chiusura client/listener HTTP al ritorno STA, retry spegnimento AP; blocco
  totale OSC quando AP è abilitato, anche con STA connessa. Ripresa su STA
  senza CONFIG APPLY. Contatore separato `suspended_ap`.

La persistenza STA di 3A e il journal OSC non sono stati cambiati; il percorso
web usa il validatore/journal STA esistente. Nessuna modifica al core, ai pin,
alla matematica, alla reference o alle fixture. Nessun reset automatico della
tara o della diagnostica. Namespace SDK e configurazioni fisiche mai toccati
durante le prove software.

### Risultati

Comandi diretti finali:

```sh
bash scripts/compile.sh
python3 -B -m unittest discover -s tests -v
git diff --check
git diff --exit-code -- reference/HandTrackerV1_1.ino tests/fixtures/phase0-Axio.ino firmware/Axio/hardware_config.h firmware/Axio/src/core firmware/Axio/src/osc
git rev-parse HEAD
git status --short --branch
```

Log locali: `.build/phase3-3b-compile.log`, `.build/phase3-3b-tests.log`;
nessuna redirezione shell, installazione o credenziale reale nei log.

| Verifica | Esito | Evidenza e limite |
| --- | --- | --- |
| Build XIAO, toolchain e partizione invariate | PASS | Nessun upload; CLI 1.5.1/core 3.3.12 e lock precedenti |
| Suite completa | PASS | **56 test**, 15 aggiunti rispetto ai 41 finali 3A; ultima esecuzione 8,113 s |
| Regressioni storiche | PASS | **1.345 vettori** invariati, baseline/core/oracolo identici |
| Diff percorsi protetti e whitespace | PASS | Exit 0 dei due comandi Git, nessun diff in core/osc/pin/reference/fixture |
| Fallback, perdita/ripresa e rollover | PASS | Clock sintetico, soglia 14.999/15.000 ms, avvio confermato/pending, chiusura alla STA |
| Retry limitati e spegnimento fallito | PASS | Timeout avvio 3 s, retry 10 s; retry stop 1 s senza raffiche |
| Generazione e persistenza AP | PASS | Sorgente casuale fake per host, record 40 B, reboot ricarica senza rigenerare, REKEY cambia segreto |
| Errori storage e profilo radio | PASS | NVS assente, scrittura/readback/CRC e profilo/start radio falliti non avviano AP; nessuno start radio aperto nel fake |
| Recupero USB e riservatezza | PASS | AP CREDENTIALS solo esplicito, normale diagnostica senza segreto; corruzione rifiutata, recupero REKEY intenzionale |
| HTTP e form | PASS | Codice parser reale, RX/TX parziali, URL encoding, modifica valida, errore persistenza e input invalido |
| HTTP limiti e protezioni | PASS | Header 1.024 B/body 512 B, duplicati/TE/NUL, token/Host/Origin, no-store; timeout RX/TX con rollover |
| TCP reale loopback host | PASS | Stesso adattatore socket non bloccante e parser di produzione: GET /, /status, /networks su 127.0.0.1 |
| Provisioning usa journal STA | PASS | Funzione runtime reale salva una rete, reload la rilegge, elimina senza alterare la rete SDK migrata |
| OSC AP-only | PASS | Runtime reale con interfacce/socket fake: nessun invio né listener, sospensione contabilizzata senza errori UDP |
| STA presente ma AP stop fallito | PASS | OSC resta sospeso in `suspended_ap`; al successo stop riprende senza CONFIG APPLY |
| Diagnostica massima | PASS | Elenco otto reti e 16 messaggi OSC, nuovo oggetto AP, JSONL ancora valido |
| AP/radio/DHCP e provisioning XIAO | NOT RUN | Nessun upload o modifica della NVS fisica autorizzati/eseguiti |
| Assenza AP aperto su etere | NOT RUN | Sequenza software protetta verificata su host; richiede osservazione radio reale |
| Socket/OSC LAN/AP reali | NOT RUN | Loopback host non equivale a una rete Wi-Fi o alla XIAO |
| Reboot e segreto persistente reali | NOT RUN | Solo memoria fake/ricostruzione oggetti su host |
| Heap/jitter/rate IMU e OSC 3B | NOT RUN | Nessuna prestazione hardware ereditata dal Checkpoint B |
| mDNS/discovery | NOT RUN | Non implementati: richiedono approvazione 3C |
| Fault NVS/power-cut e rotazione reale | NOT RUN | Nessuna prova distruttiva o modifica dei segreti del dispositivo |

FAIL intermedio corretto: l'esecuzione isolata di `test_connectivity.py`
dipendeva dall'import dei tool effettuato da un altro test della suite 3A;
aggiunta la dichiarazione esplicita del percorso `tools`. Suite isolata e
completa poi PASS. Nessun FAIL finale irrisolto.
Warning osservato nella prima build 3B: fallthrough in OSCMessage.cpp:897,
dipendenza OSC 4.0.0 già nota. La build incrementale finale non lo ristampa.
Nessun warning Axio irrisolto osservato.

### Memoria

| Misura | B finale hardware | 3A software | 3B software | 3B − 3A | 3B − B |
| --- | ---: | ---: | ---: | ---: | ---: |
| Flash | 1.163.029 B | 1.034.109 B | **1.048.457 B** | **+14.348 B** | −114.572 B |
| RAM statica | 94.116 B | 98.116 B | **104.076 B** | **+5.960 B** | +9.960 B |
| RAM residua statica | 233.564 B | 229.564 B | **223.604 B** | −5.960 B | −9.960 B |

Flash 79% secondo CLI, stessa partizione da 1.310.720 B, margine 262.263 B.
RAM globale 31% di 327.680 B. I buffer HTTP fissi occupano gran parte
dell'incremento statico; i socket e il netif AP hanno anche costi dinamici
che il linker non misura. Heap libero/minimo 3B **NOT RUN**, non dedotti dai
133.084/129.068 B del firmware B. Anche 99,947 Hz IMU e 99,942 pacchetti/s OSC
Mac restano esclusivamente riferimenti storici B.

### Rischi e piano hardware

Piano concreto, non eseguito: [phase3b-hardware-plan.md](phase3b-hardware-plan.md).
Richiede autorizzazione hardware, porta verificata e rete di prova dedicata;
non elimina reti salvate per forzare il fallback e non esegue AP REKEY per
iniziativa del collaudo. Specifica criteri PASS/FAIL/NOT RUN, controllo USB,
osservazione WPA2, DHCP, provisioning, spegnimento AP e ripresa OSC, reboot,
heap e finestre IMU/OSC. Nessun TARE o DIAG RESET.

Rischi ancora aperti: comportamento reale stop/start radio nel bootstrap,
latenza API SDK/NVS, APSTA sullo stesso canale, copertura della soglia di
fallback sotto carico, heap/stack/socket e frammentazione. Il portale HTTP
non usa TLS: chi possiede il segreto AP è autorizzato al provisioning.
USB resta la via di recupero fisico dei segreti. Un successo STA può chiudere
la pagina prima della conferma browser; controllare USB prima di ripetere.
Nessuna promessa di prestazioni o connettività universali.

### File e stato Git

Le modifiche 3A sono preservate. Rispetto alla fine di 3A, nuovi file 3B:

- `firmware/Axio/src/connectivity/ApRuntime.h`, `ApRuntime.cpp`, `FallbackAp.h`.
- `firmware/Axio/src/connectivity/ProvisioningHttp.h`, `ProvisioningSocket.h`.
- `tests/ap_harness.cpp`, `tests/provisioning_http_harness.cpp`, `tests/provisioning_socket_harness.cpp`.
- `tests/test_ap.py`, `tests/stubs/runtime/esp_random.h`.
- `docs/phase3b-hardware-plan.md`.

Aggiornati in 3B: AGENTS.md, README.md, src/README.md, DiagnosticService.h,
OscRuntime.cpp, ConnectivityRuntime.h/.cpp, test_connectivity.py,
connectivity_runtime_harness.cpp, stub WiFi.h/esp_wifi.h e i due documenti
phase3-connectivity/phase3-verification. I file già nuovi in 3A restano
untracked, pronti per revisione; nessuno è stato aggiunto all'indice.
Totale rispetto a HEAD: **11 file tracciati modificati e 20 file nuovi**.
Nessun commit, merge o push; .build/ e catture non incluse.

Checkpoint 3B concluso sul software. Fermarsi qui in attesa di approvazione;
non iniziare 3C o 3D.

## Modifica successiva — password AP richiesta esplicitamente

Su richiesta dell'utente, la password AP è ora **`12345678`**, con WPA2-PSK/CCMP.
Questo requisito sostituisce la password casuale descritta nelle evidenze 3B
precedenti. Il token HTTP resta casuale. Nessun upload o intervento sulla NVS
fisica eseguito; resta valido lo stop prima di 3C/3D.

Il record AP passa ad AXP2, mantenendo namespace e dimensione 40 B, CRC e
readback. Un record AXP1 valido viene migrato alla password fissa al prossimo
boot del nuovo firmware; una scrittura fallita impedisce l'avvio AP, senza
fallback aperto. Un record corrotto resta un errore esplicito. AP REKEY è
conservato come ripristino della stessa password, con risposta `restored:true`.
Credenziali STA e journal OSC non vengono modificati dalla migrazione AP.

- **PASS:** compilazione XIAO, flash **1.048.493 B**, RAM statica **104.044 B**
  (rispetto al 3B precedente: +36 B flash, −32 B RAM).
- **PASS:** suite completa, **56 test** e **1.345 vettori** invariati; verifica
  password esatta, migrazione AXP1→AXP2, reboot, ripristino e failure injection.
- **PASS:** `git diff --check`.
- **NOT RUN:** upload, password/AP/radio sulla XIAO e prestazioni hardware.

File aggiornati: FallbackAp.h, ApRuntime.cpp, relativi harness/test, AGENTS.md,
README e documenti di connettività/verifica/piano hardware. Nessun commit/push.
Le verifiche intermedie hanno rilevato un errore di quoting C++ corretto e
un controllo test troppo ampio: il MAC pubblico del fake conteneva le stesse
cifre della password. Il controllo ora verifica i campi e i valori JSON esatti,
senza confondere il MAC con una password esposta. Nessun FAIL finale irrisolto.

## Checkpoint 3C — hostname, mDNS e discovery: software completato

La richiesta di continuare è stata interpretata come avvio del successivo
Checkpoint 3C; le successive istruzioni AGENTS.md dell'utente hanno confermato
esplicitamente questo ambito: test host e piano LAN, stop prima di 3D.
Il riavvio della sessione ha interrotto il lavoro senza perdita delle modifiche:
stato e sorgenti sono stati verificati prima di completare documentazione e
verifiche finali. Nessun upload, commit, merge, push o modifica della NVS fisica.

### Implementazione

- Hostname hardware `axio-<MAC STA completo>`, applicato prima di avviare Wi-Fi;
  risoluzione `.local` sulla STA. Nome effettivo con eventuale suffisso numerico
  aggiornato nella diagnostica, senza dipendenza dall'ID OSC modificabile.
- Servizio `_axio._udp`, porta del listener OSC corrente, TXT pubblici limitati
  a `model=Axio`, `schema=1`, `hw=<MAC>`. Nessuna password, SSID o destinazione
  OSC annunciata, nessun HTTP sulla LAN.
- Query asincrone ESP-IDF con polling a timeout zero, prima ricerca dopo
  2–5 s, poi intervalli 30–35 s, una query da 2 s alla volta. Retry init 5 s.
- Cache fissa di 16 peer, deduplica hardware, filtri su netif STA/IPv4/TXT/nome,
  TTL massimo 90 s e gestione rollover. Nessun salvataggio NVS dei peer.
- Perdita STA/cambio IP: interfaccia scollegata dal responder, cache svuotata,
  risultati pendenti scartati e liberati prima di eliminare query/responder.
  Nessun responder o query discovery in AP-only.
- DISCOVERY GET legge la cache; INFO/STATUS/METRICS aggiungono stato mDNS,
  hostname e metriche discovery. Nessuna query forzata dalla diagnostica.
- Porta annunciata aggiornata dopo CONFIG APPLY; errori mDNS separati da UDP
  e nessuna modifica al routing, allo schema, al journal o alla tara OSC.

La password AP resta `12345678`; conservate tutte le modifiche 3A/3B precedenti.
Usate le API del componente espressif__mdns già incluso nel core 3.3.12,
ispezionando header e wrapper Arduino installati. Nessuna installazione o nuova
dipendenza esterna. Evitato queryService/query_ptr bloccante del wrapper.

### Verifiche finali

```sh
bash scripts/compile.sh
python3 -B -m unittest discover -s tests -v
git diff --check
git diff --exit-code -- reference/HandTrackerV1_1.ino tests/fixtures/phase0-Axio.ino firmware/Axio/hardware_config.h firmware/Axio/src/core firmware/Axio/src/osc
git rev-parse HEAD
git status --short --branch
git diff --cached --stat
```

Log sotto `.build/phase3-3c-compile.log` e `.build/phase3-3c-tests.log`, conservati
dagli strumenti della sessione senza redirezioni shell; file ignorati da Git.

| Verifica | Esito | Evidenza e limite |
| --- | --- | --- |
| Build XIAO ESP32C3 | PASS | Core/toolchain/partizione invariati, nessun upload |
| Suite completa | PASS | **71 test**, 15 aggiunti rispetto ai 56 finali 3B; ultima esecuzione 9,045 s |
| Matematica e regressioni storiche | PASS | **1.345 vettori** invariati, baseline/core/oracolo identici |
| Percorsi protetti e whitespace | PASS | Exit 0, nessun diff in core/osc/pin/reference/fixture |
| Identità e nome di conflitto | PASS | MAC completo stabile, formato validato, suffisso numerico riportato dal runtime fake |
| Query e scheduling | PASS | API asincrone reali adattate al fake, attesa zero, pacing, rollover, nessun catch-up |
| Cache peer e TTL | PASS | Aggiornamento IP/porta/nome, scadenza 90 s, deduplica, self escluso, goodbye completo, overflow a 16 |
| Filtri discovery | PASS | Modello/schema/hw, nome, IPv4 unicast, netif STA; risultati AP/schema errato scartati |
| Disconnessione e query pendente | PASS | Cache svuotata, risultati tardivi non accettati, handle eliminato prima di mdns_free |
| Cambio IP e reconnect | PASS | Drain e ricreazione, nessun record della vecchia sessione |
| Fallimenti init/service/query/port | PASS | Cleanup init parziale, retry limitati, errore discovery distinto da UDP, OSC preservato |
| Porta SRV e CONFIG APPLY | PASS | Modifica listener OSC seguita dal servizio, configurazione invariata dopo recupero da errore mDNS |
| Diagnostica USB | PASS | DISCOVERY GET, campi INFO/STATUS/METRICS, nessuna password nei dati discovery |
| Diagnostica massima precedente | PASS | Otto reti e 16 messaggi OSC ancora producono JSONL valido con i nuovi campi |
| AP e password fissa | PASS | Tutte le regressioni 3B, inclusi AP-only, stop AP fallito, segreto `12345678` e migrazione AXP2 |
| Ambiente Bonjour sul Mac | PASS | dns-sd presente; daemon di sistema versione 2881.160.4 verificato, senza annunciare tracker di prova |
| Risoluzione/discovery su LAN reale | NOT RUN | Nessun annuncio di prova creato, nessun firmware 3C caricato sulla XIAO |
| Due tracker, cambio IP e conflitto radio reali | NOT RUN | Richiedono dispositivi caricati e rete di prova concordata |
| Assenza mDNS su AP reale | NOT RUN | Solo lifecycle/netif fake; da confermare sulla radio |
| Heap minimo/libero, jitter e rate 3C | NOT RUN | Nessuna misura hardware ereditata dal firmware B |
| Collaudo 3D e dashboard | NOT RUN | Esclusi dall'autorizzazione attuale |

Un primo controllo dell'ambiente Bonjour nel sandbox non poteva accedere a
route/mDNSResponder; la lettura fuori sandbox autorizzata ha confermato en0
come interfaccia predefinita e la versione del daemon. La successiva lettura IP
è stata interrotta dal riavvio della sessione: non è stata ripetuta, dato che
l'ambito AGENTS.md confermato richiede il piano LAN e non un upload/collaudo.
Queste letture non costituiscono una verifica discovery tra tracker.

Nessun FAIL software finale irrisolto. Warning osservato nella prima build 3C:
fallthrough in OSCMessage.cpp:897, dipendenza OSC già nota; la build incrementale
finale non lo ristampa. Nessun warning Axio irrisolto osservato.

### Memoria e limiti

| Misura | B finale | 3B con password fissa | 3C finale | 3C − 3B | 3C − B |
| --- | ---: | ---: | ---: | ---: | ---: |
| Flash | 1.163.029 B | 1.048.493 B | **1.096.663 B** | **+48.170 B** | −66.366 B |
| RAM statica | 94.116 B | 104.044 B | **111.988 B** | **+7.944 B** | +17.872 B |
| RAM residua statica | 233.564 B | 223.636 B | **215.692 B** | −7.944 B | −17.872 B |

Flash 83% secondo CLI, stessa partizione da 1.310.720 B, margine **214.057 B**.
RAM globale 34% di 327.680 B. Cache e risposta discovery da 4.096 B contribuiscono
alla crescita statica; task, query e risultati mDNS hanno anche costi heap non
misurati dal linker. Non dedurre heap runtime o assenza di leak dai test fake.

Heap 3C libero/minimo **NOT RUN**, senza attribuire alla Fase 3 i 133.084/129.068 B
misurati in B. Anche IMU 99,947 Hz e OSC Mac 99,942 pacchetti/s restano riferimenti
esclusivamente storici. API SDK init/config/free sincrone: nessun tempo massimo
di loop o equivalenza prestazionale dimostrati sulla XIAO.

Rischi aperti: multicast filtrato/isolamento client, allocazioni SDK e latenza,
query mai completata dal driver (stato draining senza query sovrapposte),
conflitti/clonazione MAC, limiti della cache iniziale e interoperabilità reale
dei record. Ready significa servizio configurato, non raggiungibilità verificata.
Piano concreto: [phase3c-lan-plan.md](phase3c-lan-plan.md). Nessuna promessa di
discovery universale, scanner IP alternativo o routing OSC automatico.

### File e Git

Nuovi rispetto alla fine 3B:

- `firmware/Axio/src/connectivity/DiscoveryManager.h`, `DiscoveryRuntime.h`, `DiscoveryRuntime.cpp`.
- `tests/discovery_harness.cpp`, `tests/test_discovery.py`.
- `tests/stubs/runtime/esp_mac.h`, `tests/stubs/runtime/mdns.h`.
- `docs/phase3c-lan-plan.md`.

Aggiornati in 3C: AGENTS.md, README.md, src/README.md, ConnectivityRuntime.cpp,
DiagnosticService.h, OscRuntime.h/.cpp (sola lettura porta corrente aggiunta),
test_connectivity.py, connectivity_runtime_harness.cpp, stub WiFi.h e i documenti
phase3-connectivity/phase3-verification. Nessun cambio al motore schema 1.

Branch `feat/wifi-connectivity`, HEAD ancora
`010b9a91bad5c9153c01aecd5fd260e23e0e358a`; nessun file staged. Le modifiche
3A/3B restano nel working tree insieme al 3C: **12 file tracciati modificati e
28 file nuovi**, senza credenziali reali o catture aggiunte all'indice.
Nessun commit, merge o push. Stop al 3C software; attendere approvazione prima
di 3D o upload.

## Checkpoint 3D — collaudo hardware

Eseguito il 6 novembre 2026 su autorizzazione esplicita dell'utente, inclusi
upload, reboot non distruttivi e prova AP con cambio temporaneo Wi-Fi del Mac.
Firmware finale `3.0.0-checkpoint-3d`, caricato su entrambi i dispositivi.
Stop prima della Fase 4; nessun commit, merge o push.

### Dispositivi, reti e correzione finale

- Primo XIAO `/dev/cu.usbmodem2101`, MAC `ac:27:6e:7e:32:38`, BNO055 collegato,
  hostname `axio-ac276e7e3238`, IP GL finale `192.168.8.159`.
- Secondo XIAO `/dev/cu.usbmodem101`, MAC `ac:27:6e:7c:31:28`, senza BNO055 per
  scelta dell'utente: `init_failed` e nessun campione sono attesi. Hostname
  `axio-ac276e7c3128`, IP GL finale `192.168.8.184`. Verificata la connettività,
  non l'acquisizione IMU di questo dispositivo.
- Rete dedicata GL aggiunta con priorità 255, senza rimuovere TIM dal primo
  tracker (priorità 0). Password STA mai riportate nei report. Il secondo,
  inizialmente vergine, è stato provisionato mediante il vero portale AP.
- Mac ripristinato su TIM, IP `192.168.1.4`, e profilo AP temporaneo rimosso.
  Configurazione OSC originale conservata, nessuna CONFIG APPLY eseguita;
  nessuna tara, DIAG RESET, AP REKEY o cancellazione NVS.

La prova di reboot ha evidenziato un budget di associazione di 5 s troppo corto
in alcune condizioni: il primo tracker passava alla rete TIM pur essendo GL
disponibile. Budget portato a **10 s per tentativo**, fallback AP indipendente
sempre a **15 s**. Aggiunta regressione per associazione riuscita dopo 6,5 s;
ritoccati i test temporali/rollover. Il firmware finale si collega a GL al primo
tentativo su entrambi i tracker. `tools/hardware_test.py` accetta ora
`--wifi-wait-seconds` per attendere tramite STATUS l'avvio STA asincrono;
default 0, nessuna modifica alla configurazione o al dispositivo.

Le prove fisiche AP/provisioning e perdita completa della rete sono precedenti
alla sola correzione del timeout 5→10 s. I moduli AP/HTTP e il timer 15 s sono
invariati; questi scenari completi **non sono stati ripetuti sul binario finale**.
Priorità al boot, discovery, OSC e misure USB sono stati verificati nuovamente
sul firmware finale. SHA256 del binario finale:
`9a0df9f89cd40a4a600991f444b08a636576631c512a4175267664f5e1009cda`.

### Esiti e prove riproducibili

Report locali ignorati da Git in `captures/phase3d-*.json`; riepilogo strutturato
`captures/phase3d-summary-01.json`. Log build, test e upload finali:
`.build/phase3d-fixed-compile.log`, `.build/phase3d-fixed-tests.log`,
`.build/phase3d-fixed-upload-primary.log`, `.build/phase3d-fixed-upload-secondary.log`.
I probe locali sono in `.build/`; non sono strumenti di produzione distribuiti.

| Verifica | Esito | Evidenza e limite |
| --- | --- | --- |
| Compilazione finale e upload su due XIAO | PASS | Hash flash verificati da esptool, nessun erase NVS |
| Regressioni software complete | PASS | 72 test in 9,999 s, 1.345 vettori storici invariati |
| Migrazione e persistenza | PASS | Prima rete SDK migrata, poi sorgente saved; reti, AP, identità e config OSC conservati dopo reboot autorizzati |
| Priorità finale GL al boot | PASS | `phase3d-final-usb-01.json`, tentativi 1; rete TIM conservata |
| AP protetto e provisioning reale | PASS | `phase3d-ap-http-02.json`: chiave errata rifiutata, `12345678` accettata, DHCP e GET pagina/status/networks, POST valido |
| Validazione e client HTTP lento | PASS | Priorità 256 rifiutata HTTP 400 senza mutazioni; client incompleto chiuso dopo 3 s e server recuperato; secondo XIAO senza IMU |
| Perdita STA e fallback | PASS | `phase3d-outage-02.json`: primo tracker passa a TIM; secondo attiva AP/HTTP a circa 15,243 s dalla perdita osservata, timer firmware 15.000 ms |
| Ritorno GL e spegnimento AP | PASS | Secondo torna STA dopo normale riavvio router, AP e HTTP spenti; nessuna riapplicazione config OSC |
| Acquisizione senza STA, invio sospeso | PASS | `phase3d-outage-summary-01.json`: 931 snapshot validi in 9,313888 s, 0 tentativi UDP, suspended_no_sta +931 sul primo |
| AP-only sul secondo | PASS | 403 osservazioni con AP attivo e nessun UDP/listener; assenza sensore limita il significato prestazionale |
| Bonjour Mac sulla stessa LAN | PASS | Browse/resolve `_axio._udp`, SRV 16889, TXT pubblici model/schema/hw e risoluzione IPv4 su TIM |
| Discovery reciproca reale | PASS | `phase3d-final-pair-01.json`: entrambi identificano l'altro, esclusione self; ripartenza dopo reconnect |
| Listener OSC dopo reboot/reconnect | PASS | `phase3d-listener-01.json`: pacchetto volutamente non OSC rifiutato; malformed +1, tare invariata a 0, listener attivo |
| Routing, bundle e ricezione OSC | PASS | Cattura finale correlata: 1.205 datagrammi validi, 2.410 messaggi, 0 invalidi; finestre contatore/ricevitore diverse |
| IMU principale e query periodiche | PASS | 10 s a 99,940 Hz; finestra 40 s a 99,942 Hz, nessun read error |
| Ripristino automatico Wi-Fi Mac | FAIL | Comando macOS dichiarava successo, IP rimasto su GL; recupero manuale confermato dall'utente e verifica indipendente PASS in `phase3d-mac-restored-01.json` |
| IMU del secondo XIAO | NOT RUN | BNO055 non collegato |
| Rate IMU con AP attivo e sensore collegato | NOT RUN | Prove AP sul secondo senza sensore; continuità senza STA misurata sul primo durante failover |
| Tara/ACK fisici, power-cut, endurance e corruzione NVS | NOT RUN | Esclusi dal collaudo non distruttivo |
| Conflitto mDNS radio, reti multicast filtrate, soak prolungato | NOT RUN | Nessuna promessa di discovery universale; GL e TIM separate da NAT |
| Analisi frammentazione e causa minimo heap | NOT RUN | Non misurato largest free block, causa del minimo transitorio non isolata |

La chiusura di HTTP è verificata dal lifecycle e dallo stato dei dispositivi;
non è stata eseguita una certificazione radio né una cattura esaustiva di tutte
le interfacce. Il Mac su TIM non può fare discovery multicast dei due peer GL
attraverso NAT: il PASS reciproco deriva dalle risposte USB dei due XIAO sulla
stessa GL. Un annuncio Bonjour sintetico del Mac era stato usato prima come
probe aggiuntivo; non è contato come secondo tracker reale.

Conservati anche i tentativi intermedi: attesa Wi-Fi iniziale troppo corta nel
probe, primo test AP interrotto prima di mutazioni perché macOS nascondeva
l'SSID, prima finestra outage scaduta senza spegnimento della rete, e fallimenti
di accesso sandbox poi risolti con esecuzione autorizzata. Questi tentativi non
sono stati riclassificati come prove fisiche riuscite.

Comandi di regressione riproducibili, senza upload:

```sh
bash scripts/compile.sh
python3 -B -m unittest discover -s tests -v
git diff --check
```

Per le misure, i probe leggono STATUS/METRICS/CONFIG GET e DISCOVERY GET via USB;
il ricevitore usa `tools/osc_receiver.py --bind 0.0.0.0 --port 16888 --duration 12`.
Ripetere outage, provisioning o reboot richiede il coordinamento della rete di
prova: non riprodurre automaticamente modifiche di credenziali o Wi-Fi del Mac.

### Memoria e prestazioni misurate

| Misura | Checkpoint B finale | 3D finale | Differenza/condizioni |
| --- | ---: | ---: | --- |
| Flash | 1.163.029 B | **1.096.663 B** | −66.366 B, invariata rispetto a 3C |
| RAM statica | 94.116 B | **111.988 B** | +17.872 B, invariata rispetto a 3C |
| RAM residua statica | 233.564 B | **215.692 B** | Non equivale all'heap runtime |
| Heap libero/minimo, finestra finale query | 133.084 / 129.068 B | **112.708 / 107.340 B** | Boot e condizioni diversi da B |
| Heap libero/minimo, ultima lettura finale | — | **113.016 / 105.608 B** | Il minimo cumulativo scende ulteriormente durante le prove successive |
| IMU USB 10,026019 s | 99,947 Hz | **99,939966 Hz** | 1.002 campioni, 0 read error |
| IMU query 40,133101 s | — | **99,942439 Hz** | 4.011 campioni, 0 read error |
| IMU durante cattura correlata 12,028769 s | — | **99,927100 Hz** | 1.202 snapshot, 1.202 UDP queued |
| OSC ricevuto, cattura correlata | 99,942 pacchetti/s | **100,306832 pacchetti/s** | 1.205 datagrammi; GL attraverso NAT, finestre non identiche |
| Jitter ricevitore, deviazione standard | 1,416 ms | **1,912 ms** | p95 12,969 ms, massimo 21,858 ms; rete diversa |

Margine flash 214.057 B sulla partizione da 1.310.720 B; RAM statica 34%.
L'ultima lettura finale è nel riepilogo; la misura query resta una finestra
separata, non il minimo assoluto dell'intera sessione.

Durante un **boot precedente** è stato osservato un minimo heap di **31.772 B**,
già prima dell'outage, poi libero risalito a circa 113 kB. Nessun fallimento di
allocazione/reset/read error osservato, ma causa non isolata: non dichiarare
assenza di leak o frammentazione. Il massimo cumulativo di intervallo
acquisizioni raggiunge circa **77–78 ms** attorno ad avvio/transizioni; media
vicina a 100 Hz non dimostra una latenza massima di 10 ms. Le chiamate SDK
sincrone e la preemption restano limiti da caratterizzare.

Una cattura finale non correlata (`phase3d-final-osc-01.jsonl`) ha registrato
**1.248 datagrammi/12 s, 103,919 Hz**. L'origine dell'eccesso non è dimostrata:
non attribuirlo automaticamente a duplicazioni NAT. La successiva misura
correlata è vicina a 100 Hz ma non spiega formalmente l'anomalia. Schema 1 non
fornisce un sequence number: finestre differenti non provano assenza di perdita
o duplicazione. Una precedente cattura diretta TIM di 1.200 datagrammi aveva
99,937539 Hz e jitter 1,375660 ms; non sostituisce il risultato finale via GL.

### Stato di consegna e decisioni aperte

Modifiche specifiche 3D: budget STA in ConnectivityManager, versione INFO in
DiagnosticService, regressioni connectivity_harness, attesa Wi-Fi opzionale
in hardware_test e relativo test_serial, documentazione e autorizzazione AGENTS.
Tutte le aggiunte 3A/3B/3C restano nel working tree. Branch
`feat/wifi-connectivity`, HEAD `010b9a91bad5c9153c01aecd5fd260e23e0e358a`;
14 file tracciati modificati e 28 nuovi file, indice vuoto. Nessuna cattura o
credenziale STA aggiunta all'indice; baseline, fixture, pin, core e schema OSC
invariati. Nessun commit, merge o push.

Il collaudo è eseguito con i limiti dichiarati, non un PASS incondizionato di
ogni requisito 3D. Restano da decidere ripetizione del ciclo AP/outage sul binario
finale, prova AP con IMU collegata, indagine heap e cattura OSC con finestre
allineate su un'unica LAN. Attendere approvazione dell'utente; Fase 4 esclusa.
