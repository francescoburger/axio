# Motore OSC — schema 1 e prove del Checkpoint B

Implementazione software autorizzata dopo il commit locale del Checkpoint A
`30cd5e147d04b30691ae78711e5b743d312e44b0`
(`feat: complete phase 2 checkpoint A USB diagnostics and hardware tests`).
Nessun preset Legacy. Nessuna dashboard, nuova gestione multi-Wi-Fi o Fase 3.
Il firmware B è stato successivamente caricato e collaudato: IMU 99,947 Hz,
OSC Mac 99,942 pacchetti/s nella misura finale; routing moderato e persistenza
reali PASS, configurazione originale ripristinata. I 99,774 Hz del Checkpoint A
restano un riferimento storico. Risultati, errori corretti e limiti nel rapporto
phase2-verification.md, sezione finale.

## Architettura

`osc/Config.h`: schema, catalogo campi, preset, parser/validatore e JSON canonico.
`osc/Codec.h`: messaggi e bundle OSC, buffer fisso; `osc/Engine.h`: scheduler,
routing e tara; `osc/Journal.h`: journal a due slot; `osc/Transaction.h`:
preparazione, salvataggio e attivazione. Tutti sono compilabili su host.
`transport/OscRuntime.cpp` adatta questi moduli a WiFiUDP e Preferences.

Lo sketch acquisisce con il core immutato e consegna solo snapshot completi.
L'engine non include BNO055/Wire e non acquisisce I²C. Non reinvia uno snapshot
con la stessa sequenza; non trasmette l'ultimo valido dopo una lettura fallita.
Scheduler IMU ancora 10 ms, unsigned millis, senza catch-up. Il motore OSC usa
il clock monotono 64 bit: mantiene la scadenza se il ritardo è inferiore a un
periodo, salta il backlog oltre un periodo. A 100 Hz, massimo nominale del core a 10 ms, ogni snapshot fresco è dovuto:
un secondo gating sul timestamp di completamento introduceva underrate per
jitter ed è stato corretto nel collaudo. Ai rate inferiori resta la scadenza.
Ogni tick produce al massimo un invio per messaggio/destinazione; nessuna raffica di recupero. L'invio dipende
dalla disponibilità di snapshot freschi, quindi non garantisce il rate richiesto.

`LegacyTransport` conserva il Wi-Fi preesistente e la lettura iniziale delle
vecchie preferenze. I vecchi campi IP/porta/ID sono stati rimossi dal portale
per evitare modifiche incoerenti con il motore; il portale Wi-Fi resta quello
esistente, non una nuova dashboard. La vecchia funzione serializzatrice rimane
come sorgente per il test storico, ma il loop non la chiama e non apre il suo
listener. I pacchetti attivi sono esclusivamente quelli del nuovo motore.

## Configurazione completa

CONFIG VALIDATE e CONFIG APPLY ricevono lo stesso oggetto completo. Nessuna
patch parziale. Ordine delle proprietà indifferente; proprietà mancanti,
sconosciute o duplicate sono errori. Esempio modificabile:

```json
{
  "schema": 1,
  "preset": "Orientation",
  "id": "3",
  "listen_port": 16889,
  "tare_address": "/tare/{id}",
  "ack": {"enabled": false, "address": "/tared/{id}", "destination": 0},
  "destinations": [{"ip": "192.168.1.4", "port": 16888}],
  "messages": [
    {"address": "/orientation/{id}", "hz": 100, "bundle": true,
     "fields": ["yaw", "pitch", "roll"], "destinations": [0]},
    {"address": "/quaternion/{id}", "hz": 50, "bundle": true,
     "fields": ["q0", "q1", "q2", "q3"], "destinations": [0]}
  ]
}
```

`schema` è esattamente 1. `preset` è il nome descrittivo Orientation, Motion o
Custom: non rende immutabili messaggi/campi. `id` è stringa ASCII di 1–16
caratteri alfanumerici, `_` o `-`. Ogni destinazione ha IPv4 unicast numerico e
porta 1–65535. Indici destinazione zero-based; nessuna destinazione duplicata.
Non sono risolti hostname o mDNS. Esclusi IP con primo ottetto 0 o >=224 e
ultimo ottetto 255; loopback ammesso per test. I suffissi .255 sono esclusi
conservativamente senza dedurre la subnet dell'utente.

