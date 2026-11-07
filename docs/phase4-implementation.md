# Fase 4 — resoconto della prima implementazione

**Resoconto storico della prima sessione.** Lo stato successivo, inclusi upload,
collaudo fisico e limite mDNS verificato, è in [phase4-verification.md](phase4-verification.md).
Le sezioni seguenti descrivono la prima sessione, prima dell’integrazione runtime.
Nessun commit, push, upload o comando sul tracker eseguito. I riferimenti in
`axio_codex_ui/` sono preesistenti, untracked e non modificati.

## Decisioni della sessione

- La richiesta attuale autorizza Fase 4, superando lo stop storico 3D.
- Un solo editor OSC: Main Address, preview, cinque gruppi, suffissi e un
  intervallo comune. Nessun Add/Remove Message, tab, preset o bundle manuale.
- La compatibilità legacy OSC non è richiesta. Il lettore schema 1 esistente
  rimane per le regressioni e non aggiunge controlli alla dashboard.
- Pulsante GPIO5 verso GND, attivo LOW, pull-up. ADC batteria GPIO0.
- Formula tensione fornita dall'utente: 16 letture corrette in mV,
  `2 * somma / 16 / 1000.0`. L'ordine delle operazioni intere è conservato.
  Nessuna formula percentuale/SoC fornita: percentuale non disponibile.

## Analisi e piano

Arduino CLI 1.5.1, Arduino ESP32 3.3.12, IDF 5.5.5, mDNS 1.13.1,
Adafruit BNO055 1.6.4, BusIO 1.17.4, Unified Sensor 1.1.15, OSC 4.0.0.
Nessuna dipendenza installata o aggiornata. Lo sketch è `firmware/Axio/Axio.ino`.
Il firmware usa un loop cooperativo: tick connettività, scheduler IMU 10 ms,
OSC su campione completo fresco, ricezione OSC e diagnostica USB. Journal OSC,
reti e segreto AP erano separati. Il portale esistente è esclusivamente AP.

Fasi 2 e 3 effettive: motore OSC configurabile, diagnostica, rete multi-SSID,
AP protetto, hostname individuale, discovery asincrona già presenti. La Fase 3
è chiusa con riserve documentate su heap/frammentazione e limiti delle prove.
Non erano presenti dashboard LAN, lettura batteria, pulsante operativo,
sequenza sleep o punto d'ingresso comune. La baseline immutabile non contiene
la formula batteria; i documenti la lasciavano da definire.

Ordine degli interventi: baseline → OSC/schema/editor → journal reti → HTTP
con budget → collegamento web → alimentazione → prototipo mDNS → hardware e
confronti visivi. Rischi: margine partizione circa 214 KB iniziali, heap minimo
storico circa 32 KB, jitter I²C/radio/NVS e carico dei client HTTP. La misura
statica non dimostra disponibilità dell'heap o rispetto del timing.

## Implementato e provato su host

- Schema OSC 2: `interval_ms` intero 10–65.535. Supporta il documento `editor`
  con `main_address`, cinque `suffixes`, cinque maschere `selected` e intervallo.
  Compila fino a cinque messaggi, aggregati per indirizzo espanso, nell'ordine
  Orientation → Real → World → Gyroscope → Quaternion. Persistenza conserva
  l'editor, senza ricostruirlo euristicamente dai messaggi.
- Tutte le destinazioni ricevono tutti i messaggi dello schema 2. Un messaggio
  dovuto genera OSC semplice; più messaggi dovuti generano un bundle. Validazione
  conserva indirizzi, collisioni, float32, 16 argomenti, 1400 byte e journal.
- Rimangono attivi i vecchi percorsi schema 1. Una conversione esplicita opzionale
  arrotonda il periodo legacy verso l'alto; non viene eseguita al boot. L'avvio
  corrente continua ad usare la configurazione precedente: il nuovo editor non
  è ancora collegato al runtime web.
