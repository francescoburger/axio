# Correzioni aggiuntive UI e comando OSC numerico — 8 novembre 2026

Aggiornamento successivo: calibrazione via OSC e acknowledgement fisici
confermati funzionanti dall'utente; PASS manuale riportato in
[phase4-verification.md](phase4-verification.md). I NOT RUN qui sotto restano
il resoconto delle sole prove dell'agente in questa sessione storica.

Richieste dell'utente e screenshot `debug_requests/images/correzione_006.png`
e `correzione_007.png`. Questa revisione sostituisce il precedente requisito
di mantenere la navigazione alla larghezza del contenuto: ora entrambe le barre,
titolo e batteria devono seguire la finestra. Il contenuto delle pagine resta
centrato con le larghezze precedenti.

## Interventi

1. Navigazione estratta da `.shell`; header interno largo 100%, titolo sul lato
   sinistro e batteria sul lato destro, con padding responsive. Barra viola e
   navigazione coprono l'intero viewport.
2. Eliminati intestazione e stile di Message 01. L'editor singolo inizia dal
   campo Main Address; anteprima, gruppi e produzione OSC invariati.
3. Gruppo Import/Export nella sezione Configuration Files, con titolo accessibile,
   divisore, margine superiore di 20 px e padding di 16 px. Nessuna nuova funzione
   import/export: si tratta dei pulsanti già presenti.
4. Etichette estese a Enable Calibration Acknowledgement, Acknowledgement Address
   e Acknowledgement Destination. Chiavi JSON `ack`, ID DOM e valori persistenti
   restano compatibili. Le colonne dei campi si adattano anche a 320 px.
5. Corretto il parser del comando di calibrazione per il trigger numerico inviato
   da Pure Data, senza modificare il core di calibrazione.

## Causa della calibrazione non eseguita

L'utente usa `oscformat` e `netsend`, destinazione `10.166.156.41:16889`,
inviando il valore 1 al comando `/cal`. Il parser precedente richiedeva
esattamente indirizzo e type tag `,`, quindi nessun argomento. Un messaggio
con type tag `,f` o `,i` aggiunge quattro byte e veniva rifiutato prima di
chiamare il callback di calibrazione.

Per `/cal`, il comando vuoto occupa 12 byte; quello con un float 1.0 ne occupa
16. Il controllo precedente `n == padded(strlen(address)+1)+4` escludeva il
secondo caso. La lettura USB preliminare ha rilevato listener attivo, sette
comandi rifiutati e zero richieste di calibrazione: è coerente con il problema,
ma non è una cattura del datagramma originario dell'utente.

La configurazione letta in quel momento conteneva `/tare`: l'utente ha chiarito
di averlo ripristinato per una prova successiva. Non viene attribuito a questo
cambio il problema che aveva già osservato con `/cal`.

Il parser ora accetta:

- comando senza argomenti, mantenendo la compatibilità precedente;
- un solo int32 OSC, type tag `,i`;
- un solo float32 finito, type tag `,f`.

Il numero è un trigger: non rappresenta yaw o quaternion e non viene usato nei
calcoli. Ogni valore numerico valido ricevuto all'indirizzo configurato provoca
una richiesta; non viene introdotta una soglia speciale per il valore 1.
Indirizzi errati, padding non nullo, payload troncati, più argomenti, tipi non
supportati, NaN, infinito e bundle continuano a essere rifiutati. Non sono stati
aggiunti un parser OSC generale, una nuova schedulazione o comandi automatici.

L'acknowledgement mantiene la semantica esistente: viene prodotto solo se
abilitato e dopo il successo della calibrazione, sul percorso configurato,
con type tag `,i` e valore 1. Una calibrazione fallita non genera acknowledgement.
Gli alias degli indirizzi predefiniti restano invariati; un indirizzo
personalizzato come `/cal` va salvato nel campo Calibration Address.

## Verifiche software

| Verifica | Esito | Evidenza e limite |
|---|---|---|
| Suite completa | PASS | 90 test in 21,542 s, eseguiti con `python3 -B -m unittest discover -s tests -v` |
| Matematica e sorgenti congelati | PASS | 1.345 vettori baseline/core identici; baseline, fixture e vettori non modificati |
| Trigger float/int e messaggio vuoto | PASS | Configurazione `/cal`, acknowledgement `/calok`, passaggio attraverso `pollOscRuntime()`, lettura UDP, callback e verifica dei byte in uscita |
| Errori nel comando | PASS | Troncamenti a tutte le lunghezze, padding, valori non finiti, tipi errati, bundle; nessuna chiamata indesiderata al callback |
| Radio e limiti ricezione su host | PASS | Comando bloccato in AP, riabilitato in STA; pacchetto troppo grande scartato, fallimento calibrazione senza acknowledgement |
| Salvataggio dal browser | PASS | Browser con runtime C++: `/cal`, acknowledgement abilitato e `/calok` applicati tramite la UI e riletti nello snapshot reale del runtime |
| API Apply/Validate | PASS | Validate non modifica l'indirizzo; Apply restituisce la configurazione completa attesa |
| Responsive | PASS | Cinque pagine a 320, 430, 768, 1024 e 1440 CSS px, DPR 2; nessun overflow osservato |
| UI OSC | PASS | Message 01 assente; titolo Configuration Files e distanza dagli acknowledgement; nessuna abbreviazione ACK nel testo visibile |
| Runtime AP nel browser | PASS | Cinque pagine su host con radio simulata e nuovo frontend |
| Build ESP32 | PASS | 1.143.305 / 1.310.720 B flash; RAM statica 110.980 B; nessun warning emesso dalla build incrementale |

