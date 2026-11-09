# Fase 4 — consegna e verifiche

**Stato:** dashboard e supporto firmware integrati, installati sulle due XIAO e
collaudati nell'ambito descritto sotto. **La Fase 4 non è dichiarata completa:**
restano l'accettazione finale e i limiti di collaudo documentati.
Il nome comune `axio.local`, inizialmente disabilitato dopo la prova di conflitto,
è ora integrato e verificato sulle due XIAO: aggiornamento del 9 novembre in
[phase4-entry-point-verification.md](phase4-entry-point-verification.md).
Le sezioni tecniche seguenti descrivono la consegna iniziale; le correzioni UI,
OSC e AP successive sono documentate nei rapporti debug richiamati dal README.
Nessun commit, merge o push. Nessuna modifica ai riferimenti `axio_codex_ui/`,
alla baseline o alla fixture congelata.

Aggiornamento 7 novembre: corrette la richiesta sleep da pulsante e la
visualizzazione dei volt nell'header, approvate dall'utente. Dettagli e limiti
in [phase4-button-battery-fix.md](phase4-button-battery-fix.md). Questa revisione
è caricata sul principale; il secondario conserva la precedente build Fase 4.

## Decisioni applicate

- Un unico editor OSC, senza Add/Remove Message, preset, tab o toggle bundle.
  I cinque gruppi generano automaticamente messaggi aggregati per indirizzo.
- La compatibilità delle vecchie configurazioni OSC non è un requisito. Al
  primo avvio Fase 4 una configurazione precedente viene convertita all'editor
  unico; ID persistente, destinazioni, porta, tare e ACK sono conservati. Il
  contenuto dei messaggi passa al nuovo editor. Il lettore schema 1 rimane
  internamente per regressioni e strumenti USB, senza controlli legacy nella UI.
- ID iniziale: MAC completo in esadecimale minuscolo, 12 caratteri, quando non
  esiste una configurazione salvata. L'ID salvato del principale è rimasto `3`.
  Il secondario aveva soltanto il default non persistente `0`: ora ha l'ID
  automatico persistente `ac276e7c3128`. Non è una perdita di ID personalizzato.
- Pulsante: GPIO5, attivo LOW verso GND con pull-up. Dopo l'approvazione del
  7 novembre, pressione di almeno 3 s e rilascio richiedono sleep; pressione
  breve risveglia. Il pulsante premuto al boot/wake deve prima essere rilasciato.
- Batteria: **pad A0/D0 = GPIO2**, chiarito dall'utente dopo l'ambiguità iniziale
  con GPIO0. Formula fornita: `2 * somma_di_16_letture_mV / 16 / 1000.0`;
  ordine della divisione intera conservato. Nessuna curva SoC: percentuale `null`
  nell'API. Dal 7 novembre l'header mostra i volt misurati, senza percentuale,
  barre di carica o stato di ricarica inventati. «—» resta per misura assente/errore.

## Modifiche firmware

`Axio.ino` mantiene scheduler IMU 10 ms, letture complete e trasporto da snapshot.
HTTP e ADC vengono serviti dopo le attività principali, con margine rispetto
alla prossima acquisizione. La sola sequenza di alimentazione sospende
intenzionalmente acquisizione e comunicazioni. Nessun nuovo retry IMU.

Il server HTTP è legato al solo indirizzo STA, con socket non bloccante e
buffer fissi: 256 byte RX e 512 byte TX per tick, header massimo 1024 byte,
body compatto massimo 3072 byte, timeout assoluto 5 s. Asset HTML/CSS/JS gzip
in flash, senza CDN, font remoti, framework web o librerie 3D. L'header generato
è nel progetto anche per Arduino IDE; `scripts/package_web.py` lo aggiorna.

Le API controllano Host, Origin e token casuale di boot per le mutazioni.
Il token è una protezione delle richieste browser, non autenticazione dell'utente:
la dashboard rimane senza autenticazione sulla LAN come richiesto. Non viene
esposto un ponte generico verso tutti i comandi USB o verso i segreti AP.

