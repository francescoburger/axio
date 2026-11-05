# Fase 2 — verifiche Checkpoint A e B

**6 novembre 2026: Checkpoint B approvato dall'utente e Fase 2 dichiarata
completata per le funzionalità previste.** Controllo finale prima del commit:
build PASS (flash 1.163.029 B, RAM globale 94.116 B), 28 regressioni PASS con
1.345 vettori matematici invariati, controlli whitespace PASS. Log locali:
`.build/phase2-final-commit-compile.log` e `.build/phase2-final-commit-tests.log`.
Indice verificato comprensivo delle tre correzioni hardware; esclusi catture,
log, ambiente virtuale e artefatti. Commit locale autorizzato con messaggio
`feat: complete phase 2 configurable OSC engine and hardware validation`.
Nessun nuovo upload o collaudo hardware in questa chiusura; restano NOT RUN
tara/ACK fisico, power-cut/corruzione NVS reale, frammentazione/blocco massimo
allocabile, benchmark di capacità/endurance e verifica GUI Arduino IDE.
Nessun push o avvio della Fase 3.

Stato attuale: Checkpoint A approvato e committato; Checkpoint B verificato su
software e XIAO con correzioni mirate, configurazione originale ripristinata.
IMU 99,947 Hz, ricezione Mac 99,942 pacchetti/s nella prova finale. Benchmark di
capacità, tara fisica e power-cut NVS NOT RUN. Vedere le sezioni finali e
[lo schema OSC](phase2-osc.md). Le sezioni iniziali documentano il Checkpoint A.

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

## Checkpoint A approvato e commit locale

L'utente ha approvato upload/USB/BNO055 e misura 99,774 Hz. Prima del commit:
compilazione PASS (`.build/checkpoint-a-commit-compile.log`), tutti i 18 test
PASS (`.build/checkpoint-a-commit-tests.log`), `git diff --check` e controllo
indice PASS. Tutti i 16 file necessari inclusi; nessun file sorgente A rimasto
non tracciato. `.build/`, `.venv/` e `captures/` confermati ignorati e non inclusi.

Commit locale: **`30cd5e147d04b30691ae78711e5b743d312e44b0`**.
Messaggio: **`feat: complete phase 2 checkpoint A USB diagnostics and hardware tests`**.
Working tree pulito subito dopo il commit; nessun push. L'autorizzazione seguente
ha avviato il Checkpoint B, descritto sotto. Le sezioni precedenti restano come
cronologia delle evidenze del Checkpoint A, non stato attuale del motore OSC.

## Checkpoint B — verifica software conclusa

Implementati motore configurabile e catalogo campi, più destinazioni, argomenti
ordinati, rate per messaggio, bundle, token `{id}`, preset Orientation/Motion/
Custom modificabili, listener/tara/ACK configurabili. Configurazione schema 1
validata integralmente, preparata e salvata su journal NVS a due slot con CRC e
selettore atomico, poi attivata senza reboot; recupero della precedente o default.
Nessuna dashboard, Fase 3, nuova dipendenza firmware, installazione o upload B.
Nessun ulteriore commit automatico del Checkpoint B; nessun push.

Schema completo, catalogo con tipi e intervalli, limiti provvisori, migrazione,
strategie di ripristino e procedura XIAO in [phase2-osc.md](phase2-osc.md).
L'infrastruttura USB resta attiva, stesso protocollo v1 e ID; buffer portati a
8.192 B input e 12.288 B risposta per accettare configurazioni complete.
Core matematico, manager IMU, reference e fixture byte per byte invariati.

Comandi effettivamente eseguiti:

```sh
bash scripts/compile.sh > .build/phase2-b-compile-final.log 2>&1
python3 -B -m unittest discover -s tests -v > .build/phase2-b-tests.log 2>&1
bash -n scripts/compile.sh scripts/upload.sh
git diff --check
python3 -B tools/osc_benchmark.py --ip 192.168.1.4 --output .build/phase2-b-benchmark-plan.json
python3 -B tools/osc_config.py --preset Orientation --ip 192.168.1.4 --id 3 > .build/phase2-b-orientation.json
```

