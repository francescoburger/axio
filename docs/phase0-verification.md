# Verifiche Fase 0 — 5 novembre 2026

Ambiente reale: macOS arm64, Python 3.9.6, Apple clang 21.0.0, Arduino CLI
1.5.1 Homebrew. Core ESP32 3.3.12 e librerie già presenti; nessuna installazione,
aggiornamento, upload su scheda, pubblicazione o push. All'inizio nessun commit
su `main`, reference e `.DS_Store` già presenti non tracciati.

## Esiti

| Verifica | Stato | Evidenza e limiti |
| --- | --- | --- |
| CLI, core, librerie richieste | PASS | Versioni esatte in `toolchain.lock.json`; preflight eseguito |
| FQBN XIAO ESP32C3 | PASS | `board listall`, `board details` e `boards.txt` del core installato: `esp32:esp32:XIAO_ESP32C3` |
| Integrità baseline | PASS | SHA-256 `40c8113e8cb8d0ef0229ffc50282933e889a94c3e8795f8dac942a1968aaf259` invariato |
| Copia iniziale Axio | PASS | Confronto byte per byte, unica sostituzione dei define con include hardware |
| Compilazione originale | PASS | Copia temporanea compatibile Arduino, nessuna modifica alla reference |
| Compilazione Axio | PASS | CLI e opzioni bloccate; nessun upload |
| Calcoli C++ originali contro riferimento Python | PASS | 1.345 vettori, libreria imumaths installata, senza hardware |
| Suite automatica completa | PASS | 8 test, inclusi parser, analisi e UDP localhost |
| Sintassi script shell | PASS | `bash -n scripts/compile.sh` |
| Cablaggio e rilevamento BNO055 | NOT RUN | Richiede XIAO e breakout |
| Verifica mediante GUI Arduino IDE | NOT RUN | Struttura compatibile; compilazione eseguita con CLI |
| Upload, seriale e comportamento Wi-Fi reale | NOT RUN | Nessun dispositivo usato |
| Tara fisica, orientamento e calibrazione sensore | NOT RUN | Le verifiche host non sostituiscono il sensore |
| Pacchetti OSC dalla XIAO e jitter | NOT RUN | Solo loopback sintetico verificato |
| Rate e jitter acquisizione IMU | NOT RUN | Non esposti dalla V1.1 |

Entrambe le build con partizione default 4 MB riportano:

```text
Sketch uses 1124989 bytes (85%) of program storage space. Maximum is 1310720 bytes.
Global variables use 39356 bytes (12%) of dynamic memory, leaving 288324 bytes for local variables. Maximum is 327680 bytes.
```

L'uguaglianza di queste dimensioni non prova equivalenza binaria o hardware.
La prima compilazione completa ha emesso warning in librerie esterne: possibile
fallthrough in `OSCMessage.cpp:897`, formato `%d` per `uint32_t` nel debug
WiFiManager `WiFiManager.cpp:3368`. Non corretti in questa fase. Le build con
cache possono non ristampare i warning. Il log della prima build baseline li
conserva in `.build/compile-baseline.log`.

La prima esecuzione dei test è stata FAIL: un valore atteso scritto manualmente
per yaw 359.99° era errato (la formula produce -1/180); corretto il solo test.
Il confronto C++/Python sui 1.345 vettori era già PASS. Inoltre il sandbox ha
negato `bind` UDP: FAIL della prova nell'ambiente ristretto, non evidenza di un
difetto del ricevitore. La suite finale, eseguita fuori dal sandbox dopo
approvazione per il solo traffico localhost, è PASS su tutti gli 8 test.

## Comandi di riproduzione

```sh
cd /Users/franc/apps/axio
mkdir -p .build
bash scripts/compile.sh > .build/compile.log 2>&1
bash scripts/compile.sh --baseline > .build/compile-baseline-final.log 2>&1
python3 -B -m unittest discover -s tests -v > .build/tests.log 2>&1
bash -n scripts/compile.sh
shasum -a 256 reference/HandTrackerV1_1.ino
```

Log, binari e harness generati rimangono locali e ignorati da Git. Se eseguiti
in un ambiente che vieta socket localhost, i test UDP richiedono un permesso
esplicito; non convertire automaticamente il diniego in PASS. Nessuna dipendenza
mancante sulla macchina attuale. Su altri host servono le versioni e i comandi
di predisposizione documentati in README; non vengono installati dagli script.

## File introdotti

Tutti i seguenti sono nuovi; il sorgente storico resta invariato:

```text
.gitignore
AGENTS.md
README.md
toolchain.lock.json
firmware/Axio/Axio.ino
firmware/Axio/hardware_config.h
firmware/Axio/src/README.md
scripts/check_toolchain.py
scripts/compile.sh
tests/math_harness.cpp.in
tests/test_baseline.py
tests/test_osc_tools.py
tools/osc_receiver.py
tools/analyze_capture.py
docs/baseline-v1.1.md
docs/benchmarks.md
docs/phase0-verification.md
```

## Verifiche utente sulla XIAO

1. Confermare GPIO SDA 5 / SCL 6 contro cablaggio reale: la variante XIAO usa
   per default 6/7. Controllare alimentazione/masse/livelli del breakout.
2. Confermare indirizzo 0x29 e presenza del cristallo esterno; verificare
   inizializzazione via seriale e montaggio/remap P0.
3. Verificare associazione Wi-Fi, portale, persistenza IP/porta/ID e lunghezze
   effettive dei campi. Impostare un IP destinatario LAN valido.
4. Registrare i quattro messaggi OSC su 16888 e inviare `/tare` alla XIAO su
   16889; controllare azzeramento yaw e rotazione relativa dell'accelerazione.
   Il quaternion non viene trasmesso: una sua misura diretta richiederebbe
   strumentazione successiva autorizzata.
5. Verificare segni e assi Euler con movimenti noti, gyro in gradi/s rispetto
   al commento storico rad/s, wrap a 180° e comportamento del portale durante
   l'invio. Applicare la procedura benchmark senza attribuire al sensore il
   rate dei pacchetti ricevuti.

## Pronto per la Fase 1

Sketch Arduino compilabile, pin centralizzati ma provvisori, baseline congelata,
toolchain e build documentate, regressione matematica, ricevitore/catture e
protocollo benchmark sono pronti. Restano da acquisire le evidenze hardware.
`src/` documenta i futuri confini dei moduli senza implementarli. I requisiti
futuri sono in AGENTS.md; nessuna scelta di limite OSC o compatibilità Legacy
è stata imposta. La Fase 1 richiede una nuova autorizzazione dell'utente.

## Aggiornamento hardware successivo alla Fase 0

L'utente ha confermato SDA GPIO6 (D4), SCL GPIO7 (D5), BNO055 0x29.
La configurazione corrente è in `firmware/Axio/hardware_config.h`; i valori
5/6 riportati sopra descrivono soltanto la baseline storica, che resta immutata.
La conferma del cablaggio non sostituisce le prove funzionali hardware NOT RUN.
