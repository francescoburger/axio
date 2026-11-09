# Correzioni 001–005 — 8 novembre 2026

Implementazione delle richieste in `debug_requests/README_Codex.md`, con due
precisazioni dell'utente: solo la barra viola deve estendersi a tutta finestra;
il badge superiore va eliminato da tutte le cinque schermate. Dopo la validazione
software, l'utente ha autorizzato l'upload sulla XIAO principale: risultati nella
sezione hardware sotto. Nessun comando di calibrazione, DIAG RESET, modifica
delle reti, restart web o sleep è stato inviato all'hardware; il riavvio al
termine dell'upload è quello ordinario dell'uploader.

## Modifiche

| Richiesta | Implementazione | Verifica software |
|---|---|---|
| 001 | Header fuori da `.shell`, sfondo a tutta finestra e contenuto interno allineato alla navigazione. Titolo adattabile e batteria senza sovrapposizioni. CSS della navigazione e larghezze del contenuto conservati. | PASS |
| 002 | Rimossi rendering e spazio del badge superiore in Overview, Motion Data, OSC, Network e Diagnostics. Stato della telemetria conservato come testo nella card Sensor Stream; stati Wi-Fi/IMU/OSC nelle card conservati. | PASS |
| 003 | Intestazione Message 01 con bordo uniforme, angoli arrotondati e token Axio; Message Preview usa `.subcard`, sfondo bianco e contenuto monospaziato. Nessuna barra laterale. Anteprima aggiornata anche al cambio dei chip. | PASS |
| 004 | Terminologia Calibration nelle UI, API, strumenti e simboli di produzione; parser compatibile con le vecchie chiavi e indirizzi persistenti conservati. | PASS |
| 005 | Stesso DashboardService e stessi asset gzip su AP e STA. Provisioning nella scheda Network, usando le API e il journal reti esistenti. | PASS su host e STA reale; AP reale NOT RUN |

## Contratto Calibration e persistenza

- Chiave canonica JSON: `calibration_address`, negli schemi OSC 1 e 2.
  `tare_address` rimane accettata in lettura. Specificare entrambe le chiavi,
  anche con lo stesso valore, è un errore; valgono gli stessi controlli dei
  duplicati per le due grafie. CONFIG GET e snapshot producono la chiave nuova.
- Default per nuove configurazioni: `/calibration`; ACK `/calibrated/{id}`.
  Gli indirizzi salvati, compresi `/tare`, `/tare/{id}`, `/tared/{id}` e quelli
  personalizzati, non vengono riscritti. L'ACK resta quello configurato,
  con tipo `,i` e valore 1, e viene emesso soltanto dopo il successo.
- I default `/calibration` e `/tare` sono alias reciproci; lo stesso vale per
  le rispettive forme con `/{id}`. Gli alias non si applicano a indirizzi
  personalizzati e vengono disabilitati se coincidono con un messaggio in
  uscita o con l'ACK. Non si modifica così la validità delle vecchie configurazioni.
- Comando canonico USB/web: `CALIBRATION`; `TARE` è ancora accettato. La risposta
  USB al comando precedente conserva `tared:true`; quella nuova usa
  `calibrated:true`. Le metriche usano `calibration_requests` e
  `calibration_errors`. Non sono mantenuti doppi nomi nei contatori in uscita.
- Gli strumenti usano `--calibrate`, con `--tare` come alias della stessa opzione
  esplicita. Configurazioni/catture precedenti sono normalizzate solo nei nomi
  durante il confronto; valori e documenti originali restano conservati.
- Namespace NVS `axio-osc-v1`, formato del record, CRC, generazioni e commit a
  due slot restano invariati. La lettura di record con la vecchia chiave non
  richiede scritture; al successivo salvataggio esplicito si serializza la
  chiave canonica. Il test usa record binari contenenti davvero `tare_address`,
  non soltanto JSON nuovo ripassato al parser; verifica anche fallimento del
  commit e successivo riavvio simulato.