| Verifica | Esito | Evidenza / limite |
| --- | --- | --- |
| Build esp32:esp32:XIAO_ESP32C3 | PASS | Toolchain e partizione invariate, nessun upload |
| Suite completa | PASS | 28 test: 18 precedenti e 10 nuovi, 1.345 vettori invariati |
| Protocollo USB conservato | PASS | Comandi, timeout/rollover, JSONL, errori, backpressure e buffer aggiornati |
| Codec OSC, campi e routing multiplo | PASS | Byte produzione decodificati su Python, float fuori nominale preservati |
| Bundle immediati e ordine argomenti | PASS | Pacchetti reali del codec, percorso bundle e messaggio singolo |
| OSC localhost | PASS | Pacchetti produzione trasmessi/ricevuti su socket Mac loopback |
| Preset e schema completo | PASS | Roundtrip JSON canonico; rifiuto duplicati, unknown, route/porta/rate/campi invalidi |
| Limiti tecnici e dimensione pacchetti | PASS | 36 scenari validati su host; config troppo grandi o bundle oltre limite rifiutati |
| Scheduler e frequenze | PASS | Clock sintetico, 25 Hz, snapshot duplicato scartato, nessun catch-up dopo pausa |
| Persistenza e ripristino | PASS | Journal reale con memoria fake: reboot, CRC corrotto, commit/scrittura falliti, slot non committato ignorato |
| Applicazione atomica | PASS | Transaction reale e runtime: listener/NVS falliti mantengono configurazione; GET dopo reboot fake |
| Runtime seriale B | PASS | OscRuntime.cpp compilato con adattatori host; VALIDATE/APPLY/GET, INFO/METRICS e 8 destinazioni/16 messaggi |
| Tara OSC e ACK | PASS | Callback comune, formato esatto ,i=1, nessun ACK se fallisce, padding rigoroso |
| Media/deviazione intervalli IMU | PASS | Welford produzione con intervalli noti, nessuna modifica alle formule IMU |
| Strumenti Python/shell e whitespace | PASS | Sintassi e git diff --check |
| Benchmark reali 1/2/4/8 e 4/8/16 | NOT RUN | Piano generato con 36 scenari, tutti NOT RUN; nessuna simulazione come misura |
| Upload/test USB del firmware B | NOT RUN | Preparati, non eseguiti |
| OSC XIAO, ACK fisico, routing LAN | NOT RUN | Tool e comandi pronti, richiedono upload B e configurazione esplicita |
| NVS/power-cut reali, heap e overhead B | NOT RUN | Solo fault injection host, da misurare su scheda |

Il primo test UDP B nel sandbox è stato bloccato (`Operation not permitted`);
rieseguito fuori sandbox con autorizzazione: PASS. Un primo warning locale di
API UDP flush deprecata è stato corretto usando clear prima della build finale.
Restano warning noti delle dipendenze OSC e WiFiManager già documentati; le
build con cache non li ristampano sempre. Nessun warning Axio irrisolto osservato.

| Versione | Flash | RAM globale | RAM residua statica |
| --- | ---: | ---: | ---: |
| Fase 1 | 1.126.869 B | 39.660 B | 288.020 B |
| Checkpoint A committato | 1.133.051 B | 42.796 B | 284.884 B |
| Checkpoint B | 1.163.007 B | 94.116 B | 233.564 B |
| B − A | +29.956 B | +51.320 B | −51.320 B |
| B − Fase 1 | +36.138 B | +54.456 B | −54.456 B |

Flash 88% di 1.310.720 B, RAM globale 28% di 327.680 B. Buffer fissi di comando,
risposta, journal e configurazione spiegano gran parte dell'incremento RAM.
Workspace diagnostico persistente evita grandi buffer sullo stack del loop.
Questi dati non misurano heap runtime, stack massimo, CPU o sostenibilità 100 Hz.
Il margine flash resta nella stessa partizione; non è stata cambiata per far
entrare il firmware. I 99,774 Hz hardware del Checkpoint A non si trasferiscono
al Checkpoint B senza nuova misura.

