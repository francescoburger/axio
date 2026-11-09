# Axio

**Aggiornamento mDNS (9 novembre 2026):** `http://axio.local` integrato con
elezione automatica e subentro, verificati sulle due XIAO. Hostname individuali,
reti e impostazioni OSC conservati; principale con IMU, secondaria senza IMU.
[Verifiche e limiti del punto d'accesso comune](docs/phase4-entry-point-verification.md).

**Aggiornamento UI (9 novembre 2026):** icona OSC viola, logo XYZ e solo Axio
nell'header, vista Motion isometrica con assi che ruotano insieme al box
(yaw su Y, pitch su X, roll su Z). Aggiornamento
caricato sulla XIAO principale; configurazioni preservate.
[Verifiche UI e limiti](docs/debug-round3-verification.md).

**Correzioni aggiuntive (8 novembre 2026):** navigazione, titolo e batteria a
larghezza finestra; rimosso Message 01; sezione Configuration Files e nomi
Acknowledgement estesi. Risolto il rifiuto dei trigger numerici OSC da Pure Data.
Revisione caricata sulla XIAO principale, impostazioni conservate.
[Verifiche e limiti del collaudo](docs/debug-round2-verification.md).

**Correzioni 001–005 (8 novembre 2026):** header a tutta finestra, badge superiore
rimosso da tutte le schermate, stile Messages, terminologia Calibration e
dashboard completa anche in AP. Revisione caricata sulla XIAO principale
AC:27:6E:7E:32:38, con configurazioni conservate e verifiche USB/STA/browser;
Dashboard su AP reale: PASS manuale riferito dall'utente il 9 novembre 2026;
provisioning e passaggio AP→STA non confermati da questa prova. Compatibilità e limiti in
[correzioni dashboard](docs/debug-corrections-verification.md).

**Fase 4:** dashboard LAN inglese con cinque schermate, editor OSC unico a
intervalli ms, batteria A0/GPIO2 e controlli di alimentazione. Build installata
e collaudata sulle XIAO disponibili; risultati e limiti in
[verifiche Fase 4](docs/phase4-verification.md).
`axio.local` ora usa il probing nativo nello stesso responder e conserva i nomi
individuali. [Implementazione e collaudo](docs/phase4-entry-point-verification.md).
La precedente scorciatoia con nome comune delegato rimane documentata come
prova fallita in [evidenza storica mDNS](docs/phase4-mdns-decision.md).
Nessun commit o push di questa implementazione; accettazione visiva ancora da
esprimere rispetto alle catture in `captures/phase4-device-ui/`.

## Stato storico Fase 3

Fase 3, **Checkpoint 3D: collaudo eseguito con limiti documentati, in attesa di approvazione**: STA multi-rete, fallback AP WPA2 dopo
15 secondi e provisioning minimo su AP. Password AP fissa `12345678`,
richiesta esplicitamente; SSID leggibile con `AP CREDENTIALS` via USB.
Firmware caricato sui due XIAO autorizzati; il secondo è senza BNO055.
Hostname `axio-<MAC completo>.local`, servizio `_axio._udp` e discovery
asincrona STA implementati e verificati su LAN reale. Dashboard esclusa;
risultati e limiti del collaudo 3D nel rapporto di verifica.
[Architettura e comandi Wi-Fi](docs/phase3-connectivity.md),
[verifiche e memoria 3A/3B/3C/3D](docs/phase3-verification.md),
[piano LAN 3C](docs/phase3c-lan-plan.md).

Base storica Fase 2, **Checkpoint B**: core modulare per Seeed XIAO ESP32C3 + BNO055,
diagnostica USB JSON Lines e motore OSC configurabile separato dal core.
Matematica V1.1 invariata, configurazione validata e persistente a due slot.
Sul Checkpoint A sono verificati upload, USB,
BNO055 ready e 99,774 Hz. Collaudo hardware B PASS dopo correzioni USB/client/
scheduler: IMU 99,947 Hz, OSC Mac 99,942 pacchetti/s, routing moderato e
persistenza dopo reboot verificati; configurazione originale ripristinata.
Nessuna tara fisica. Benchmark di capacità e power-cut NVS ancora NOT RUN.
Schema, metriche e procedura hardware: [motore OSC](docs/phase2-osc.md).

| Percorso | Contenuto |
| --- | --- |
| `reference/HandTrackerV1_1.ino` | Baseline storica immutabile |
| `firmware/Axio/Axio.ino` | Sketch apribile direttamente con Arduino IDE |
| `firmware/Axio/hardware_config.h` | Mappa confermata: SDA GPIO6/D4, SCL GPIO7/D5, BNO055 0x29 |
| `firmware/Axio/src/core/` | Acquisizione, taratura, campioni, diagnostica e scheduler |
| `firmware/Axio/src/transport/` | Adattatore UDP/NVS OSC e compatibilità Legacy |
| `firmware/Axio/src/connectivity/` | Manager STA, journal reti e adattatore ESP32 |
| `firmware/Axio/src/osc/` | Schema, preset, codec, scheduling, routing e journal |
| `firmware/Axio/src/diagnostics/` | Protocollo seriale limitato, servizio e invio USB senza attese |
| `tests/fixtures/phase0-Axio.ino` | Copia Fase 0 congelata, controllo byte per byte |
| `scripts/`, `toolchain.lock.json` | Compilazione e verifica versioni |
| `tests/` | Regressione matematica e strumenti OSC senza hardware |
| `tools/` | Ricevitore UDP OSC e analisi delle catture |
| `docs/` | Baseline, benchmark e rapporto reale delle verifiche |

Dal Terminale, posizionarsi nella radice del repository:

```sh
cd /Users/franc/apps/axio
bash scripts/compile.sh
python3 -B -m unittest discover -s tests -v
```

La diagnostica accetta `AXIO <id> <comando>` e risponde con JSONL correlato:

```text
AXIO 1 PING
{"axio":1,"id":"1","ok":true,"data":{"pong":true}}
```

Disponibili PING, INFO, STATUS, METRICS, CALIBRATION, CONFIG GET e DIAG RESET.
WIFI GET/SET/DELETE gestiscono le reti; AP CREDENTIALS mostra SSID e password
AP su richiesta USB. DISCOVERY GET legge i tracker presenti nella cache LAN,
senza avviare una nuova scansione. Nessun routing OSC automatico dalla discovery.
CONFIG VALIDATE controlla integralmente lo schema 1 senza applicarlo;
CONFIG APPLY salva e attiva solo una configurazione valida. CALIBRATION cambia i riferimenti
in RAM; DIAG RESET azzera la finestra diagnostica. Nessun output periodico.
Protocollo, limiti, memoria e risultati: [verifiche Fase 2](docs/phase2-verification.md).

I test host non richiedono pacchetti Python. Per USB reale, setup esplicito
opzionale (non eseguito automaticamente):

```sh
python3 -m venv .venv
.venv/bin/python -m pip install -r tools/requirements-hardware.txt
.venv/bin/python -B tools/hardware_test.py --list-ports
# Sostituire con la porta verificata; non esegue upload o tara:
.venv/bin/python -B tools/hardware_test.py --port /dev/cu.usbmodem2101 --metrics-seconds 10 --report captures/phase2-usb.json
# Solo quando si desidera effettivamente tarare:
.venv/bin/python -B tools/hardware_test.py --port /dev/cu.usbmodem2101 --calibrate --report captures/calibration.json
```

Senza porta o pyserial: NOT RUN, exit 2. Test falliti: exit 1. Report senza
sovrascrittura. Wi-Fi/ricevitore richiesti solo per le opzioni dedicate
`--require-wifi --wifi-wait-seconds 20 --osc-seconds 10`; configurare prima sul tracker la destinazione
Mac. `--reset-metrics` modifica esplicitamente i contatori. La persistenza del
nuovo motore è testata su host; ripristino fisico sulla XIAO ancora NOT RUN.

Upload separato e intenzionale, mai richiamato dai test:

```sh
bash scripts/upload.sh --port /dev/cu.usbmodem2101
```

Ricompila e carica con FQBN `esp32:esp32:XIAO_ESP32C3`, stesse opzioni della
build. Su Arduino IDE scegliere **USB CDC On Boot: Enabled**. Nel core 3.3.12
questa opzione corrisponde a `CDCOnBoot=default`, già nello script; disattivata,
Serial usa UART0 e la build Axio si interrompe. Dopo upload/reset verificare
nuovamente la porta e chiudere Serial Monitor prima dei test Python.

Gli script richiedono Arduino CLI, Python 3.9+ e un compilatore C++ per i test
host. Le versioni Arduino verificate sono bloccate in `toolchain.lock.json`:
CLI 1.5.1, core `esp32:esp32` 3.3.12, Adafruit BNO055 1.6.4, Unified Sensor
1.1.15, BusIO 1.17.4, OSC (CNMAT) 4.0.0, WiFiManager 2.0.17. Tutto è risultato
già installato sulla macchina ispezionata. Nessuna nuova dipendenza firmware;
pyserial è stato installato nel venv per le prove USB autorizzate.
WiFi, DNSServer, WiFiClient, WiFiUdp, Preferences e Wire provengono dal core.

Il FQBN **`esp32:esp32:XIAO_ESP32C3`** è stato ricavato dal core installato:

```sh
arduino-cli board listall XIAO_ESP32C3
arduino-cli board details --fqbn esp32:esp32:XIAO_ESP32C3
python3 -B scripts/check_toolchain.py
```

Lo script fissa le opzioni della scheda (160 MHz, flash 4 MB, partizione
default, USB CDC attivo), controlla le versioni e interrompe la build se non
corrispondono. Non installa e non carica firmware. Binari in `.build/artifacts/`,
temporanei e cache sotto `.build/`. Ripetibilità con la toolchain registrata,
senza promessa di binari identici tra host o date diverse. Librerie modificate
localmente non sono riconosciute dal solo controllo di versione.

Per compilare anche l'originale, copiato senza modifiche
in una cartella Arduino temporanea dentro il repository:

```sh
mkdir -p .build
bash scripts/compile.sh
bash scripts/compile.sh --baseline
```

Su un'altra macchina, **solo dopo autorizzazione**, predisporre Arduino CLI
1.5.1 e, se assenti, installare queste dipendenze (comandi indicativi di setup
esplicito, mai eseguiti dagli script):

```sh
arduino-cli core update-index --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core install esp32:esp32@3.3.12 --additional-urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli lib update-index
arduino-cli lib install 'Adafruit Unified Sensor@1.1.15' 'Adafruit BusIO@1.17.4' 'Adafruit BNO055@1.6.4' 'OSC@4.0.0' 'WiFiManager@2.0.17'
```

Questi comandi scrivono nel profilo Arduino esterno al repository e richiedono
rete. Se manca C++, installare gli strumenti macOS con `xcode-select --install`
solo dopo approvazione. Arduino IDE deve usare le stesse versioni e la scheda
XIAO_ESP32C3; aprire `firmware/Axio/Axio.ino`. Verificare con Compila prima di
qualsiasi upload autorizzato e scegliere manualmente la porta USB corretta.

I test estraggono le funzioni originali e compilano anche il core di produzione,
usando gli header `imumaths` della libreria installata. I 1.345 vettori storici
restano invariati: baseline e core sono confrontati con l'oracolo Python e fra
loro. Mock minimi verificano errori I²C, conversioni dei registri, campioni,
tara, sequenze, rollover e serializzazione OSC. Nessun risultato hardware è
simulato come misura reale. La posizione
predefinita è `~/Documents/Arduino/libraries/Adafruit_BNO055`. Per un profilo
diverso:

```sh
BNO055_LIBRARY='/percorso/Adafruit_BNO055' python3 -B -m unittest discover -s tests -v
```

Per i test host non servono pacchetti Python: solo standard library. Un virtualenv è facoltativo,
interamente nel repository, e non richiede `pip install`:

```sh
python3 -m venv .venv
source .venv/bin/activate
python -B -m unittest discover -s tests -v
```

Per registrare OSC sul Mac:

```sh
python3 -B tools/osc_receiver.py --bind 0.0.0.0 --port 16888 --duration 60 --output captures/baseline-01.jsonl
python3 -B tools/analyze_capture.py captures/baseline-01.jsonl
```

La cattura non sovrascrive un file esistente. Durata 0 attende Ctrl-C. Per il firmware B usare CONFIG APPLY con IP LAN del Mac e porta 16888; il default `0.0.0.0` non è
una destinazione operativa da assumere valida. Nei firmware storici precedenti al 3A, l'AP si chiamava
`Tracker_<id>` senza password. Il firmware 3B usa `Axio-<MAC completo>`, WPA2
e password `12345678`; dopo il fallback aprire `http://192.168.4.1/`.
Il firmware 3A intermedio configurava le reti solo via USB. Il ricevitore può richiedere consenso al firewall macOS.

Ogni riga JSONL rappresenta **un datagramma**, con timestamp host UNIX e monotono
in ns, sorgente, dimensione e messaggi con indirizzo, type tag e valori. I
timetag dei bundle sono conservati, senza pianificare la consegna. Supportati
`i f s b h d T F N I`; altri tag e pacchetti malformati vengono registrati come
errore con payload base64. Non è un'implementazione completa di OSC.

Con il nuovo default `/calibration`, per inviare la calibrazione sostituire l'IP della XIAO; il comando modifica
quaternion di riferimento e yaw nella RAM del dispositivo:

```sh
AXIO_IP=192.168.1.50 python3 -B - <<'PY'
import os, socket
with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
    sock.sendto(b'/calibration\0\0\0\0,\0\0\0', (os.environ['AXIO_IP'], 16889))
PY
```

Le configurazioni precedenti con `tare_address` sono lette senza cambiare
l'indirizzo né l'ACK salvati; le risposte usano `calibration_address`.
`TARE`, `--tare` e gli endpoint predefiniti precedenti restano alias di
compatibilità. Per inviare a un indirizzo personalizzato usare quello restituito
da CONFIG GET. Nessuna calibrazione automatica al boot o durante la migrazione.

Analisi esatta e rischi: [docs/baseline-v1.1.md](docs/baseline-v1.1.md).
Procedura e limiti delle misure: [docs/benchmarks.md](docs/benchmarks.md).
Rapporto storico: [docs/phase0-verification.md](docs/phase0-verification.md).
Architettura, memoria, differenze e verifiche correnti:
[docs/phase1-verification.md](docs/phase1-verification.md).
Cablaggio confermato dall'utente: **SDA GPIO6 (D4)**, **SCL GPIO7 (D5)**,
BNO055 **0x29**. Pin e indirizzo sono modificabili in `hardware_config.h`;
l'inizializzazione usa `Wire.begin(SDA_PIN, SCL_PIN)`. I test sulla XIAO non
eseguiti restano NOT RUN; la conferma del cablaggio non costituisce una misura
di funzionamento o prestazioni.