| API | Operazione |
|---|---|
| GET `/api/session` | Token per richieste browser |
| GET `/api/snapshot` | Identità, stato, OSC, reti senza password, peer e diagnostica |
| GET `/api/sample` | Campione completo richiesto dal client; copia condivisa aggiornata al massimo a 10 Hz |
| GET `/api/battery` | Tensione e percentuale non disponibile |
| GET `/api/power` | Stato della sequenza, errore e causa di risveglio |
| POST `/api/identity` | Salvataggio del solo Tracker ID globale |
| POST `/api/osc/validate`, `/api/osc/apply` | Validazione separata da persistenza/attivazione |
| POST `/api/network/validate`, `/api/network/apply` | Reti, credenziali conservate e AP password |
| POST `/api/command` | Solo TARE e DIAG RESET; non eseguiti fisicamente dai probe |
| POST `/api/control` | Restart/Sleep con conferma esplicita e ACK prima della transizione |

Schema OSC 2 con editor persistente: Main Address, cinque suffissi, cinque
maschere di selezione e `interval_ms` intero 10–65.535. Ordine dei campi:
Orientation, Real Acceleration, World Acceleration, Gyroscope, Quaternion.
Un solo messaggio dovuto produce OSC semplice; più messaggi dovuti producono
un bundle, uguale per tutte le destinazioni. Limiti indirizzi, collisioni,
float32, 16 argomenti, 1400 byte e validazione transazionale preservati.

Apply rete salva prima nel journal AXW2, che include anche l'eventuale password
AP; legge i vecchi AXW1. Attivazione dopo risposta HTTP, con breve differimento.
Se il browser scompare dopo il commit, il dispositivo converge comunque sulla
configurazione salvata: il browser deve trattare l'esito come incerto e usare
hostname/IP o fallback AP. Revisioni impediscono di riutilizzare indici password
stale. Password omessa significa invariata; stringa vuota significa rete STA
aperta. Rinomina e riordino conservano il segreto tramite indice originale.

SSID AP `Axio-<Tracker ID>`, hostname individuale hardware invariato. `_axio._udp`
annuncia anche il Tracker ID; `_http._tcp` annuncia la dashboard. Peer letti
asincronamente e accesso individuale per IP, senza routing OSC automatico.
Il portale AP di provisioning rimane separato; nessun OSC viene trasmesso in AP.

Deep Sleep: attesa del rilascio stabile del pulsante, configurazione wake GPIO5,
CONFIG BNO055, attesa, scrittura SUSPEND e readback, arresto ordinato OSC/mDNS/
HTTP/Wi-Fi, ingresso sleep. Errori e timeout impediscono l'ingresso; tentativo di
ripristino del modo sensore originale. Al wake il boot normale reinizializza
sensore e rete, senza tara automatica o persistenza inventata del riferimento.

## Modifiche web

Cinque pagine inglesi, in ordine e con card conformi al prototipo. Overview è
l'unico punto di modifica ID. Motion Data ha 12 barre, numeri non saturati,
Tare e checkbox unica quaternion/canvas 3D. Il rendering 3D è nel browser.
La telemetria è polling solo sulla pagina Motion visibile; Pause/chiusura
interrompono le richieste. Target 10 Hz, fallback client a 5 Hz dopo errori.

OSC ha preview immediatamente sotto Main Address, chip individuali, slash
fisso per suffisso, intervallo dopo i gruppi, ACK condizionale e import/export
solo in Advanced. Import carica una bozza validata e non effettua Apply.
Network ha massimo otto reti, ordine prioritario, password non precaricate,
controlli eye e conferma di possibile disconnessione. Diagnostics è on demand,
con Detailed Metrics e Reset confermato; non contiene Tare o alimentazione.
Checkbox, bordi, palette, spaziature e tipografia derivano dal provino. Gli
adattamenti funzionali mancanti nei PNG (eye, campi ACK, feedback, canvas reale)
non sono nuovi pannelli o controlli estranei alla specifica.

## Build, memoria e correzioni

Toolchain invariata: CLI 1.5.1, ESP32 3.3.12, IDF 5.5.5, mDNS 1.13.1,
BNO055 1.6.4, BusIO 1.17.4, Unified Sensor 1.1.15, OSC 4.0.0.
Nessuna installazione o modifica del framework esterno.

| Verifica software | Esito | Evidenza |
|---|---|---|
| Suite completa | PASS | 83 test, incluse regressioni OSC/JSON/NVS, HTTP reale su host, sequenza power con fault injection e pulsante senza rete/browser |
| Matematica e sorgenti congelati | PASS | 1.345 vettori baseline/core; controlli hash/byte e diff protetti |
| Ultima build firmware | PASS | 1.148.331 B su partizione 1.310.720 B; margine 162.389 B |
| RAM statica | PASS | 116.732 B; 210.948 B residui statici, non equivalenti all'heap operativo |
| Sintassi JS e asset gzip | PASS | node --check, generazione deterministica e compilazione |
| Audit stack compiler | PASS con limite | parse OSC 2496→112 B, compilatore editor 2768→896 B, setup OSC 2480→80 B; non è misura del high-water mark hardware |
| Whitespace e percorsi congelati | PASS | git diff --check e diff baseline/fixture |