Nuovi file B:

- `firmware/Axio/src/osc/Config.h`, `Codec.h`, `Engine.h`, `Journal.h`, `Transaction.h`.
- `firmware/Axio/src/transport/OscRuntime.h`, `OscRuntime.cpp`.
- `tools/osc_config.py`, `osc_benchmark.py`, `osc_hardware_test.py`.
- `tests/osc_engine_harness.cpp`, `osc_runtime_harness.cpp`, `test_osc_engine.py`.
- `tests/stubs/runtime/{Arduino,WiFi,WiFiUdp,Preferences,esp_timer}.h`.
- `docs/phase2-osc.md`.

Modificati B: AGENTS.md (autorizzazione attuale), README, indice src, Axio.ino,
Diagnostics.h (statistiche intervalli), DiagnosticService.h, SerialProtocol.h,
LegacyTransport.cpp (listener e campi OSC portale affidati al motore),
hardware_test.py e test_serial.py (limiti aggiornati), questo rapporto.

Rischi da verificare: NVS piena e power-cut fisico, latenza delle API sincrone
NVS/I²C/UDP, risorse durante cambio listener, riconnessione rete/riavvio,
combinazioni intensive CPU/heap e jitter durante polling seriale. Nessuna
sostenibilità prestazionale o limite definitivo dichiarato. In condizioni normali
il parser non attende input/TX; APPLY e TARE possono prolungare un giro del loop
con le operazioni sincrone necessarie e sono esclusi dalle finestre benchmark.

## Collaudo hardware Checkpoint B autorizzato — esito finale PASS

La XIAO su `/dev/cu.usbmodem2101` era già confermata dall'utente. Nessuna nuova
richiesta di identità, nessun commit o push, nessuna Fase 3. Le modifiche già
nell'indice sono state ispezionate e salvate come patch locale in
`.build/checkpoint-b-index-before-hardware.patch`; confronto byte identico con
la patch dell'indice prima di integrare le correzioni: PASS. Nessun reset Git
né perdita delle modifiche precedenti.

Compilazione/upload iniziali B PASS. Dopo due correzioni firmware motivate dai
risultati reali sono state completate build, regressioni e upload correttivi.
Terza e ultima build caricata: **flash 1.163.029 B, RAM globale 94.116 B**;
esptool ha verificato l'hash e riavviato. Gli upload non hanno cancellato NVS.
Nessun caricamento ripetuto per tentativi senza una causa identificata.

### Risultati conclusivi

| Prova | Esito | Evidenza reale / limite |
| --- | --- | --- |
| Build/upload finale XIAO ESP32C3 | PASS | `.build/checkpoint-b-scheduler-fix-compile.log`, `...-upload.log` |
| Regressioni software dopo correzioni | PASS | 28 test, 1.345 vettori invariati, `.build/checkpoint-b-scheduler-fix-tests.log` |
| USB PING/INFO/STATUS/CONFIG GET/METRICS | PASS | `captures/checkpoint-b-validated-usb.json`, exit 0 |
| BNO055 / motore | PASS | ready, sample_valid true, configurable, listener attivo, Wi-Fi connesso |
| Misura IMU >=10 s | PASS | 1.005 acquisizioni in 10,055290 s = **99,947391 Hz** |
| OSC originale dopo correzione 100 Hz | PASS | 1.200 datagrammi, 2.400 messaggi validi sul Mac in cattura da 12 s |
| Routing moderato multi-destinazione | PASS | 2 endpoint Mac, 4 messaggi a 25/50 Hz, bundle e messaggio singolo |
| Persistenza configurazione moderata | PASS | riavvio reale, uptime ridotto, GET identico e config_source saved; ricezione dopo reboot PASS |
| Ripristino originale e persistenza | PASS | JSON identico prima/dopo reboot e nell'ultima lettura dopo upload finale |
| ACK di tara su host/mock | PASS | formato ,i=1 e assenza ACK se callback fallisce, harness produzione nelle 28 regressioni |
| Tara fisica / ACK reale | NOT RUN | esplicitamente esclusi; nessun comando TARE o OSC /tare inviato |
| DIAG RESET | NOT RUN | mai inviato; i riavvii e APPLY hanno i reset interni già documentati |
| Frammentazione / massimo blocco allocabile | NOT RUN | non esposti nel protocollo seriale attuale; non dedotti da heap libero/minimo |
| Corruzione/power-cut NVS fisico | NOT RUN | ripristino da reboot valido reale PASS; fault injection solo host |
| Capacità massima / endurance | NOT RUN | carico moderato, nessuna conclusione sui limiti definitivi |