- Scheduler senza catch-up, scadenze in microsecondi da intervalli ms. Il tick
  rimane vincolato a snapshot freschi: intervalli come 11 ms sono rappresentabili,
  ma non garantiscono consegne esatte a 11 ms con acquisizione nominale 10 ms.
- Journal reti AXW2: include password AP nello stesso record CRC/readback/commit;
  legge anche AXW1. Helper bulk con SSID/password codificati hex, conservazione
  password per indice originale anche durante riordino e rinomina. Configurazione
  radio AP usa l'eventuale password personalizzata quando viene armata.
- Parser HTTP separabile: RX 256 byte/tick, TX 512, header 1024, body 7168,
  timeout assoluto 5 s, controllo Host/Origin/token e rifiuto lunghezze duplicate.
  **Non è ancora istanziato o collegato a socket e servizi nel firmware.**
- Accumulatore batteria: una conversione per intervallo di almeno 63 ms,
  media di 16 letture, formula utente, nessuna curva SoC. **Non ancora collegato
  all'ADC nel loop firmware.**
- Metodi BNO055 per lettura/scrittura verificabile dei registri di alimentazione.
  **Non costituiscono ancora una sequenza SUSPEND/Deep Sleep.**

## Web e confronto

`firmware/Axio/web/` contiene cinque render dinamici, usando il CSS copiato dal
prototipo. Controlli: editor unico OSC, chip, preview dei campi aggregati,
import/export, ACK condizionale, password/riordino reti, conferme comandi,
12 barre non saturate numericamente, checkbox unica quaternion/canvas 3D,
pausa polling su pagina nascosta e diagnostica manuale. Nessun dato demo è
usato come fallback nel frontend operativo.

**Gli endpoint chiamati dal frontend sono un contratto ancora da implementare.**
Senza provider API il frontend mostra un errore di caricamento; non è una
dashboard embedded funzionante. Lo script browser usa deliberatamente fixture
indipendenti per QA presentazionale, non simula un collaudo firmware.

Catture a 430×860 CSS px, DPR 2, full page in `captures/phase4-ui/` (ignorate da
Git). `comparison.html` mostra riferimento e implementazione affiancati.
Non è certificata la fedeltà pixel-perfect: mancano l'intera matrice degli stati,
confronti numerici/overlay, validazione su dispositivo e accettazione utente.
Differenze funzionali giustificate: preview include campi, canvas orientato
anziché placeholder, checkbox reali, password con Show/Hide, stati non disponibili.

## mDNS: punto di stop del sottosistema

Verificati header installati e `versions.txt`: esistono add/set/remove per
hostname delegati, oltre a `mdns_hostname_exists`. Quest'ultima indica soltanto
la presenza nella tabella locale: non è verifica di assenza di un leader LAN.
Un probe isolato in `.build/mdns-api-probe/` ha compilato e linkato queste API
con il target XIAO; non è stato caricato né eseguito. Fonte primaria aggiuntiva:
https://docs.espressif.com/projects/esp-protocols/mdns/docs/latest/en/index.html

Non è implementata o simulata la pubblicazione di `axio.local`. L'esistenza
API non prova probing/conflitti sufficienti per elezione e failover. Il passo
verificabile richiesto dal §11.6 è un prototipo isolato su due XIAO: hostname
individuale + delegato, avvio simultaneo, conflitti, perdita leader, ritorno,
partizioni/perdita pacchetti e osservazione DNS/HTTP. Preservare accesso IP e
hostname individuale; non sostituirli con un alias fittizio.

## Verifiche

