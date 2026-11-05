# Fase 2 — Checkpoint A

5 novembre 2026, branch `feat/osc-engine`. Infrastruttura seriale e strumenti
host implementati. **Checkpoint B in attesa della conferma dell'utente**:
nessun nuovo motore OSC, schema configurabile, persistenza o preset introdotto.
La Fase 2 completa non è ancora dichiarata conclusa. Nessun commit, push,
installazione o upload eseguito nella verifica software iniziale. Successivamente,
con autorizzazione esplicita: pyserial installato nel venv, upload e test USB
PASS; risultati nella sezione finale, che aggiorna gli esiti hardware iniziali.

## Preparazione e architettura

Letti AGENTS.md, README, rapporti delle Fasi 0/1, baseline, benchmark, test e
sorgenti effettivi. Stato Git iniziale pulito sul branch richiesto. Build
iniziale PASS (`.build/phase2-before-compile.log`), 11 test iniziali PASS.

Lo sketch coordina `ImuManager`, `Scheduler` e `LegacyTransport` esistenti.
Il nuovo `DiagnosticService` legge lo stato e chiama la tara comune; il parser
non include Arduino, Wi-Fi o I²C. `UsbDiagnostics` collega il protocollo alla
sola USB. I nuovi moduli sono in `src/diagnostics/`; core e trasporto restano
in `src/core/` e `src/transport/`. Nessuna dipendenza firmware aggiunta.
La matematica, il manager IMU, la reference e la fixture Fase 0 non sono modificati.

Ordine realizzato: verifica iniziale → protocollo e servizio → adattatore USB
e misure → script hardware/upload → test host → documentazione e memoria.

## Protocollo USB v1

115200 baud richiesti dal client (USB CDC), ASCII, LF finale; CRLF accettato.
Niente BOM. JSON usa ASCII e può rappresentare Unicode con escape `\uXXXX`.
Forma canonica, con ID obbligatorio:

```text
AXIO <id> <comando> [<oggetto JSON compatto>]<LF>
```

ID: 1–16 caratteri `[A-Za-z0-9_-]`, unico fra richieste pendenti. Un solo
comando in volo: attendere l'intera risposta prima del successivo. Spazi singoli
fra prefisso, ID e comando; comandi case sensitive. Per esempio, `AXIO PING`
come nome del comando diventa sul filo `AXIO 1 PING`. Una richiesta corretta
produce una sola riga JSON con `axio:1`, `id` uguale a quello ricevuto e `ok`.
Successo: `data` oggetto. Errore: `error` codice stringa. Per input malformato
l'ID viene recuperato quando possibile, altrimenti è `null`.

```text
AXIO 1 PING
{"axio":1,"id":"1","ok":true,"data":{"pong":true}}
AXIO 2 INFO
AXIO 3 STATUS
AXIO 4 METRICS
AXIO 5 CONFIG GET
AXIO 6 CONFIG VALIDATE {"schema":1}
{"axio":1,"id":"6","ok":false,"error":"config_engine_unavailable"}
AXIO 7 CONFIG APPLY {bad}
{"axio":1,"id":"7","ok":false,"error":"invalid_json"}
AXIO 8 TARE
{"axio":1,"id":"8","ok":true,"data":{"tared":true,"persistent":false}}
AXIO 9 DIAG RESET
```

Esempi illustrativi, non catture hardware. TARE e DIAG RESET modificano lo
stato RAM: inviarli solo intenzionalmente. TARE richiama direttamente
`ImuManager::calibrateReference()`, come `/tare` OSC; su errore restituisce
`imu_unavailable` senza alterare i riferimenti. Nessuna tara automatica.

| Comando | Risultato al Checkpoint A |
| --- | --- |
| PING | `pong:true` |
| INFO | firmware/versione, board, protocollo, data/ora build, CDC e disponibilità motore |
| STATUS | stato IMU, validità/tempo/sequenza ultimo snapshot, Wi-Fi, trasporto, heap e uptime |
| METRICS | contatori, finestra, frequenze, massimi temporali |
| TARE | tara effettiva del core o errore IMU |
| CONFIG GET | schema transitorio 0, ID, IP/porta destinazione, ascolto e indirizzo tara; nessuna credenziale |
| CONFIG VALIDATE / APPLY | verifica sintassi oggetto JSON; `config_engine_unavailable` se sintatticamente corretto, nessuna applicazione |
| DIAG RESET | azzera finestra e contatori diagnostici; conserva init_errors, sequenza, stato IMU, riferimenti e minimo heap di boot |