Report riassuntivo reale: `captures/checkpoint-b-hardware-summary.json` (PASS).
Stato/GET/METRICS finali: `captures/checkpoint-b-final-state.json`.
Tutti i file captures sono locali, ignorati da Git; nessuna credenziale salvata.
I NOT RUN delle prove dedicate OSC/persistenza nel solo hardware_test.py non
significano che queste prove non siano state effettuate: sono nei report OSC
e di persistenza separati indicati qui.

### Frequenze e ricezione

Confronto col riferimento A **99,774 Hz**, senza dedurre equivalenza o miglioramento
statistico da finestre e condizioni diverse:

| Condizione reale | Finestra firmware | Acquisizioni IMU/s |
| --- | ---: | ---: |
| Primo firmware B, prima correzioni | 10,055997 s | 99,940364 |
| B con RX corretta | 10,045221 s | 99,948025 |
| Carico moderato, prima reboot | 12,080436 s | 99,499720 |
| Carico moderato, dopo reboot | 12,061092 s | 99,493479 |
| Firmware finale, originale ripristinato | 10,055290 s | **99,947391** |

La differenza finale da A è **+0,173391 Hz (+0,173784%)**. Nessun errore init,
lettura o tara nei contatori finali; tare_requests=0. Acquisizioni complete e
snapshot coincidono, non sono misure del rate interno di fusione BNO055.

Nella finestra finale firmware: **1.005 tentativi UDP, 1.005 accettati localmente**,
rate di scheduling messaggi **99,947619 Hz**. Questo non è un ACK di consegna.
Dall'avvio, 3.315 tentativi e 3.299 accettati alla seconda lettura METRICS: 16
non accettati prima della finestra stabile, durante l'avvio. Non nascosti né
confusi con perdita di datagrammi sulla LAN.

Cattura indipendente Mac `captures/checkpoint-b-validated-osc.jsonl`:

- XIAO **192.168.1.15:16889** → Mac **192.168.1.4:16888**.
- **1.200 pacchetti OSC validi, 2.400 messaggi, 0 malformati**.
- Ricezione **99,942397 pacchetti/s**, intervallo medio **10,005764 ms**,
  p95 **12,565959 ms**, massimo **17,941 ms**, jitter stddev popolazione
  **1,416034 ms**. Jitter di ricezione Mac, non jitter IMU.
- `/orientation/3` con tag `,fff`; `/quaternion/3` con `,ffff`, bundle immediato 1.
  Valori finiti verificati, senza clamp o rigetto fuori nominale.

Un esempio reale della cattura prima delle correzioni aveva orientation
`[0.005555556, 0.011111111, -0.055555556]` e quaternion
`[0.995544434, -0.092407227, -0.015014648, -0.009216309]`.
I valori seguono le formule storiche, non costituiscono una verifica metrologica
con pose note. Nessuna tara per produrre questi valori.

Finestre USB di circa 10 s e cattura Mac da 12 s diverse: i conteggi non
consentono di dichiarare zero perdite. Anche i ricevitori integrati aprono socket
prima di CONFIG GET/METRICS e possono avere pacchetti iniziali in coda: frequenze
per porta e conteggi delle prove integrate hanno questo limite ai bordi. La
cattura finale indipendente evita quel setup seriale nel ricevitore.