L'audit stack ha eliminato copie annidate di Config potenzialmente superiori
agli 8 KB del task Arduino. L'output editor rimane invariato se la validazione
fallisce. La dimensione immagine è letta dai soli header una volta all'avvio:
non si richiama `ESP.getSketchSize()` nel loop, perché nel core installato
verifica l'immagine completa.

La prima sonda a 10 Hz ha osservato 62 risposte 429 su 200. Corretto tramite
copia condivisa su richiesta, senza invii spontanei. Il minimo heap di 6–8 KB
persisteva anche dopo la correzione della lettura flash: non era stato spiegato
interamente da essa. Una build diagnostica temporanea, poi rimossa, ha tracciato
allocazioni da `esf_buf_alloc_dynamic` e `mem_malloc` (driver Wi-Fi/lwIP), con
minimo osservato 7880 B. Non si tratta di una deduzione dal solo contatore SDK.

Sono stati recuperati circa 11 KB rimuovendo il buffer JSON OSC duplicato e
riducendo MaxReply USB da 12288 a 8192, mantenendo tutti i payload validi massimi
(MaxConfig 7168 più envelope). Journal CRC/readback e limiti configurazione
invariati. Non sono stati alterati i parametri interni del driver Wi-Fi.

Warning esterno noto: OSCMessage.cpp:897, implicit fallthrough. Nessun warning
Axio irrisolto osservato nell'ultima build. I warning del probe diagnostico
isolato sono stati corretti prima della prova finale mDNS.

## Prove sui dispositivi reali

Principale AC:27:6E:7E:32:38, BNO055 presente, `192.168.8.159` sulla LAN GL.
Secondario AC:27:6E:7C:31:28, `192.168.8.184`, senza BNO055: `init_failed` è
atteso, non un risultato PASS del sensore. Il secondo XIAO è stato successivamente rinumerato da usbmodem101 a
usbmodem1101; non è un cambio di identità. Le porte USB possono rinumerarsi;
identità verificata tramite MAC. Entrambi hanno la build Fase 4, senza probe
sperimentali lasciati attivi. Hash dell'artefatto installato in
`captures/phase4-final-upload-03.json`.

| Prova fisica | Esito | Evidenza/limite |
|---|---|---|
| Upload e avvio sulle due XIAO | PASS | Nessuna cancellazione NVS, rete e dashboard ripartite |
| Reti e hostname individuale conservati | PASS | Confronto USB prima/dopo, incluse prove power; credenziali mai pubblicate |
| ID, destinazioni, Listen/Tare/ACK principale | PASS | Confronto con copia precedente; secondario genera ID dal default non salvato |
| HTTP asset e API in lettura | PASS | Root, CSS, JS, sessione, snapshot, batteria |
| Browser contro XIAO reale | PASS funzionale | Cinque pagine e stati locali, viewport 430×860 CSS px/DPR 2 |
| Telemetria web | PASS funzionale | 200 richieste valide/200 nell'ultima finestra di circa 20 s |
| IMU sotto telemetria | Misurata | 99,560551 Hz, zero errori di lettura; non equivalenza temporale |
| IMU nella finestra precedente | Misurata | 99,714550 Hz in circa 5 s |
| UDP accettato localmente nella finestra web | PASS con limite | 50,004851 datagrammi/s, delta errori 0; non prova di consegna |
| Tensione su A0/GPIO2 | PASS lettura | Circa 4,048 V; accuratezza assoluta e percentuale non certificate |
| Restart dalla API | PASS | ACK, disconnessione, ritorno IMU/STA, OSC e reti identici, wake cause 0 |
| Deep Sleep e pulsante | PASS nel percorso provato | ACK, disconnessione, pressione manuale, wake cause GPIO=7, IMU/STA ripartiti e configurazioni identiche |
| Discovery reciproca con ID | PASS | Entrambe le dashboard vedono l'altro dispositivo e il relativo IP |
| Nome comune esclusivo | FAIL della scorciatoia delegata | Due record A contemporanei quando entrambe le schede pubblicano il nome; funzione non integrata |

