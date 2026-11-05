# Protocollo benchmark — predisposto, hardware NOT RUN

Non sono state misurate prestazioni della XIAO in Fase 0. Il test UDP locale
verifica solo il funzionamento del ricevitore; i dati sintetici dei test non
sono benchmark. Nessuna soglia o limite di destinazioni/messaggi viene deciso.

## Grandezze distinte

| Grandezza | Definizione e osservabilità nella V1.1 |
| --- | --- |
| Pacchetti OSC ricevuti/s | Datagrammi UDP decodificati come OSC per sorgente, timestamp host |
| Messaggi OSC | Quattro messaggi in ogni bundle V1.1: non quattro acquisizioni o pacchetti |
| Frequenza acquisizione IMU | NOT RUN / non osservabile direttamente dai pacchetti V1.1 |
| Frequenza interna BNO055 | Distinta dalle chiamate di lettura del firmware; non misurata |
| Jitter di ricezione | Variazione degli intervalli tra datagrammi osservati sul Mac |
| Jitter acquisizione | Variazione dei tempi di lettura IMU; non esposta dalla V1.1 |
| Latenza end to end | Non ricavabile senza timestamp firmware e sincronizzazione/strumenti |

Il firmware tenta un ciclo quando sono trascorsi almeno 10 ms, senza catch-up.
Una chiamata `bno_loop()` completata con `imuok=true` tenta un bundle, ma rete,
I²C e portale introducono ritardi; ricezione non implica campioni IMU nuovi.
La V1.1 non trasmette numero progressivo, timestamp del campione, durata I²C o
contatore di letture. Il timetag immediato OSC non sostituisce questi dati.

## Preparazione sulla XIAO (tutto NOT RUN)

1. Verificare alimentazione, massa comune, livelli I²C del breakout, GPIO 5/6,
   indirizzo 0x29, cristallo esterno e orientamento P0. Non dedurre GPIO dalle
   etichette Dn. Registrare modello/revisione del breakout e collegamenti.
2. Compilare con la toolchain bloccata; registrare hash sorgenti, versioni,
   opzioni, memoria e commit, quando disponibile. Non modificare la baseline.
3. Caricare sulla porta USB scelta dall'utente; leggere seriale a 115200 e
   verificare inizializzazione BNO055. Questa fase richiede hardware reale.
4. Nel portale configurare IP LAN del Mac, porta 16888 e ID. Registrare firmware,
   tipo rete/AP, distanza, RSSI se disponibile da strumenti esterni, canale,
   condizioni di alimentazione e traffico. Non aggiungere telemetria ora.
5. Lasciare stabilizzare Wi-Fi e fusione del sensore prima della finestra di misura.
   Annotare la durata di stabilizzazione effettiva e ogni `/tare` inviato.

## Cattura ripetibile

Eseguire dalla radice del repository, con nomi nuovi per ogni cattura:

```sh
python3 -B tools/osc_receiver.py --bind 0.0.0.0 --port 16888 --duration 60 --output captures/baseline-idle-01.jsonl
python3 -B tools/analyze_capture.py captures/baseline-idle-01.jsonl
```

Per benchmark, dedicare la porta al dispositivo e chiudere altri ricevitori.
Ripetere almeno tre finestre della stessa durata: sensore fermo, movimento
controllato, interazione col portale; annotare eventuale tara. Disconnessione e
riconnessione sono prove separate, con tempi e condizioni espliciti. Usare il
profilo storico di una destinazione e quattro messaggi; i futuri confronti di
carico verranno progettati dopo queste misure. Registrare carico del Mac e
versione OS: la scrittura JSONL con flush per pacchetto e il scheduling Python
possono influire sulla ricezione. Non assumere che tutti i pacchetti siano arrivati.

Ogni riga contiene `packet_id` locale alla cattura, `received_unix_ns`,
`received_monotonic_ns`, `source_ip`, `source_port`, `size_bytes`, `messages` o
`error` e payload grezzo base64. I timestamp vengono letti subito dopo `recvfrom`,
prima del parsing: misurano quando il processo riceve dal socket, non l'arrivo
radio o il tempo kernel. Il timestamp UNIX consente correlazione approssimativa;
quello monotono evita correzioni dell'orologio nei calcoli degli intervalli.

## Calcoli e interpretazione

L'analizzatore separa ogni coppia IP/porta sorgente e considera l'ordine della
cattura. Non unire riavvii o sorgenti diverse senza analisi esplicita.

- `received_udp_hz = (N-1)/(t_ultimo-t_primo)` per tutti i datagrammi osservati.
- `received_osc_hz` usa la stessa formula sui soli datagrammi decodificati OSC.
  Meno di due timestamp distinti produce `null`, non zero o infinito.
- `osc_messages` conta i messaggi dentro i pacchetti; nella V1.1 sono normalmente
  4N. Non è la frequenza dei pacchetti e non misura l'IMU.
- Intervalli UDP `dt[i] = t[i]-t[i-1]` in ms: media, mediana, p95 nearest-rank,
  massimo e deviazione standard della popolazione. Quest'ultima è la definizione
  di jitter usata qui. Include anche datagrammi invalidi: verificare il relativo
  contatore e, se presenti, analizzare una cattura dedicata valida.
- La finestra osservata è dal primo all'ultimo pacchetto, non tutti i 60 secondi
  richiesti. Non include silenzi iniziali/finali. Cattura vuota produce `[]`:
  nessun rate stimabile. Per misure a finestra fissa servono anche i confini della
  finestra annotati dall'operatore, includendo esplicitamente quei silenzi.
- Controllare quattro indirizzi attesi, type tag `,fff`, ID e contatori invalidi.
  Annotare gap e burst. Un gap non dimostra perdita: potrebbe essere una pausa
  di invio. Non è possibile distinguere perdite, duplicati o riordino senza sequence
  ID del mittente; `packet_id` è solo un indice del ricevitore.
- `imu_hz` resta `null`: non ricavarlo da `received_osc_hz`, nemmeno se circa 100.

Misurare in futuro acquisizione e jitter IMU richiederà una build separata
strumentata, autorizzata in una fase successiva: contatori e timestamp attorno
alle letture oppure un GPIO osservato da logic analyzer, distinguendo letture
dal tasso di aggiornamento interno. Tale build non deve sostituire la baseline
e va confrontata per l'overhead introdotto. Non è stata implementata in Fase 0.

## Scheda risultati da compilare

| Verifica | Stato attuale | Risultato |
| --- | --- | --- |
| Ricezione OSC dalla XIAO, rate pacchetti | NOT RUN | Nessuna cattura hardware |
| Jitter di ricezione dalla XIAO | NOT RUN | Nessuna cattura hardware |
| Qualità valori e tara quaternion/yaw | NOT RUN | Verifica orientamento e movimento richiesta |
| Rate/jitter acquisizione firmware | NOT RUN | V1.1 non li espone |
| Rate interno BNO055 | NOT RUN | Nessuna misura hardware |
| Stabilità con portale e rete | NOT RUN | Da eseguire |

Per ogni prova futura riportare PASS/FAIL/NOT RUN, durata e condizioni, file di
cattura, dati grezzi, definizione delle metriche, statistiche e limiti osservativi.
Il PASS di un test funzionale non equivale al raggiungimento di un obiettivo
prestazionale ancora da definire.