Indirizzi: iniziano con `/`, hanno almeno un carattere dopo slash, niente slash
finale/doppio, spazi, wildcard OSC, virgolette, backslash o altri token tra
parentesi graffe. `{id}` è l'unico token, sostituito anche per tara e ACK.
Massimo 79 byte incluso lo spazio necessario all'espansione, più terminatore
nel buffer da 80. Stringhe schema ASCII senza escape JSON; il parser del
protocollo riconosce gli escape JSON, ma lo schema li rifiuta nei suoi nomi.
Indirizzi messaggio unici dopo espansione, anche su route differenti; nessun
conflitto fra messaggi, indirizzo tara e indirizzo ACK (anche se disabilitato).

Ogni messaggio ha `fields` non vuoto, ordinato, e `destinations` non vuoto con
indici unici. I campi determinano il tipo: **OSC float32 `f`** per tutti gli
argomenti; un campo ripetuto è consentito e produce due argomenti distinti.
Niente tipi scelti liberamente o costanti implicite: nomi non riconosciuti,
argomenti numerici anziché nomi e valori fuori schema sono rifiutati.
`hz` è intero 1–100: limite tecnico provvisorio coerente con lo scheduler IMU,
non una misura di sostenibilità; frequenze frazionarie non supportate in schema 1.

`bundle:true`: tutti i messaggi dovuti nello stesso tick per la medesima
destinazione vengono raccolti in un bundle con timetag immediato 1.
`bundle:false`: un datagramma per messaggio. Messaggi con rate diversi possono
condividere il bundle nei tick in cui sono entrambi dovuti. La validazione
calcola anche la dimensione peggiore quando tutti sono dovuti insieme.

Limiti **provvisori**, da rivedere dopo benchmark reali:

| Risorsa | Limite tecnico |
| --- | ---: |
| Destinazioni | 8 |
| Messaggi | 16 |
| Argomenti per messaggio | 16 |
| Datagramma OSC | 1.400 B |
| JSON configurazione | 7.168 B |
| Riga seriale, escluso LF | 8.192 B |
| Buffer risposta seriale | 12.288 B |
| Profondità sintattica JSON | 8 |

Nessun limite definitivo di prodotto derivato da questi numeri. Non tutte le
combinazioni 16 messaggi × 16 argomenti possono entrare nello stesso bundle:
la validazione rifiuta `packet_too_large`. Lo schema resta vincolato anche dal
budget seriale; il timeout assoluto della riga è ancora 1.000 ms e il client
usa stop-and-wait e ID di 16 caratteri (8 hex casuali di sessione + 8 hex di
contatore), per ignorare risposte tardive dopo una riapertura USB. HWCDC RX è
configurato a 8.193 byte prima di Serial.begin: la coda predefinita di 256 byte
perdeva richieste complete mentre il loop acquisiva il sensore. La coda è heap
limitato, distinto dai buffer statici del parser. I test storici overflow sono conservati aggiornando solo
le dimensioni degli input per esercitare il nuovo limite.

## Catalogo dati

Nessun clamp, inverse o normalizzazione quaternion aggiuntiva. Si leggono
esattamente i float già calcolati dal core V1.1.

| Campi | Tipo OSC | Intervallo nominale | Significato / eccezioni storiche |
| --- | --- | --- | --- |
| q0, q1, q2, q3 | f | −1…1 se quaternion unitari | Componenti float del quaternion relativo, offset coniugato × corrente; non normalizzato, può eccedere il nominale |
| yaw | f | −1…1 | Yaw con offset intero, cast verso zero e resto C++; input storici negativi possono produrre valori sotto −1 |
| pitch | f | −1…1 | Euler Y intero / 90; nessun controllo del range del registro |
| roll | f | −1…1 | −Euler Z intero / 180; nessun controllo del range del registro |
| axReal, ayReal, azReal | f | −1…1 per ±35 m/s² | Accelerazione lineare nel frame sensore / 35; può superare ±1 |
| axWorld, ayWorld, azWorld | f | −1…1 per ±35 m/s² e rotazione unitaria | Accelerazione relativa nel frame di riferimento, matrice storica / 35; quaternion non unitari possono amplificarla |
| gx, gy, gz | f | −1…1 per ±1500 °/s | Cast verso zero del gyro in °/s e divisione per 1500; può superare ±1 |

Orientamento iniziale del core e yaw restano identità/zero senza tara automatica.
Orientation iniziale: Euler e quaternion; Motion: lineare, world e gyro;
Custom: un messaggio yaw. Tutti modificabili attraverso lo stesso schema.
Se esistono preferenze storiche valide, solo ID/IP/porta sono usati per preparare
il default Orientation in RAM; nessuna migrazione persistente automatica.
In assenza di target valido, default valido su 127.0.0.1:16888, ID 0,
listener 16889, `/tare`, ACK disabilitato. Un default loopback non consegna al Mac:
la destinazione reale deve essere applicata intenzionalmente.

