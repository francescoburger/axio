# Fase 4 — confronto HTTP con browser concorrente

9 novembre 2026, 15:46–15:49 Europe/Rome. Branch `test/phase4-endurance`,
base firmware `5e4790d`, firmware e configurazioni invariati. L'utente ha
confermato chiuse le altre schede Axio anche sugli altri dispositivi.

**Decisione successiva dell'utente:** limitazione HTTP con client concorrenti
accettata; mantenere il firmware attuale senza correzioni di concorrenza.
Il caso riprodotto è sonda + browser. Due browser distinti restano NOT RUN:
potrebbero incontrare lo stesso limite, ma il test non lo dimostra. La
limitazione accettata non blocca la prosecuzione del piano. Risultati grezzi
e classificazioni PASS/FAIL rimangono invariati.

**Problema riprodotto:** nessun errore HTTP prima/dopo il browser; nove errori
nella finestra con browser. Questo conferma l'effetto del browser concorrente
nel caso provato; non identifica da solo ogni componente responsabile o la
causa di ogni singolo errore della precedente prova di un'ora.

## Procedura

Tre finestre di 60 s, con lo stesso raccoglitore, timeout e configurazione:

1. A: sonda sola, target campioni 10 Hz, ricezione OSC, controlli USB e rete.
2. B: stessa sonda e browser Brave reale headless con profilo isolato sotto
   `.build/`. Due aperture Overview, due periodi Motion con telemetria Live,
   pagine visibili. Nessun dato fittizio, modifica della UI o comando hardware.
3. A2: browser chiuso, attesa 6 s per smaltire i timeout delle connessioni,
   poi stessa sonda per altri 60 s.

Il browser usa l'IP STA della principale, `http://192.168.1.15`; il test non
dipende dalla risoluzione mDNS per caricare la dashboard. I controlli mDNS della
sonda restano uguali nelle tre condizioni. GET/HEAD consentiti nel browser;
eventuali altri metodi sarebbero bloccati. Nessun metodo inatteso osservato.

## Risultati

| Finestra | Esito HTTP | Campioni validi / richieste | Errori HTTP | Campioni validi/s | Latenza p95 | Massimo tentativo |
|---|---|---:|---:|---:|---:|---:|
| A — sonda sola | PASS | 549 / 549 | 0 | 9,150 | 148,6 ms | 1,054 s |
| B — sonda + browser | FAIL | 278 / 287 | 9 | 4,633 | 1.053,5 ms | 3,010 s |
| A2 — recupero senza browser | PASS | 562 / 562 | 0 | 9,367 | 147,3 ms | 165,8 ms |

Percentile p95 nearest-rank calcolato su tutti i tentativi, errori inclusi.
Le latenze sono misure del processo host. La soglia di 2 s si applica alle
singole operazioni socket: un tentativo può durare più di 2 s sommando una
connessione lenta e la successiva attesa di risposta. Non è un timeout totale
di 2 s per l'intera richiesta.

Tutti i nove dettagli sono conservati: **otto timeout durante `connect()`**,
**uno durante l'attesa degli header**, dopo una connessione di circa 1 s.
Non si tratta di errori di decodifica dei campioni o di nove risposte HTTP
con codice di errore: quei tentativi non hanno ottenuto una risposta completa.

Browser reale: **PASS funzionale**, quattro navigazioni completate, Overview
con quattro card e Motion con sette card/stato Live. Registrate 268 richieste
browser, di cui 243 `/api/sample`, zero fallimenti di caricamento e zero metodi
inattesi. Le richieste del browser si aggiungono a quelle della sonda.

| Altra verifica | Esito | Risultato |
|---|---|---|
| IMU principale | PASS funzionale | A 99,757 Hz; B 99,718 Hz; A2 99,677 Hz; zero nuovi errori di lettura |
| Ricezione OSC | PASS formato/ricezione | 5.765 / 5.723 / 5.773 datagrammi validi nelle tre finestre, nessun errore di formato |
| Errori UDP firmware | PASS nella finestra osservata | Delta zero in tutte le fasi |
| Impostazioni OSC e reti | PASS | Confronto prima/dopo identico in ciascuna fase |
| Riavvii e connettività nei controlli | PASS | Nessun errore USB/reboot/STA registrato; uptime crescente anche tra le fasi |
| Secondaria | Stato atteso | BNO055 assente; non viene attribuito un PASS al suo sensore |

Nessuna equivalenza temporale, garanzia di 100 Hz esatti o misura delle perdite
dedotta dai conteggi UDP. Le finestre ricevitore/contatori non coincidono.

## Interpretazione

L'aggiunta controllata del browser rende riproducibile il degrado HTTP; alla
sua rimozione il servizio della sonda recupera. Nel codice attuale il server
elabora una connessione alla volta e richiede un backlog TCP di 1. Connessioni
lente/in attesa possono occupare il client attivo fino al timeout assoluto
di 5 s. Gli otto timeout di connessione sono coerenti con una contesa o
saturazione dell'ammissione delle connessioni; le tracce non osservano
direttamente la coda interna di lwIP o i pacchetti TCP sulla radio.

Prima della decisione di accettazione, la correzione proposta riguardava la gestione delle connessioni HTTP
concorrenti e dei client inattivi, preservando i budget del loop IMU, la
validazione delle richieste e la memoria limitata. Il browser è un uso normale
del prodotto. Aumentare soltanto il timeout della sonda non risolverebbe il
calo osservato delle richieste servite. Una futura correzione va verificata
prima con lo stesso confronto breve, poi con un'endurance mirata se giustificata.
Questa proposta è ora sospesa per scelta dell'utente: nessun intervento o nuova
prova sulla concorrenza HTTP è richiesto nel piano corrente.

## Strumenti ed evidenze

- `scripts/http_contention_probe.mjs`: orchestrazione A/B/A e browser reale.
- `tools/endurance_test.py`: aggiunti log incrementali di tutti gli errori e,
  con `--trace-http`, tempi di connessione, invio, header e corpo per ogni
  richiesta. Nessuna modifica ai timeout o ai criteri per far passare la prova.
- PASS: sei test host del raccoglitore, inclusi oltre dieci errori preservati
  nel sink e identificazione del timeout degli header dopo un connect riuscito.
- PASS: sintassi Node e `git diff --check`.
- Compilazione firmware: NOT RUN in questa sessione, nessun sorgente firmware
  modificato né upload eseguito.

Evidenze complete in `captures/phase4-http-contention-01/`: `report.json`,
`analysis.json`, `summary.md` e sottocartelle delle tre fasi. Ogni fase conserva
`initial.json`, `observations.jsonl`, `errors.jsonl`, `http-requests.jsonl` e il
rapporto finale. B conserva anche `browser/browser-report.json` e il log CDP
`browser/browser-network.jsonl`. La prova originale di un'ora resta invariata.

Nessuna calibrazione, DIAG RESET, Apply, modifica di reti, sleep o restart.
Nessun commit aggiuntivo, merge o push.
