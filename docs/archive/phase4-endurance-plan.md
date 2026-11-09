# Fase 4 — primo collaudo: endurance autonoma

**Esito della prova di un'ora:** durata completata, risultato complessivo FAIL
per errori HTTP; IMU, OSC e conservazione delle impostazioni verificati.
Dettagli in [phase4-endurance-verification.md](phase4-endurance-verification.md).
Le voci NOT RUN più sotto descrivono lo stato precedente all'esecuzione.

L'utente ha successivamente accettato la limitazione HTTP riprodotta con
sonda + browser e richiesto di lasciare invariato il firmware. Nessuna nuova
endurance o correzione della concorrenza HTTP è richiesta per tale limite;
gli esiti originali restano conservati.

La «fase 1» richiesta in questa sessione è il primo punto del collaudo finale,
una prova prolungata di un'ora. Non riapre la Fase 1 storica del firmware.
Base salvata nel commit `5e4790d`; lavoro sul branch `test/phase4-endurance`.

## Modalità e consumo Codex

`scripts/start_endurance.py` avvia un processo locale separato dal terminale
del tool, protetto da `caffeinate -i` contro lo sleep per inattività del Mac.
Non cambia le impostazioni di alimentazione, non installa servizi e non usa
Codex, API AI, automazioni ChatGPT o polling dell'assistente. Dopo l'avvio
verificato il turno termina. L'ora è gestita interamente dal programma locale;
il rapporto viene scritto senza consumare token del modello. L'analisi con
Codex richiede un nuovo messaggio dell'utente al termine.

Mac acceso, coperchio aperto, USB e Wi-Fi stabili; lasciare libere le due porte
seriali e la porta UDP del ricevitore per tutta la prova. `caffeinate` non
impedisce lo sleep manuale o dovuto alla chiusura del coperchio.

## Operazioni

- Principale con IMU; secondaria senza IMU, stato `init_failed` previsto.
- GET `/api/sample` con target 10 Hz, senza raffiche di recupero. Frequenza
  effettiva, latenze, errori e campioni validi sono registrati separatamente.
  Timeout della sonda: 2 s per i campioni, 3 s per snapshot e richieste USB.
  Un timeout è un errore osservato della prova; da solo non ne identifica la causa.
- STATUS/METRICS via USB ogni circa 10 s su entrambe le schede. Nessun reset dei
  contatori: si usano differenze rispetto al valore precedente e iniziale.
- Snapshot HTTP e query A `axio.local` ogni circa 30 s; osservazione heap,
  blocco massimo libero, connettività, eventuali riavvii e proprietà del nome.
- Ricezione OSC passiva sul Mac dalla destinazione già configurata
  `192.168.1.4:16888`, con decodifica dei pacchetti della principale. La porta
  viene aperta senza condivisione; se occupata, la prova si ferma senza sottrarla
  ad altri ricevitori. Nessuna configurazione OSC viene applicata.
- INFO, configurazione OSC completa ed elenco reti letti all'inizio; confronto
  finale delle impostazioni. Nessuna calibrazione, DIAG RESET, Apply, upload,
  sleep o restart. Non vengono forzate disconnessioni durante questa prova.

La frequenza IMU deriva dai contatori USB, non dai datagrammi. Gli intervalli
UDP sono osservati dal processo host: non misurano direttamente jitter radio
o latenza end-to-end. Nessuna garanzia di zero perdite/duplicazioni.
Le misure di memoria descrivono l'andamento; non impongono soglie di capacità
arbitrarie e non certificano assenza di leak. I massimi diagnostici firmware
rimangono cumulativi, anche quando precedono l'inizio della prova.

## Risultati e criterio

Ogni esecuzione usa una cartella nuova sotto `captures/`, ignorata da Git:

- `status.json`: stato, PID, orari e statistiche aggiornati circa ogni 10 s.
- `observations.jsonl`: osservazioni USB/rete incrementali, non tutti i campioni
  HTTP o i pacchetti OSC; memoria del raccoglitore limitata e log contenuti.
- `initial.json`: stato/configurazioni prima della prova.
- `report.json` e `summary.md`: risultati automatici finali.

PASS richiede durata completata, nessun errore osservato dai controlli,
impostazioni conservate e ricezione di campioni HTTP e OSC validi. Errori
producono FAIL e dettagli; una prova interrotta non viene dichiarata completata.
Un PASS funzionale non promette 100 Hz esatti, 10 Hz HTTP effettivi o un limite
di prestazione universale. Non si interrompe automaticamente al primo errore:
si conserva l'osservazione fino al termine, salvo un guasto del raccoglitore.

Il programma si arresta al termine, chiude USB/socket e rilascia `caffeinate`.
Per fermarlo prima: leggere `pid` in `status.json` e inviare `kill -TERM PID`;
viene comunque tentata la scrittura del rapporto. Nessun riavvio automatico.

Avvio, dalla radice del repository, con l'IP corrente verificato prima:

```sh
python3 -B scripts/start_endurance.py \
  --primary /dev/cu.usbmodem2101 --secondary /dev/cu.usbmodem1101 \
  --osc-bind 192.168.1.4 --osc-port 16888 --seconds 3600
```

## Verifiche del raccoglitore

- PASS: quattro test host di aggregazione a memoria limitata, contatori
  azzerati, prevenzione di falsi PASS, campioni HTTP ripetuti/regressivi,
  filtro sorgente OSC e pacchetti malformati.
- PASS: prima prova fisica di 12 s, HTTP e OSC validi, configurazioni conservate,
  nessun errore osservato. Evidenza: `captures/phase4-endurance-smoke-01/`.
- PASS: avvio distaccato tramite `caffeinate`, rapporto scritto e processo
  terminato autonomamente nella seconda prova breve.
- FAIL della seconda finestra HTTP: un timeout di 2 s, 53 risposte valide su
  54 tentativi; 1.141 pacchetti OSC validi, impostazioni conservate. Evidenza
  originale in `captures/phase4-endurance-detached-smoke-01/`. Non è stata
  riscritta né attribuita una causa al timeout. Il criterio rimane invariato
  nella prova di un'ora, che continua a registrare anche dopo eventuali errori.
- L'endurance di un'ora resta NOT RUN finché il rapporto non ne certifica la
  durata completata. L'avvio del processo non costituisce un risultato PASS.

Nessun cambiamento firmware in questo branch; il binario sulle due XIAO è
quello verificato nel rapporto mDNS. I nuovi strumenti e il piano non sono
stati inclusi nel commit della base firmware e non sono stati pubblicati.
