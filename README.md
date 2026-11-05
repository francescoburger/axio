# Axio

Fase 1: core modulare per Seeed XIAO ESP32C3 + BNO055, Wi-Fi e OSC UDP.
Matematica V1.1 e protocollo OSC transitorio conservati; letture I²C fallite
non vengono pubblicate. La Fase 2, la dashboard e il nuovo motore OSC non
sono implementati. Verifiche hardware ancora NOT RUN.

| Percorso | Contenuto |
| --- | --- |
| `reference/HandTrackerV1_1.ino` | Baseline storica immutabile |
| `firmware/Axio/Axio.ino` | Sketch apribile direttamente con Arduino IDE |
| `firmware/Axio/hardware_config.h` | Mappa confermata: SDA GPIO6/D4, SCL GPIO7/D5, BNO055 0x29 |
| `firmware/Axio/src/core/` | Acquisizione, taratura, campioni, diagnostica e scheduler |
| `firmware/Axio/src/transport/` | Wi-Fi e serializzazione OSC transitori |
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

Gli script richiedono Arduino CLI, Python 3.9+ e un compilatore C++ per i test
host. Le versioni Arduino verificate sono bloccate in `toolchain.lock.json`:
CLI 1.5.1, core `esp32:esp32` 3.3.12, Adafruit BNO055 1.6.4, Unified Sensor
1.1.15, BusIO 1.17.4, OSC (CNMAT) 4.0.0, WiFiManager 2.0.17. Tutto è risultato
già installato sulla macchina ispezionata. Nessuna installazione eseguita.
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

Per conservare i log e compilare anche l'originale, copiato senza modifiche
in una cartella Arduino temporanea dentro il repository:

```sh
mkdir -p .build
bash scripts/compile.sh > .build/compile.log 2>&1
bash scripts/compile.sh --baseline > .build/compile-baseline.log 2>&1
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

Non servono pacchetti Python: solo standard library. Un virtualenv è facoltativo,
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

La cattura non sovrascrive un file esistente. Durata 0 attende Ctrl-C. Configurare
nel portale storico l'IP LAN del Mac e porta 16888; il default `0.0.0.0` non è
una destinazione operativa da assumere valida. L'AP storico si chiama
`Tracker_<id>` e viene richiesto senza password; il futuro AP protetto non è
ancora implementato. Il ricevitore può richiedere consenso al firewall macOS.

Ogni riga JSONL rappresenta **un datagramma**, con timestamp host UNIX e monotono
in ns, sorgente, dimensione e messaggi con indirizzo, type tag e valori. I
timetag dei bundle sono conservati, senza pianificare la consegna. Supportati
`i f s b h d T F N I`; altri tag e pacchetti malformati vengono registrati come
errore con payload base64. Non è un'implementazione completa di OSC.

Per inviare la taratura storica, sostituire l'IP della XIAO; il comando modifica
quaternion di riferimento e yaw nella RAM del dispositivo:

```sh
AXIO_IP=192.168.1.50 python3 -B - <<'PY'
import os, socket
with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
    sock.sendto(b'/tare\0\0\0,\0\0\0', (os.environ['AXIO_IP'], 16889))
PY
```

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