- La retrocompatibilità riguarda la lettura da parte della nuova revisione.
  Un firmware precedente che accetta soltanto `tare_address` non può leggere
  i record risalvati con la chiave nuova: per un eventuale downgrade serve
  una conversione esplicita della chiave prima del caricamento del vecchio firmware.
- Non sono stati individuati progetti client Max/Pd/TouchDesigner nel repository;
  eventuali client esterni non sono stati verificati. Gli alias non vengono rimossi.

Unico ingresso al core ancora `ImuManager::calibrateReference()`. Nessuna
calibrazione automatica, nessuna modifica delle operazioni matematiche o dello
scheduler. `Calibration.h` cambia soltanto un commento; ImuManager cambia nomi
dei contatori e commenti. Baseline, fixture, vettori storici e rapporti delle
prove passate non vengono rinominati retroattivamente. I residui `tare` nel
codice operativo sono limitati agli adattatori di compatibilità.

## AP/STA

Il vecchio ProvisioningHttp non è più istanziato dal runtime AP; rimane nel
repository per i test storici del componente. Un solo listener non bloccante,
con gli stessi budget RX/TX e timeout, serve la dashboard sull'indirizzo AP
quando l'AP è attivo e sull'indirizzo STA quando l'AP è chiuso. Nessun bind ANY
e nessuna duplicazione di frontend o buffer HTTP.

Host/Origin e token sono verificati anche in AP. L'hostname mDNS è ammesso su
STA; in AP si usa l'IP. Frontend e asset restano relativi all'host corrente.
Lo snapshot espone `ap_active`, l'IP usato dalla dashboard e, senza STA, valori
assenti per gateway/subnet/RSSI. L'Overview distingue l'accesso AP dalla STA
disconnessa. La schermata Network spiega chiusura AP e riconnessione.

WPA2, password predefinita `12345678`, credenziali già salvate e politica di
fallback sono conservati. Dopo connessione STA l'AP viene chiuso; la pagina AP
può quindi perdere connessione, e si riapre la dashboard dall'IP/hostname STA.
Nel breve stato AP+STA il listener resta sull'AP fino alla sua chiusura.
OSC resta sospeso finché l'AP è attivo, anche se il tentativo di spegnerlo fallisce.

Il test con errore iniettato nello spegnimento AP ha rilevato che la chiusura
HTTP ripetuta a ogni tick interrompeva le risposte. Corretto il lifecycle:
il server rimane operativo fra i tentativi; viene chiuso quando la radio AP
si spegne realmente o quando si riconfigura intenzionalmente il dispositivo.

## Verifiche

| Verifica | Esito | Evidenza e limite |
|---|---|---|
| Suite host completa | PASS | 89 test; `.build/debug-corrections-tests-final.log` |
| Matematica baseline/core | PASS | 1.345 vettori; output host identico, hash baseline e fixture byte per byte |
| Configurazioni precedenti | PASS | Schemi 1/2, API web/USB, chiavi ambigue rifiutate, endpoint e ACK personalizzati preservati, record NVS precedente e fault injection |
| OSC e calibrazione su host | PASS | Byte, routing, scheduling, ACK canonico/personalizzato, alias e conflitti; nessuna misura hardware |
| HTTP AP/STA su host | PASS | Stessi asset e cinque route, sessione/sample/batteria/power, controlli Host/Origin/token, chiusura AP fallita e recupero |
| Provisioning AP su host | PASS | Aggiunta, riordino e cancellazione; Validate senza mutazioni, password preservate, NVS fallita, revisione obsoleta, passaggio a STA |
| Browser responsive | PASS | Cinque pagine a 320/430/768/1024/1440 CSS px, DPR 2; header a tutta finestra, navigazione invariata, nessun overflow osservato |
| Browser con runtime C++ | PASS | Cinque pagine in STA e AP simulati, matrice dei controlli; socket TCP reali su loopback, radio/NVS/IMU simulati |
| Sintassi JS e asset embedded | PASS | Probe browser, `node --check`, packaging deterministico e build |
| Build ESP32 | PASS | `.build/debug-corrections-build-final.log`; 1.143.133 / 1.310.720 B flash, margine 167.587 B |
| RAM statica | PASS | 110.980 B; 216.700 B residui statici, non una misura di heap operativo |
| Whitespace e sorgenti protetti | PASS | `git diff --check`; nessuna modifica a baseline, fixture o vettori storici |
| Upload e collaudo USB/STA di questa revisione sulla XIAO | PASS | Principale AC:27:6E:7E:32:38; dettagli nella sezione hardware |
| Conferma visiva utente | NOT RUN | Catture disponibili; nessuna accettazione dedotta dai test |