Lo schema attualmente restituito da GET è descrittivo e non applicabile:

```json
{"schema":0,"engine":"transitional","editable":false,"id":0,"destination":{"ip":"0.0.0.0","port":16888},"listen_port":16889,"tare_address":"/tare"}
```

Lo schema 1 del nuovo motore, la validazione semantica integrale, la migrazione,
il ripristino e la persistenza sono da implementare al Checkpoint B. La
validazione sintattica del Checkpoint A non è una validazione configurazione.

Limiti tecnici del protocollo, rivedibili prima dei payload del Checkpoint B:
1024 byte prima di LF (incluso l'eventuale CR), 2048 byte di buffer risposta,
profondità JSON 8. Timeout assoluto 1000 ms dal primo byte, anche con input a
stillicidio; sottrazione unsigned resistente al rollover. Una riga troppo lunga
viene scartata fino a LF e produce `line_too_long`. Dopo `incomplete_timeout`
si scarta fino al prossimo LF: per risincronizzare inviare LF, poi la richiesta.
Righe vuote ignorate. Altri errori: `invalid_encoding`, `invalid_request`,
`unknown_command`, `invalid_json`. Nessuna eco di payload o password.

Buffer statici; parser e servizio senza allocazioni dinamiche per comando.
Budget massimo per poll: 64 byte RX oppure 64 byte TX, una sola risposta
pendente. Durante TX si sospende la lettura RX, non il loop IMU. Host che non
legge: risposta mantenuta, nessuna attesa applicativa; inviare a raffica può
saturare il buffer CDC del core, quindi il client deve rispettare stop-and-wait.
La validazione della riga è sincrona ma limitata alla dimensione/profondità
sopra. TARE esegue le letture I²C sincrone già previste dal core: parser non
bloccante non significa I²C asincrono o garanzia di latenza hardware.

I log applicativi hanno prefisso `# ` e newline; debug WiFiManager disabilitato
per evitare interferenze durante le risposte. Log ROM/core all'avvio non sono
controllabili: il client accetta solo JSONL marcato `axio:1` con ID atteso e
ignora gli altri log. JSON marcato male genera errore, non un falso PASS.
Nessuna esposizione del parser su UDP, TCP o portale.

## USB CDC e upload esplicito

Verificato sul core ESP32 **3.3.12 installato**, non presunto dal nome Serial:

- `boards.txt`, righe 37801–37805: per XIAO ESP32C3 `CDCOnBoot=default`
  significa **Enabled** e `build.cdc_on_boot=1`; `CDCOnBoot=cdc` è Disabled.
- `cores/esp32/HardwareSerial.h`: con CDC e USB mode hardware, `Serial` è
  `HWCDCSerial`, altrimenti è UART0. Il firmware rifiuta la build con CDC spento.
- `HWCDC.h/.cpp`: disponibili `setTxTimeoutMs`, `availableForWrite`, scritture
  parziali. Il firmware usa `setTxTimeoutMs(0)`, controlla spazio e conserva
  l'offset di risposta. Non usa flush o attesa di apertura della porta.

Arduino IDE: aprire `firmware/Axio/Axio.ino`, scheda **XIAO_ESP32C3**, core e
librerie del lock; Tools → USB CDC On Boot → **Enabled**, CPU 160 MHz,
flash 4 MB, partizione Default, debug None, erase flash disattivato. Chiudere
Serial Monitor prima di usare il client Python. Verifica GUI IDE NOT RUN.

```sh
arduino-cli board list --json
arduino-cli board details --fqbn esp32:esp32:XIAO_ESP32C3
bash scripts/compile.sh
# SOLO per caricare intenzionalmente: sostituire con la porta verificata.
bash scripts/upload.sh --port /dev/cu.usbmodem2101
```

`upload.sh` ricompila e invoca separatamente `arduino-cli upload`, FQBN
`esp32:esp32:XIAO_ESP32C3`, stesse board-options di compile.sh, tra cui
`CDCOnBoot=default` e `EraseFlash=none`. Non viene mai richiamato dai test.
Dopo upload può avvenire reset e ricomparsa della porta: ripetere il rilevamento,
verificare il nome e avviare il client. Se necessario usare il reset fisico e
la procedura bootloader della board; nessun reset DTR/RTS applicativo forzato
dallo script (il driver può comunque influire all'apertura).

Nella ricognizione è stata trovata `/dev/cu.usbmodem2101`, VID 303A / PID 1001,
identificata genericamente ESP32 dalla CLI. Non dimostra quale firmware sia
caricato né identifica da sola la XIAO: **nessun upload/apertura/test hardware
eseguito in questa sessione**. Pyserial non è installato nel Python ispezionato.

## Client e comandi riproducibili

I test software usano solo standard library, C++ e librerie Arduino esistenti.
Per test USB reali, predisporre intenzionalmente un venv nel repository:

```sh
python3 -m venv .venv
.venv/bin/python -m pip install -r tools/requirements-hardware.txt
.venv/bin/python -B tools/hardware_test.py --list-ports
.venv/bin/python -B tools/hardware_test.py --port /dev/cu.usbmodem2101 --timeout 2 --boot-timeout 20 --metrics-seconds 10 --report captures/phase2-a-usb.json
# Modifica riferimenti in RAM, solo con il tracker nella posa desiderata:
.venv/bin/python -B tools/hardware_test.py --port /dev/cu.usbmodem2101 --tare --report captures/phase2-a-tare.json
# Reset esplicito dei soli contatori azzerabili:
.venv/bin/python -B tools/hardware_test.py --port /dev/cu.usbmodem2101 --reset-metrics --metrics-seconds 10 --report captures/phase2-a-reset.json
# Wi-Fi richiesto e ricevitore integrato, target Mac già impostato sul tracker:
.venv/bin/python -B tools/hardware_test.py --port /dev/cu.usbmodem2101 --require-wifi --osc-seconds 10 --osc-port 16888 --report captures/phase2-a-osc.json
```

Setup dipendenze documentato, **non eseguito**. Rilevamento porte anche senza
pyserial tramite candidate macOS, senza selezione automatica. I test USB
PING/INFO/STATUS/configurazione letta/contatori/tara non richiedono Wi-Fi;
`--require-wifi` controlla la connessione, `--osc-seconds` richiede ricevitore
raggiungibile e destinazione già configurata. Nessun invio automatico di
configurazioni o modifica persistente. Report creati in modo esclusivo: usare
un nome nuovo, non sovrascrivere catture. Exit 0: test richiesti PASS; 1: FAIL;
2: hardware NOT RUN per porta/dipendenza mancante. `--list-ports` da solo
termina 0 e non costituisce un test hardware.

Il client aspetta PING fino al timeout di avvio, scarta risposte con ID vecchio,
limita le righe ricevute e controlla JSON/esiti. Ogni richiesta ha timeout;
il timeout di avvio può essere superato al massimo dalla richiesta in corso.
Una porta non apribile o un firmware senza protocollo genera FAIL di comunicazione;
le prove non raggiunte sono NOT RUN. Una IMU assente/fallita o contatori che
non avanzano generano FAIL, non un PASS basato sulla sola USB.

## Metriche e benchmark

`window_us` è tempo monotono di boot o dal reset diagnostico. `acquired` conta
letture periodiche complete, `snapshots` le pubblicazioni (coincidenti oggi),
`acquisition_attempts` include i fallimenti. Non misura il rate interno della
fusione BNO055. Frequenze firmware: conteggio / durata finestra; il client
misura delta conteggio / delta `window_us`, evitando di attribuire al firmware
il tempo di attesa USB. Reset concorrenti rilevati da contatori/tempo decrescenti.

`max_read_us`: transazioni I²C; `max_processing_us`: durata acquisizione totale
meno durata letture, inclusi matematica, copie, clock e bookkeeping;
`max_send_us`: serializzazione più tentativo UDP. `osc_attempts` non prova che
il datagramma sia stato accettato o consegnato. `osc_requested_hz=100` indica
il periodo nominale transitorio, non una frequenza garantita. `udp_sent` e
`host_received` sono `null`: il trasporto attuale non verifica endPacket e il
firmware non conosce il numero ricevuto sul Mac. Al Checkpoint B saranno
aggiunti contatori distinti dell'esito UDP e scheduler configurabile.
STATUS espone heap libero e minimo osservato dall'ESP dal boot, non azzerato
con DIAG RESET. Max intervallo fra tentativi disponibile; jitter statistico
IMU richiede ulteriori serie/aggregazioni al Checkpoint B, non viene dedotto
dal jitter di ricezione.

Il ricevitore integrato nel test hardware usa l'analizzatore esistente e
riporta conteggi ricevuti, frequenze pacchetti OSC, intervalli e jitter per
sorgente. Non verifica da solo l'identità del mittente: usare una porta LAN
dedicata. Per conservare anche i datagrammi grezzi usare i tool esistenti:

```sh
python3 -B tools/osc_receiver.py --bind 0.0.0.0 --port 16888 --duration 60 --output captures/phase2-a-01.jsonl
python3 -B tools/analyze_capture.py captures/phase2-a-01.jsonl
```

Non eseguire due ricevitori sulla stessa porta. Nessuna diagnostica periodica
è attiva: durante benchmark interrompere polling seriale intensivo, misurare
METRICS prima/dopo e usare il ricevitore separato. L'overhead inattivo non è
stato misurato su XIAO. Matrice 1/2/4/8 destinazioni, 4/8/16 messaggi e rate fino
a 100 Hz, limiti provvisori e benchmark reali: **rinviati al Checkpoint B**.
Nessun limite definitivo OSC o prestazione hardware dichiarato.

## Verifiche software e memoria

```sh
bash scripts/compile.sh > .build/phase2-a-compile-final.log 2>&1
python3 -B -m unittest discover -s tests -v > .build/phase2-a-tests-final.log 2>&1
bash -n scripts/compile.sh scripts/upload.sh
git diff --check
```

| Verifica | Esito | Evidenza / limite |
| --- | --- | --- |
| Build FQBN richiesto e toolchain bloccata | PASS | log build finale, nessun upload |
| 11 test storici | PASS | inclusi tutti i 1.345 vettori, hash reference e fixture byte per byte |
| 7 nuovi test host seriale/script | PASS | protocollo/servizio reali C++, client Python e runner con seriale fake |
| JSONL, correlazione, comandi, reset | PASS | risposte decodificate con json.loads; config transitoria e indisponibilità esplicita |
| Timeout, rollover, input lungo, sintassi malformata | PASS | recupero al newline, errori strutturati |
| Backpressure, budget e scritture parziali | PASS | porta fake senza spazio: nessun consumo RX o blocco del poll |
| Tara seriale e fallimento | PASS | core reale con device fake, quaternion/yaw aggiornati insieme; invariati su errore |
| Script: opzioni mutanti e fallimenti Wi-Fi | PASS | fake seriale, nessun hardware implicato |
| OSC e UDP localhost preesistenti | PASS | suite finale autorizzata fuori sandbox |
| Sintassi shell e whitespace | PASS | comandi sopra |
| USB reale, reboot, stall/disconnessione host | NOT RUN | nuovo firmware non caricato, pyserial assente |
| Tara fisica e IMU scollegata | NOT RUN | nessuna misura nuova; test host distinti |
| OSC reale, rate IMU, jitter e heap runtime | NOT RUN | nessun benchmark eseguito |
| Motore/configurazione/persistenza/ACK OSC nuovi | NOT RUN | Checkpoint B non iniziato |

Una prima esecuzione finale in sandbox ha dato FAIL solo sul bind UDP localhost
(`Operation not permitted`); ripetuta con permesso, PASS. Warning noti in
WiFiManager (`%d` con uint32_t) e OSC (fallthrough) rimangono esterni al progetto;
le build con cache possono non ristamparli. Nessun warning nuovo Axio osservato.

Stesse opzioni e partizione, build reale sulla macchina:

| Versione | Flash programma | RAM globale | RAM residua statica |
| --- | ---: | ---: | ---: |
| Fase 1 ricompilata prima delle modifiche | 1.126.869 B | 39.660 B | 288.020 B |
| Checkpoint A | 1.133.051 B | 42.796 B | 284.884 B |
| Differenza | +6.182 B | +3.136 B | −3.136 B |

Flash 86% di 1.310.720 B, RAM globale 13% di 327.680 B. La RAM statica non
include picchi stack/heap di portale, UDP o servizio: da misurare su XIAO.
Margine flash da monitorare prima del nuovo motore, senza cambiare partizione
arbitrariamente. Non è un benchmark di CPU o latenza.

## File e attività restanti

Creati: `src/diagnostics/{SerialProtocol,DiagnosticService,UsbDiagnostics}.h`
sotto `firmware/Axio/`, `tools/hardware_test.py`,
`tools/requirements-hardware.txt`, `scripts/upload.sh`,
`tests/serial_harness.cpp`, `tests/test_serial.py`, questo rapporto.
Modificati: `firmware/Axio/Axio.ino`, `src/core/Diagnostics.h`,
`src/core/BnoDevice.h` (solo log), `src/transport/LegacyTransport.{h,cpp}`
sotto lo sketch, README e indice architettura `firmware/Axio/src/README.md`.

Da verificare sulla XIAO: upload esplicito, enumerazione CDC dopo reboot,
PING a rete assente, stato IMU e avanzamento contatori, tara in posa nota,
errori con sensore assente senza riferimenti alterati, host che smette di
leggere/disconnette, contatori e heap prima/dopo uso prolungato, ricezione OSC
reale e impatto diagnostica. I rischi storici del portale transitorio, incluso
AP aperto e validazione limitata dei suoi campi, restano documentati in Fase 1.
Nessuna nuova dashboard o funzione delle fasi successive.

Report strutturato dei test hardware non eseguiti:
`.build/phase2-a-hardware-not-run.json`, generato senza selezionare una porta
con `python3 -B tools/hardware_test.py --report .build/phase2-a-hardware-not-run.json`.
Exit 2 previsto; tutte le prove hardware NOT RUN. Questo file non è una cattura
del dispositivo.

## Tentativo hardware autorizzato successivo

L'utente ha confermato che `/dev/cu.usbmodem2101` è la XIAO e autorizzato
upload e test USB senza tara, reset dei contatori o modifiche configurazione.

- PASS: compilazione tramite `bash scripts/compile.sh`; flash 1.133.051 B,
  RAM globale 42.796 B, invariate. Log `.build/checkpoint-a-hardware-compile.log`.
- PASS: `.venv` creato e pyserial 3.5 installato. Primo tentativo pip bloccato
  dalla rete del sandbox; installazione riuscita con accesso autorizzato.
- FAIL: singolo tentativo `bash scripts/upload.sh --port /dev/cu.usbmodem2101`:
  esptool non apre la porta, `Errno 16 Resource busy`. Nessuna scrittura flash
  iniziata. Log `.build/checkpoint-a-upload.log`.
- Causa verificata con lsof e ps: Arduino
  `packages/builtin/tools/serial-monitor/0.15.0/serial-monitor`, PID 65366,
  mantiene aperta la porta. Processo non terminato dall'agente.
- NOT RUN: test seriali del nuovo firmware, stato BNO055 e frequenza IMU;
  upload non completato. Tara, reset e modifiche configurazione non eseguiti.

Report dell'esecuzione interrotta, conservato prima della ripresa:
`captures/checkpoint-a-usb-01-upload-blocked.json`.
Descrive il fallimento dell'upload e le prove NOT RUN; non è il risultato del
runner seriale, che non è stato avviato. Chiudere Serial Monitor prima di
riprendere l'upload già autorizzato; per il report successivo conservare questa
evidenza con un nome distinto, senza sovrascriverla. Nessun Checkpoint B avviato.

## Ripresa hardware dopo chiusura Serial Monitor — PASS

L'utente ha chiuso Serial Monitor e chiesto di riprendere. `lsof` non rilevava
più processi sulla porta; stesso dispositivo confermato, seriale
`AC:27:6E:7E:32:38`. Nessun intervento fisico necessario.

Comandi eseguiti:

```sh
bash scripts/upload.sh --port /dev/cu.usbmodem2101 > .build/checkpoint-a-upload-retry.log 2>&1
arduino-cli board list --json
.venv/bin/python -B tools/hardware_test.py --list-ports
.venv/bin/python -B tools/hardware_test.py --port /dev/cu.usbmodem2101 --timeout 2 --boot-timeout 20 --metrics-seconds 10 --report captures/checkpoint-a-usb-01.json > .build/checkpoint-a-usb-01.log 2>&1
```

Upload PASS, exit 0: ricompilazione con flash 1.133.051 B e RAM globale
42.796 B; esptool ha scritto il firmware, verificato l'hash e riavviato via RTS.
La porta dopo il riavvio è rimasta `/dev/cu.usbmodem2101`. Accesso USB eseguito
fuori sandbox con autorizzazione. Nessun ulteriore upload necessario.

Runner hardware PASS, exit 0. Report reale:
`captures/checkpoint-a-usb-01.json`; log `.build/checkpoint-a-usb-01.log`.
Il precedente report di upload bloccato è stato rinominato prima della prova,
senza perderne il contenuto. Catture ignorate da Git.

| Verifica | Esito | Evidenza reale |
| --- | --- | --- |
| PING, INFO, STATUS | PASS | firmware Axio 2.0.0-checkpoint-a, protocollo 1, CDC attivo |
| Stato IMU | PASS | BNO055 ready, sample_valid true; nessun errore init/lettura |
| CONFIG GET | PASS | schema 0 transitorio letto, nessuna credenziale esposta |
| Contatori e acquisizioni | PASS | 1.004 acquisizioni/snapshot in 10,062724 s |
| Tara e reset metriche | NOT RUN | esclusi intenzionalmente; tare_requests=0 |
| Test Wi-Fi dedicato e ricezione OSC Mac | NOT RUN | STATUS indica connessione Wi-Fi, ma non verifica rete o consegna |
| Persistenza del nuovo motore | NOT RUN | Checkpoint B non iniziato |

Misura fra due METRICS usando il tempo monotono del firmware:

- Acquisizioni complete: **99,774176 Hz**; da 1.781 a 2.785.
- Snapshot prodotti: **99,774176 Hz**, stessa differenza 1.004.
- Tentativi OSC: **99,774176 Hz**, stessa differenza 1.004.
- Errori lettura nella finestra: **0**; init_errors=0 e tare_errors=0.
- Heap allo STATUS: libero **193.260 B**, minimo dal boot **187.768 B**.
- Massimi cumulativi a fine prova: letture **8.026 µs**, elaborazione e
  bookkeeping **641 µs**, invio tentato **2.159 µs**, intervallo fra tentativi
  **15.000 µs**. Non sono massimi isolati alla sola finestra di 10 secondi.

Nessun errore seriale segnalato dal runner. Il software non conta separatamente
eventuali retry PING iniziali, quindi non si afferma che ogni singola richiesta
sia riuscita al primo tentativo. Il solo errore precedente verificato era la
porta occupata durante l'upload, risolto chiudendo Serial Monitor.

Le frequenze cumulative mostrate nei singoli METRICS includono l'avvio e sono
più basse (75,49 e 82,75 Hz): la misura di questa prova è il delta di 1.004
acquisizioni diviso 10,062724 s. Non misura il rate interno di fusione BNO055,
la consegna UDP o il jitter statistico IMU. `udp_sent` e `host_received` restano
null. Non sono stati eseguiti benchmark estesi o prove disconnessione/stall USB.

Rispetto ai 18 test software già PASS, questa prova aggiunge evidenza reale
di upload, enumerazione CDC, protocollo sul dispositivo, sensore leggibile e
contatori in avanzamento. Non sostituisce i test matematici e non verifica la
tara fisica o il futuro motore OSC. Nessuna tara, reset dei contatori o comando
di modifica persistente alla configurazione inviato. Nessun commit o push;
Checkpoint B ancora in attesa.