La sonda finale è `tools/dashboard_hardware_probe.py`, rapporto
`captures/phase4-hardware-06.json`. Heap dopo la finestra: **100.780 B** liberi,
minimo cumulativo **20.428 B**, blocco massimo libero **90.100 B**. Intervallo
IMU massimo cumulativo **77.812 µs**, invariato fra gli snapshot della prova.
La misura di carico e le prove power/OSC precedono soltanto gli ultimi ritocchi
dei file web (indicatore Live e titolo preview); il codice C++ di controllo
è lo stesso. Gli asset finali sono verificati sul dispositivo con nuove catture.
Le finestre precedenti hanno misurato circa 99,47–99,56 Hz sotto carico web;
non si seleziona una finestra per promettere 100 Hz esatti o assenza di jitter.
Il margine heap migliorato non certifica stabilità infinita o assenza di leak.

### OSC ricevuto sulla stessa LAN

Destinazione temporanea verso il Mac `192.168.8.164:16888`, ripristino esatto
verificato via CONFIG GET nel finally. Nessuna tara. Validazione prima di Apply,
con controllo che Validate non cambi la configurazione. Finestra di circa 5 s
per caso; conteggi non sono una garanzia generale di zero perdite/duplicazioni.

| Intervallo | Forma verificata | Datagrammi ricevuti | Messaggi decodificati | Esito |
|---|---|---:|---:|---|
| 20 ms | Un solo indirizzo, OSC semplice a 16 float | 252 | 252 | PASS |
| 11 ms | Cinque indirizzi, bundle automatico | 454 | 2270 | PASS |
| 13 ms | Cinque indirizzi, bundle automatico | 388 | 1940 | PASS |
| 30 ms | Cinque indirizzi, bundle automatico | 168 | 840 | PASS |

Zero errori di formato, delta errori UDP 0 e delta errori IMU 0 nei quattro
casi. Frequenza IMU misurata separatamente dai contatori USB: circa 99,79–99,90
Hz. Ricevere OSC non misura il rate IMU. Gli intervalli ms non promettono
scadenze prive di jitter: l'invio usa soltanto snapshot completi freschi.
Evidenza: `captures/phase4-osc-lan-02.json`, `restored:true`.

## Confronti visivi

`captures/phase4-device-ui/comparison.html`: PNG di riferimento a sinistra,
interfaccia servita dalla XIAO a destra. Cinque screenshot full-page, stessi
430 CSS px e DPR 2. Nessun overflow orizzontale osservato. Aggiunte catture
editing ID, quaternion OFF, Paused, ACK ON, otto destinazioni, zero/otto reti,
Detailed Metrics aperto. I dialoghi Restart/Sleep del probe browser sono stati
annullati: la prova power effettiva è separata e autorizzata.

`captures/phase4-ui/` contiene inoltre la matrice con dati di test, inclusi i
livelli batteria normale/basso/critico/N/D. Quei valori non sono dati del
tracker e non vengono usati come fallback nella UI operativa. Nessun valore
fittizio di batteria o peer è codificato nel frontend operativo.

Non è dichiarata una corrispondenza pixel-perfect: non esiste una tolleranza
numerica approvata. Sono presenti differenze funzionali documentate (valori
reali, stato vuoto peer, preview con campi, eye, ACK, canvas reale, N/D). Mancano
l'accettazione utente, l'intera matrice di errori/import e una verifica estesa
di tutte le varianti desktop/mobile. I riferimenti non sono stati modificati.

## Limiti e lavoro aperto

- **Aggiornamento 9 novembre 2026 — dashboard AP: PASS manuale utente.**
  L'utente riferisce di aver provato la dashboard su AP e che sembra funzionare
  bene. La verifica vale per l'uso della dashboard osservato dall'utente;
  provisioning, salvataggio reti e passaggio AP→STA non sono stati specificati.
  Le diciture AP reale NOT RUN nei rapporti precedenti descrivono le prove
  dell'agente in quelle sessioni e restano evidenze storiche.
- `axio.local`: elezione, probing nativo e subentro integrati il 9 novembre,
  verificati sui due dispositivi senza un secondo responder o server centrale.
  Restano prove con tre XIAO, partizioni e perdita pacchetti controllata; dettagli
  in [phase4-entry-point-verification.md](phase4-entry-point-verification.md).
- NOT RUN: endurance prolungata, potenza assorbita in sleep, accuratezza ADC con
  strumento esterno e curva SoC, stress radio/multi-client esteso, power-cut/NVS
  fisico, tara/ACK fisici, perdita di pacchetti e zero duplicazioni garantiti.