## Persistenza e attivazione

Namespace NVS nuovo `axio-osc-v1`, separato da credenziali Wi-Fi e vecchie
preferenze. Due blob `slot0`/`slot1`: magic AXO1, generazione uint32, lunghezza,
CRC32 e JSON canonico schema 1. Il selettore atomico `active` contiene slot e
generazione (uint64). Non vengono serializzati padding di strutture C++.

1. Parse e validazione integrale in configurazione candidata separata.
2. Preparazione di un secondo socket se cambia la porta di ascolto.
3. Scrittura dello slot inattivo, poi rilettura/verifica byte, lunghezza e CRC.
4. Aggiornamento del selettore NVS: punto di commit.
5. Switch del socket e copia completa della configurazione attiva nello stesso
   task, prima di servire il prossimo snapshot. Scadenze OSC reinizializzate.

Prima del punto 4, errori chiudono solo il socket preparato, mantengono listener,
configurazione e selettore precedenti. Al boot: slot selezionato valido della
generazione esatta; se corrotto, slot precedente valido con generazione minore;
altrimenti default. Uno slot nuovo non committato non viene caricato come
fallback. Senza selettore valido si usa il default, non si indovina uno slot.
Generazione esaurita: APPLY rifiutato. Nessuna cancellazione delle credenziali
né reset factory aggiunto. Schema sconosciuto: non reinterpretato o convertito;
si tenta il precedente schema 1 e poi default. Futuri schemi richiederanno
migrazione esplicita, validata, prima del punto di commit.

Garanzie basate sull'atomicità delle singole chiavi NVS del core ESP32 e sul
modello di errori verificato su host. Power-cut reale, NVS piena/usura e radio
con listener in transizione **NOT RUN**. CONFIG APPLY e TARE chiamano API
sincrone di flash/socket/I²C: possono prolungare un giro del loop. Nessuna
attesa applicativa o parser bloccante, ma nessuna promessa di latenza durante
queste operazioni. Non usarle nel mezzo di una finestra benchmark.

## Tara e metriche

Listener: un datagramma per giro, messaggio OSC singolo all'indirizzo configurato
con zero argomenti, padding rigoroso. Bundle in ingresso, pattern e argomenti
aggiuntivi sono rifiutati; non è un parser OSC generico. Input massimo 1.400 B,
scarto oltre limite. WiFiUDP del core alloca internamente buffer limitati nella
ricezione: il codec del motore non alloca, ma non si afferma zero heap per l'intera
pila di rete. Nessun comando USB è esposto su rete.

USB TARE e OSC tare usano lo stesso ingresso del motore, che chiama il callback
`ImuManager::calibrateReference()`. Solo se restituisce true, ACK opzionale con
indirizzo configurato, **type tag `,i` e valore 1**, alla destinazione configurata.
Nessun ACK di successo se il sensore è assente o le letture falliscono.
ACK tentato non significa ricevuto; entra nei contatori UDP insieme agli altri
pacchetti. Nessuna scrittura persistente per la tara.

METRICS conserva i campi del Checkpoint A. `osc_attempts`/`osc_attempt_hz`
contano ora chiamate di dispatch con snapshot freschi, non messaggi o pacchetti;
`osc_requested_hz` è null nel motore configurabile perché il rate è per messaggio.
`udp_sent`/`host_received` restano null: non si presenta una coda UDP come consegna.
I campi autorevoli del motore sono sotto `engine`:

- `window_us`, `udp_attempts`, `udp_queued`: endPacket riuscito indica accettazione
  locale, **non** consegna verificata (`delivery_verified:false`).
- `messages[i].requested_hz`, `scheduled`, `effective_hz`: eventi del messaggio
  prima del fan-out; `attempted`/`queued`: invii per tutte le sue route, contati
  separatamente. La frequenza effettiva indica scheduling, non consegne.
- `max_send_us`, `max_receive_us`, `max_config_us`, `malformed_commands`, stato
  listener e origine configurazione. Max config include validazione e NVS.