Le prove del ricevitore usano la produzione `OscRuntime.cpp`, con radio e UDP
simulati. Lo stub ora consente di accodare datagrammi a una porta e ispezionare
l'ultimo datagramma trasmesso; in precedenza `parsePacket()` restituiva sempre
zero. Questo colma la lacuna fra i test del solo engine e il percorso di ricezione
del runtime, senza presentarlo come misura hardware o esecuzione di Pure Data.

Build: `.build/debug-round2-build-01.log`. Catture simulate e report:
`captures/phase4-ui-{320,768,1024,1440}-round2/`,
`phase4-runtime-ui-430-round2/`, `phase4-runtime-ui-ap-430-round2/`.
Le catture precedenti sono conservate separatamente.

## Upload e verifiche reali

Upload sulla stessa XIAO principale già autorizzata, identificata da MAC
`AC:27:6E:7E:32:38`, porta `/dev/cu.usbmodem101`. Prima del caricamento sono
state lette nuovamente le impostazioni per conservare anche le modifiche fatte
dall'utente dopo l'upload precedente.

`bash scripts/upload.sh --port /dev/cu.usbmodem101`: PASS, verifica hash della
scrittura PASS, `EraseFlash=none`. Log `.build/debug-round2-upload-01.log`.
SHA-256 del binario:
`c960a30b0bf80bb0689ddbf02fe49fb4a2567bd83193c7f3f4a9ca70f87c3f1f`.
La stringa INFO.build è rimasta quella precedente: la verifica della nuova
revisione si basa sul binario scritto e sui byte dei tre asset HTTP, non sulla
data visualizzata da INFO.

| Verifica reale | Esito | Evidenza/limite |
|---|---|---|
| Upload, identità e avvio | PASS | Stesso MAC, IMU ready |
| Configurazione OSC completa | PASS | Identica a quella acquisita immediatamente prima dell'upload |
| Reti e hostname | PASS | Confronto prima/dopo identico, nessuna credenziale modificata |
| Riconnessione STA | PASS con limite | Fallback AP iniziale, poi STA a `10.166.156.41` senza interventi sulle reti; causa del ritardo iniziale non isolata |
| Asset installati | PASS | HTML/CSS/JS serviti dal dispositivo identici byte per byte ai sorgenti correnti |
| API in lettura | PASS | Sessione, snapshot, campione e batteria HTTP 200 |
| Browser reale mobile/desktop | PASS | Cinque schermate a 430 e 1440 CSS px, nuova disposizione e campi acknowledgement |
| Calibrazione numerica dalla patch Pure Data sulla XIAO | NOT RUN | Nessun comando fisico inviato dall'agente; contatore calibration_requests 0 nell'ultima lettura di controllo |
| Acknowledgement fisico | NOT RUN | Prova di generazione dei byte e routing su host; nessun comando fisico eseguito |
| Provisioning/API AP su radio reale | NOT RUN | Il Mac non è stato collegato all'AP né sono state forzate modifiche alle reti |

Evidenze: `captures/debug-round2-before-01.json`, `debug-round2-after-01.json`,
`debug-round2-reconnect-01.json`, `debug-round2-http-01.json`;
`captures/phase4-device-ui-430-round2/` e `phase4-device-ui-1440-round2/`.

L'upload conserva l'indirizzo che l'utente aveva ripristinato per la prova
(`/tare`) e gli altri valori correnti. Per ritestare `/cal 1`, impostare e
applicare `/cal` nel campo Calibration Address, oppure usare l'indirizzo
attualmente salvato. Il controllo browser su host verifica che il campo e
l'abilitazione acknowledgement siano effettivamente applicati.

Nessuna calibrazione, DIAG RESET o prova sleep/restart inviata dall'agente.
Il solo riavvio fisico è quello ordinario dell'upload. Nessuna nuova misura di
frequenza IMU o promessa di prestazione viene dedotta dai test numerici.

## Risultati intermedi conservati

- Lettura USB iniziale bloccata dal sandbox: report
  `captures/debug-round2-readonly-01.json`, hardware_executed false.
  Rilancio autorizzato: `debug-round2-readonly-02.json`, PASS.
- Primo controllo dopo l'upload: USB, IMU e conservazione impostazioni PASS,
  HTTP verso AP in timeout dalla connessione del Mac corrente. Il report
  `debug-round2-after-01.json` resta FAIL. Nessuna cancellazione o correzione
  retroattiva dell'evidenza.
- Successiva riconnessione STA e verifiche HTTP PASS, registrate in nuove catture.

## File modificati in questa revisione

- `firmware/Axio/web/index.html`, `style.css`, `app.js`.
- `firmware/Axio/src/web/WebAssets.h`, rigenerato dai sorgenti web.
- `firmware/Axio/src/osc/Codec.h`.
- `tests/stubs/runtime/WiFiUdp.h`, `tests/osc_runtime_harness.cpp`,
  `tests/osc_engine_harness.cpp`, `tests/test_osc_engine.py`,
  `tests/test_dashboard_runtime.py`.
- `scripts/phase4_browser_probe.mjs`, `README.md`, questo rapporto.

Nessuna nuova dipendenza. Baseline, fixture storica, NVS e reti preservate.
Nessun commit, merge o push.