I browser probe controllano anche l'allineamento dell'interno dell'header alla
navigazione, l'assenza del badge superiore, il contenuto iniziale e dinamico
dell'anteprima OSC, i bordi senza stripe e i controlli batteria/quaternion/ACK.
Screenshot e report sono sotto `captures/phase4-ui-{320,430,768,1024,1440}/`,
`captures/phase4-runtime-ui-430/` e `captures/phase4-runtime-ui-ap-430/`.
I dati visualizzati in queste catture sono dati di test, non misure del dispositivo.

La build incrementale finale non ha emesso warning; il warning esterno storico
della libreria OSC non è una correzione inclusa in questa revisione.
Ricezione OSC su host e contatori simulati non misurano il rate IMU fisico.
Restano NOT RUN: provisioning via radio reale su `192.168.4.1`,
calibrazione/ACK fisici, ricezione OSC sulla LAN con la nuova build e stabilità
prolungata sotto carico reale. Configurazioni dopo upload e una breve finestra
di carico HTTP sono ora verificate sotto. Non sono riutilizzati come risultati
della nuova revisione i collaudi hardware delle fasi precedenti.

## Upload e verifiche hardware autorizzati

XIAO principale identificata prima dell'upload tramite seriale USB
`AC:27:6E:7E:32:38` e hostname `axio-ac276e7e3238`. Porta attuale
`/dev/cu.usbmodem101`; è lo stesso dispositivo precedentemente visto su altre
porte, non un nuovo tracker. Nessun secondo XIAO aggiornato in questa sessione.

Eseguito `bash scripts/upload.sh --port /dev/cu.usbmodem101`: build PASS,
upload PASS, verifica hash dei dati scritti PASS, riavvio ordinario completato.
Opzione `EraseFlash=none`, nessuna cancellazione NVS. Log:
`.build/debug-corrections-upload-01.log`. Dimensioni della build installata:
1.143.133 B flash, 110.980 B RAM statica. Identità verificata anche dall'uploader.

SHA-256 dell'applicazione caricata:
`422b7dc5553d3326810089308e637fbe40d633af57e4f4a60597ef8b793d9a58`.
Build esposta da INFO: `Nov 8 2026 16:39:07`.

| Prova reale | Esito | Evidenza/limite |
|---|---|---|
| Avvio e identità | PASS | Stessa XIAO, hostname individuale conservato |
| IMU | PASS | Stato ready dopo upload; nessuna calibrazione richiesta |
| Reti salvate e riconnessione | PASS | WIFI GET prima/dopo identico, riconnessa alla STA con IP `10.166.156.41`; nessuna password richiesta o modificata |
| Configurazione OSC completa | PASS | Confronto prima/dopo identico normalizzando soltanto `tare_address` in `calibration_address`; indirizzo precedente e ACK conservati |
| Dashboard/API HTTP su STA | PASS | Root, CSS, JS, sessione, snapshot e batteria HTTP 200 |
| Browser su dispositivo reale | PASS | Cinque pagine a 430 e 1440 CSS px/DPR 2; header a tutta finestra, assenza del badge superiore, anteprima e nuova terminologia |
| Telemetria HTTP | PASS | 100 campioni validi su 100 richieste nella finestra di circa 10 s; nessun errore HTTP, latenza massima osservata 0,174 s |
| Errori IMU nelle finestre | PASS | Delta read_errors 0 sia a riposo sia durante telemetria |
| Errori invio UDP locali | PASS con limite | Delta errori 0; non prova di ricezione sul destinatario |
| AP/provisioning reale | NOT RUN | Nessuna disconnessione forzata o modifica delle reti |
| Calibrazione/ACK fisici | NOT RUN | Nessun comando inviato; contatore calibration_requests 0 dopo avvio |