L'applicazione crea una nuova finestra di metriche OSC; non azzera contatori IMU.
DIAG RESET esplicito azzera entrambe, conservando stato/riferimenti, sequenza,
init_errors e minimo heap dal boot. Gli intervalli IMU hanno media e deviazione
standard popolazione in µs, con Welford sui tentativi periodici; non sugli
aggiornamenti interni di fusione del BNO055. Errori/contatori separati come prima.
Heap libero/minimo in STATUS. Nessuna telemetria periodica: benchmark senza
polling intensivo, METRICS prima/dopo; distinguere finestre e config applicate.
`max_read_us` comprende anche il bookkeeping in beginRead prima delle letture;
`max_processing_us` è il residuo della durata acquisizione fuori da tale finestra.
Sono misure strumentate, non tempi puri del bus o della sola matematica.

## Procedura riproducibile delle prove XIAO

Procedura inizialmente preparata nel Checkpoint B software, poi eseguita con
autorizzazione esplicita. Esiti reali nel rapporto; non ripetere APPLY o tara
senza intenzione esplicita.

Compilazione/test software non effettuano upload. Quando si decide di caricare B,
chiudere Serial Monitor, verificare porta, quindi:

```sh
bash scripts/upload.sh --port /dev/cu.usbmodem2101
.venv/bin/python -B tools/hardware_test.py --list-ports
.venv/bin/python -B tools/hardware_test.py --port /dev/cu.usbmodem2101 --timeout 2 --boot-timeout 20 --metrics-seconds 10 --report captures/checkpoint-b-usb-01.json
```

Generare un payload modificabile nella cartella locale `.build/` (sostituire
IP del Mac con quello attuale; nessuna esportazione/importazione di backup):

```sh
python3 -B tools/osc_config.py --preset Orientation --ip 192.168.1.4 --id 3 > .build/orientation.json
.venv/bin/python -B tools/osc_config.py --port /dev/cu.usbmodem2101 --validate .build/orientation.json
# Azione esplicita persistente, dopo validazione:
.venv/bin/python -B tools/osc_config.py --port /dev/cu.usbmodem2101 --apply .build/orientation.json
.venv/bin/python -B tools/osc_config.py --port /dev/cu.usbmodem2101 --get
# Sostituire 192.168.1.50 con IP reale del tracker:
.venv/bin/python -B tools/osc_hardware_test.py --config .build/orientation.json --tracker-ip 192.168.1.50 --serial-port /dev/cu.usbmodem2101 --duration 30 --report captures/checkpoint-b-osc-01.json
```

Il ricevitore verifica indirizzi, tag/numero argomenti, bundle, porte di routing,
float finiti e almeno un messaggio per route; riporta jitter per porta Mac.
Non rifiuta valori fuori intervallo nominale. Non applica configurazioni.
CONFIG GET viene confrontato con il payload atteso, METRICS prima/dopo inclusi
nel report. Indirizzi/porte IP destinazione devono appartenere al Mac ricevitore;
più IP con la stessa porta non distinguibili da questo strumento su bind wildcard.
Firewall/socket LAN possono richiedere consenso. Exit non zero su FAIL.

Prova tara: modificare intenzionalmente il payload con ACK enabled=true,
validare/applicare, mettere il tracker nella posa voluta e aggiungere `--tare`
al comando ricevitore. Viene inviato un solo comando OSC di tara e richiesto
un solo ACK valido. Senza `--tare` non si invia alcun comando di stato.
Provare separatamente sensore assente e mancato ACK; niente risultati inventati.

Persistenza reale: applicare esplicitamente un payload, leggere GET, riavviare
fisicamente, attendere CDC, ripetere GET e il ricevitore con lo stesso payload.
Il confronto automatico GET nel ricevitore rileva differenze; l'operatore deve
registrare che il riavvio è avvenuto. Ripetere con porta listener diversa e
configurazioni invalide, verificando configurazione precedente e listener.
Power-cut richiede una prova fisica pianificata: non simulato come risultato reale.

Piano progressivo, senza invii o salvataggi sul dispositivo:

```sh
python3 -B tools/osc_benchmark.py --ip 192.168.1.4 --output .build/osc-benchmark-plan.json
```

36 scenari: 1/2/4/8 destinazioni su porte consecutive da 16888, 4/8/16 messaggi,
25/50/100 Hz. Ogni scenario è marcato NOT RUN, non produce misure sintetiche.
Estrarre intenzionalmente il payload dello scenario, validare sul firmware e
applicare prima della finestra, poi usare il ricevitore con report distinto.
Conservare METRICS, heap e ricezione; interrompere se memoria o rate peggiorano.
Nessuna conclusione sui limiti definitivi prima dei risultati hardware.