| Verifica | Esito | Limite |
|---|---|---|
| Baseline iniziale build/suite | PASS | 72 test, 1.345 vettori |
| Suite aggiornata | PASS | 77 test, 1.345 vettori; socket host, non radio |
| Build firmware aggiornata | PASS | 1.101.861 B flash, 113.316 B RAM statica |
| Margine partizione attuale | PASS | 208.859 B; non include server/asset web ancora scollegati |
| Schema 2, editor, interval_ms, bundle, transazioni | PASS | Harness C++ produzione; intervalli 10/11/13/30/65.535 |
| HTTP parser e batteria | PASS | Fake socket e ADC; non integrazione runtime |
| Sintassi JS | PASS | node --check |
| Browser cinque schermate | PASS | Fixture; no overflow, quaternion/3D e ACK condizionali |
| Fedeltà completa e matrice degli stati | NOT RUN | Catture iniziali non equivalgono ad accettazione |
| API e server embedded | NOT RUN | Non integrati |
| Batteria fisica e percentuale | NOT RUN | ADC non collegato; curva SoC assente |
| Restart, SUSPEND, wake e ripresa | NOT RUN | Sequenza non implementata |
| axio.local: elezione/conflitti/subentro | NOT RUN | Solo compile/link delle API |
| Heap/jitter/OSC con dashboard | NOT RUN | Nessun upload o benchmark Fase 4 |

Warning noto della dipendenza OSCMessage.cpp:897, implicit fallthrough; non
corretto nella libreria esterna. Un run OSC loopback è fallito per sandbox
(socket PermissionError); la suite è stata rieseguita con permesso e passa.
Il primo probe browser da file locale non caricava lo script; corretto con
server loopback. Nessuna installazione, modifica dei riferimenti o hardware.

## Lavoro restante, non attribuito a blocchi dell'utente

1. Istanziare HTTP sul solo indirizzo STA e aggiungere handler/contratto snapshot.
   Budget, backpressure, token, concorrenza e timeout vanno provati nel runtime.
2. Integrare identità globale persistente e AP SSID derivato; aggiornare TXT peer
   con Tracker ID e servizio HTTP, mantenendo hostname hardware stabile.
3. Completare Apply rete in due fasi (persistenza prima dell'ACK, attivazione
   dopo risposta) e riarmo sicuro AP. Il helper attuale disconnette immediatamente:
   **non va esposto direttamente come handler HTTP Apply**.
4. Collegare asset compressi e API a tutte le schermate. Validate deve usare il
   validatore reale; Import non deve effettuare Apply.
5. Collegare ADC e implementare la macchina a stati SUSPEND verificato, stop
   comunicazioni, GPIO5 LOW/pull-up, Deep Sleep e reboot completo al wake.
6. Eseguire prototipo mDNS su due tracker prima dell'integrazione del nome comune.
7. Completare screenshot e casi interattivi; collaudo su XIAO, heap/largest block,
   jitter IMU, OSC, fallback, riconnessioni, browser scollegato e più client.

## Inventario file della sessione

Modificati:
- `firmware/Axio/hardware_config.h`
- `firmware/Axio/src/connectivity/ApRuntime.cpp`
- `firmware/Axio/src/connectivity/ConnectivityManager.h`
- `firmware/Axio/src/connectivity/ConnectivityRuntime.cpp` e `.h`
- `firmware/Axio/src/core/BnoDevice.h`
- `firmware/Axio/src/osc/Config.h`, `Engine.h`
- `firmware/Axio/src/transport/OscRuntime.cpp` e `.h`
- `tests/osc_engine_harness.cpp`, `tests/test_osc_engine.py`
- `tests/stubs/Adafruit_I2CDevice.h`

Nuovi:
- `firmware/Axio/src/osc/Editor.h`
- `firmware/Axio/src/power/Battery.h`
- `firmware/Axio/src/web/DashboardHttp.h`
- `firmware/Axio/web/index.html`, `style.css`, `app.js`
- `scripts/phase4_browser_probe.mjs`
- `tests/phase4_harness.cpp`, `tests/test_phase4.py`
- questo documento.

Reference, fixture Fase 0, calibrazione, scheduler IMU e trasporto matematico
sono preservati. Nessun commit, push o upload.