### Carico moderato e persistenza

Configurazione originale salvata prima delle prove in
`captures/checkpoint-b-original-config.json`: Orientation, ID 3,
192.168.1.4:16888, listener 16889, /tare, ACK disabilitato. La prova temporanea
`captures/checkpoint-b-moderate-config.json` ha usato listener **16989** e due
endpoint sullo stesso Mac (**16900**, **16901**), non due host fisici distinti:

| Messaggio | Campi ordinati | Hz | Route | Bundle |
| --- | --- | ---: | --- | --- |
| /test/orientation/3 | yaw,pitch,roll | 25 | entrambe | sì |
| /test/quaternion/3 | q0,q1,q2,q3 | 25 | entrambe | sì |
| /test/motion/3 | axWorld,ayWorld,azWorld | 50 | 16900 | no |
| /test/gyro/3 | gz,gx,gy | 25 | 16901 | sì |

Nella prova moderata finale: rate firmware **24,999081 / 49,998162 Hz**;
1.208 UDP tentati e accettati nella finestra METRICS. Sul Mac, 907 datagrammi
sulla 16900 e 302 sulla 16901; 302 orientation/quaternion su ciascuna route,
605 motion sulla 16900 e 302 gyro sulla 16901. Zero pacchetti malformati.
Il jitter aggregato della 16900 mescola due cadenze e non misura da solo la rete.

Dopo reboot: 906 datagrammi sulla 16900 e 302 sulla 16901, zero malformati;
rate scheduling circa **24,956196 / 49,995303 Hz**. Queste prove a 25/50 Hz sono
state eseguite sul firmware con RX corretta, prima della successiva correzione
del solo ramo a 100 Hz; i rami a 25/50 Hz non sono cambiati, regressioni PASS.

Report principali:

- `captures/checkpoint-b-retest-osc-moderate.json`.
- `captures/checkpoint-b-retest-osc-after-reboot.json`.
- `captures/checkpoint-b-persistence-and-restore-03.json`: 16 verifiche PASS,
  restored=true. Uptime moderato **163.469.068 → 5.823.427 µs**; originale
  ripristinato **18.310.737 → 5.874.193 µs**.
- `captures/checkpoint-b-retest-osc-restored.json`: ricezione originale dopo reboot.

Riavvii con `esptool --port /dev/cu.usbmodem2101 --after hard-reset run`:
caricamento stub solo in RAM e reset, nessuna riscrittura firmware/NVS. Porta
invariata; nessun intervento fisico necessario. Alla fine, GET è identico
all'originale anche dopo l'ultimo upload. L'originale era inizialmente generato
come default; il ripristino ne ha salvato gli stessi valori nel journal
(`config_source:saved`). Non è stata cancellata la configurazione Wi-Fi.

### Memoria reale e cause RAM statica

STATUS finale: heap libero **133.084 B**, minimo dal boot **129.068 B**.
Nella misura USB finale, heap libero 133.356 B e stesso minimo; nei passaggi
precedenti è stato osservato un minimo di 126.828 B in un altro boot, da non
mescolare con il minimo del boot finale. Nessuna misura di frammentazione o
blocco massimo: il firmware non espone questi dati via USB, e il solo heap
libero non basta per calcolarli. Nessuna prova di assenza di leak su lunga durata.

| Build | Flash | RAM globale |
| --- | ---: | ---: |
| A | 1.133.051 B | 42.796 B |
| B prima collaudo | 1.163.007 B | 94.116 B |
| B finale corretto | **1.163.029 B** | **94.116 B** |
| Finale − A | +29.978 B | +51.320 B |

Analisi mediante `riscv32-esp-elf-nm -S --size-sort -C` sull'ELF reale,
conservata in `.build/checkpoint-b-final-symbols.txt`. Principali incrementi:

| Componente | Incremento RAM statica rispetto ad A |
| --- | ---: |
| Parser/riposta USB: input 1.024→8.192 e output 2.048→12.288 | 17.408 B |
| Workspace persistente DiagnosticService | 5.000 B |
| Journal (buffer blob + JSON) | 14.368 B |
| JSON canonico configurazione | 7.169 B |
| Engine (config attiva, packet buffer, scheduler, metriche) | 3.864 B |
| Configurazione candidata | 1.876 B |
| Buffer ricezione UDP | 1.400 B |
| Altri stati/network/allineamenti, residuo | 235 B |
| Totale | **51.320 B** |

La correzione RX alloca **8.193 byte di coda** tramite HWCDC al posto dei 256
predefiniti, una volta prima di Serial.begin. È heap limitato, non ulteriore
RAM statica; il costo osservato va quindi distinto dai 51.320 B sopra. Il primo
B senza questa correzione mostrava heap libero 141.908 B/minimo 135.568 B.
Workspace e buffer statici hanno costi noti, ma non sono limiti definitivi:
ottimizzazioni e margini dinamici richiedono i benchmark di capacità successivi.

Max tempi cumulativi nel METRICS finale della finestra USB: letture **7.973 µs**,
elaborazione/bookkeeping **726 µs**, invio dispatch **1.681 µs**, intervallo
acquisizioni **12.004 µs**; stddev intervalli tentativi IMU **92,494 µs**.
Durante APPLY moderato max_config_us **8.772 µs** e massimo intervallo cumulativo
**19.000 µs**: NVS e cambio socket sono sincroni, fuori dalla finestra stabile.
Questi massimi includono la strumentazione e non sono tempi puri del bus.

### Errori risolti e regressioni

1. **FAIL iniziale — richiesta CONFIG troncata**: payload moderato 678 B più
   framing, coda RX HWCDC predefinita 256 B; ISR del core scarta byte se piena.
   Esito `incomplete_timeout`, prima di APPLY. Correzione limitata: configurare
   RX a MaxLine+1 prima di Serial.begin. Test burst massimo su host usando
   UsbDiagnostics di produzione e porta fake, build e 28 regressioni PASS;
   validazione/APPLY reali del payload moderato poi PASS.
2. **FAIL controllo reboot — risposta di sessione precedente**: client nuovi
   ripartivano da ID 1; PING tardivo scambiabile con STATUS. Config originale
   ripristinata dal finally, ma test uptime fallito. Correzione client: prefisso
   casuale di sessione 8 hex + contatore 8 hex; nessun retry automatico di APPLY.
   Test risposta vecchia ignorata, ID distinti fra client e collaudo reale PASS.
3. **Underrate OSC 100 Hz**: alcune finestre ~94,8–95,7 pacchetti/s pur con IMU
   ~99,95 Hz, con meno eventi scheduled già nel firmware. Non attribuito a
   perdita Wi-Fi. A 100 Hz il doppio gating sui timestamp di completamento
   scartava snapshot per jitter; corretto per seguire ogni snapshot nuovo del
   core a periodo 10 ms. Sequenze duplicate ancora scartate; nessun catch-up,
   nessuna modifica al core o alla matematica. Nuovo vettore di jitter nel
   harness e verifica reale finale ~99,947 Hz scheduling / ~99,942 Hz Mac PASS.

Conservati i report FAIL delle prove precedenti
`checkpoint-b-persistence-and-restore.json` e `...-02.json`; il report
conclusivo è `...-03.json` più il summary, senza cancellare evidenze.
Log delle tre regressioni in `.build/checkpoint-b-hardware-fix-tests.log`,
`checkpoint-b-client-fix-tests.log`, `checkpoint-b-scheduler-fix-tests.log`.
Nessun warning nuovo Axio irrisolto; warning storici delle dipendenze già noti.

Modifiche correttive: Axio.ino, UsbDiagnostics.h, Engine.h, hardware_test.py e
relativi harness/test. Matematica, reference e fixture immutate. README e schema
operativo aggiornati con risultati e semantica 100 Hz. Le modifiche B iniziali
sono conservate insieme alle correzioni; nessun nuovo commit, nessun push.