Frequenza IMU misurata dai contatori USB: **99,892581 Hz** a riposo
(503 acquisizioni / 5,035409 s), **99,497949 Hz** con telemetria
(999 acquisizioni / 10,040408 s). Non è una certificazione di equivalenza
temporale, jitter o stabilità prolungata. UDP accettato localmente dal driver
nella finestra web: circa 95,4145 datagrammi/s, zero errori; non misura il rate
IMU e non dimostra consegna sulla LAN.

Batteria letta: 4,034 V, percentuale assente. Snapshot dopo la finestra:
106.744 B heap libero, minimo cumulativo 90.748 B, blocco libero massimo
90.100 B. Sono osservazioni di questa finestra, non una prova di assenza di leak.

Catture non tracciate: `captures/debug-corrections-before-01.json`,
`debug-corrections-after-01.json`, `debug-corrections-hardware-01.json`.
Screenshot/report delle schermate realmente servite dal tracker:
`captures/phase4-device-ui-430/` e `captures/phase4-device-ui-1440/`.
Le catture simulate delle sezioni precedenti restano distinte da queste.

L'accesso USB iniziale è stato bloccato dal sandbox e poi autorizzato;
non è stato un errore del firmware. Nessun commit, merge o push.

## Esecuzioni intermedie

- FAIL iniziale della suite: aspettativa di padding rimasta a 3 byte dopo
  l'allungamento dell'indirizzo OSC; corretta a 4 byte.
- FAIL della nuova prova AP+STA con stop radio fallito: individuata e corretta
  la chiusura HTTP ripetuta descritta sopra; successivo test mirato PASS.
- FAIL della suite con log dentro il sandbox: socket locali bloccati con
  `PermissionError`. Rilancio esplicitamente autorizzato fuori sandbox: 89/89 PASS.
  Log dell'esecuzione bloccata: `.build/debug-corrections-tests.log`.
- In revisione CSS eliminata una regola duplicata che limitava a 460 px
  l'interno dell'header desktop; aggiunto controllo browser dell'allineamento.

## File interessati

- Documentazione: `README.md`, questo rapporto.
- Frontend: `firmware/Axio/web/index.html`, `style.css`, `app.js`;
  `firmware/Axio/src/web/WebAssets.h` rigenerato dai sorgenti.
- HTTP: `src/web/DashboardRuntime.cpp`, `DashboardService.h`;
  `src/connectivity/ApRuntime.cpp`, `ApRuntime.h`.
- Calibrazione/diagnostica: `firmware/Axio/Axio.ino`,
  `src/core/Calibration.h` (solo commento), `Diagnostics.h`, `ImuManager.h`,
  `src/diagnostics/DiagnosticService.h`.
- OSC/trasporto: `src/osc/Config.h`, `Codec.h`, `Engine.h`,
  `src/transport/OscRuntime.cpp`, `OscRuntime.h`, `LegacyTransport.cpp`,
  `LegacyTransport.h`.
- Strumenti: `tools/osc_config.py`, `osc_hardware_test.py`, `hardware_test.py`,
  `dashboard_hardware_probe.py`; `scripts/phase4_browser_probe.mjs`.
- Test: `tests/test_dashboard_runtime.py`, `test_osc_engine.py`, `test_serial.py`,
  `dashboard_runtime_harness.cpp`, `dashboard_service_harness.cpp`,
  `osc_engine_harness.cpp`, `osc_runtime_harness.cpp`, `serial_harness.cpp`,
  `connectivity_runtime_harness.cpp`, `stubs/runtime/WiFi.h`.

I percorsi `src/` in questo elenco sono relativi a `firmware/Axio/`.
`.gitignore` e `debug_requests/` erano già modificati/non tracciati e sono
stati lasciati intatti. Nessuna dipendenza aggiunta, commit, merge o push.