- Apply rete e password AP: validazione, transazioni e fault injection PASS su
  host; modifica di credenziali/AP reali non forzata per non alterare le reti
  dell'utente. Fallback radio storico 3D non ricertificato integralmente in Fase 4.
- La prova wake dimostra il percorso GPIO e il ripristino funzionale; SUSPEND è
  verificato dal firmware mediante readback prima di proseguire, non tramite
  misura indipendente dei consumi.
- FAIL intermedi conservati: timeout HTTP quando Mac e tracker erano su LAN
  diverse; rate limit 429; heap minimo basso; sonda Sleep con collisione di nome
  prima dell'invio comando; sonda OSC interrotta nello svuotamento continuo con
  ripristino avvenuto. Correzioni e nuove prove non cancellano tali evidenze.

Nessun processo di prova o ricevitore deve rimanere attivo alla consegna.
Le catture e i temporanei sono ignorati da Git. Nessun commit o push.

## File modificati o aggiunti

- `README.md`
- `docs/phase4-implementation.md`
- `docs/phase4-mdns-decision.md`
- `docs/phase4-verification.md`
- `docs/phase4-button-battery-fix.md`
- `firmware/Axio/Axio.ino`
- `firmware/Axio/hardware_config.h`
- `firmware/Axio/src/README.md`
- `firmware/Axio/src/connectivity/ApRuntime.cpp`
- `firmware/Axio/src/connectivity/ApRuntime.h`
- `firmware/Axio/src/connectivity/ConnectivityManager.h`
- `firmware/Axio/src/connectivity/ConnectivityRuntime.cpp`
- `firmware/Axio/src/connectivity/ConnectivityRuntime.h`
- `firmware/Axio/src/connectivity/DiscoveryManager.h`
- `firmware/Axio/src/connectivity/DiscoveryRuntime.cpp`
- `firmware/Axio/src/connectivity/DiscoveryRuntime.h`
- `firmware/Axio/src/core/BnoDevice.h`
- `firmware/Axio/src/core/Scheduler.h`
- `firmware/Axio/src/diagnostics/DiagnosticService.h`
- `firmware/Axio/src/diagnostics/SerialProtocol.h`
- `firmware/Axio/src/osc/Config.h`
- `firmware/Axio/src/osc/Editor.h`
- `firmware/Axio/src/osc/Engine.h`
- `firmware/Axio/src/power/Battery.h`
- `firmware/Axio/src/power/PowerController.h`
- `firmware/Axio/src/power/SleepButton.h`
- `firmware/Axio/src/transport/OscRuntime.cpp`
- `firmware/Axio/src/transport/OscRuntime.h`
- `firmware/Axio/src/web/DashboardHttp.h`
- `firmware/Axio/src/web/DashboardRuntime.cpp`
- `firmware/Axio/src/web/DashboardRuntime.h`
- `firmware/Axio/src/web/DashboardService.h`
- `firmware/Axio/src/web/ImageSize.h`
- `firmware/Axio/src/web/JsonWriter.h`
- `firmware/Axio/src/web/WebAssets.h`
- `firmware/Axio/web/app.js`
- `firmware/Axio/web/index.html`
- `firmware/Axio/web/style.css`
- `scripts/compile.sh`
- `scripts/package_web.py`
- `scripts/phase4_browser_probe.mjs`
- `tests/dashboard_runtime_harness.cpp`
- `tests/dashboard_service_harness.cpp`
- `tests/hardware/AxioMdnsProbe/AxioMdnsProbe.ino`
- `tests/osc_engine_harness.cpp`
- `tests/phase4_harness.cpp`
- `tests/stubs/Adafruit_I2CDevice.h`
- `tests/stubs/runtime/Arduino.h`
- `tests/stubs/runtime/WiFi.h`
- `tests/stubs/runtime/esp_heap_caps.h`
- `tests/stubs/runtime/esp_sleep.h`
- `tests/stubs/runtime/esp_timer.h`
- `tests/stubs/runtime/mdns.h`
- `tests/support_dashboard_build.py`
- `tests/test_dashboard_runtime.py`
- `tests/test_osc_engine.py`
- `tests/test_phase4.py`
- `tools/dashboard_hardware_probe.py`

`axio_codex_ui/` era già untracked all’inizio e resta la fonte di riferimento, non un risultato modificato della sessione. `.build/` e `captures/` non fanno parte di questo inventario Git.
