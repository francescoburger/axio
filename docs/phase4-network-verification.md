# Fase 4 — avvio coordinato e recupero rete

9 novembre 2026. Firmware invariato, nessun upload, Apply, calibrazione o
DIAG RESET inviati. Principale AC:27:6E:7E:32:38 con BNO055, secondaria
AC:27:6E:7C:31:28 senza IMU. Prove nel branch `test/phase4-endurance`.

## Avvio coordinato su TIM — PASS

Due richieste Restart alle API esistenti, confermate da HTTP 200 e ACK su
entrambe, inviate con barriera fra due connessioni già stabilite. Scarto fra
gli invii: **0,169 ms**. Nuovi uptime osservati su entrambe; differenza stimata
fra le origini dei boot **37,550 ms**, inclusa l'incertezza delle letture USB.
È un riavvio software coordinato, non una misura di accensione elettrica
perfettamente simultanea.

| Verifica | Esito |
|---|---|
| Entrambe riavviate e tornate in STA, AP chiuso | PASS |
| Configurazioni OSC complete e reti identiche prima/dopo | PASS |
| Principale IMU ready, secondaria init_failed atteso | PASS sul principale; IMU secondaria non verificata |
| Discovery reciproca e un solo owner axio.local | PASS |
| Nessuna risposta A multipla nei campioni | PASS nei campioni |
| HTTP individuale e nome comune | PASS, HTTP 200 |
| OSC ripreso sul Mac | PASS: 1.183 datagrammi dopo l'inizio della finestra stabile, zero errori di formato |

Evidenze in `captures/phase4-network-restart-01/`. Il campo originale
`estimated_boot_skew_ms` in `report.json` considerava una risposta ancora
precedente al riavvio effettivo, ricevuta subito dopo l'ACK. L'estrazione è
stata corretta selezionando uptime nuovi su entrambe le schede; risultato in
`boot-timing-review.json`, evidenza originale conservata. Due test host PASS
coprono la selezione dei nuovi boot e l'assenza di un boot rilevabile.

## Passaggio volontario a GL

Dopo la prima prova, l'utente ha acceso GL, rimosso TIM dalle preferenze Axio
e confermato entrambe le XIAO e il Mac su GL. Questo è un cambiamento volontario
della base di configurazione, non una perdita dovuta al riavvio.
Il monitor successivo acquisisce una nuova base e non ripristina TIM.

Mac su GL: 192.168.8.164. La destinazione OSC della principale resta
192.168.1.4:16888. Per preservarla, il monitor verifica la ripresa del runtime
OSC dai contatori del dispositivo; ricezione fisica OSC del Mac su GL con
questa destinazione è NOT RUN. La ricezione reale su TIM è distinta e già
verificata. La seconda USB si è rinumerata da usbmodem1101 a usbmodem101;
identità confermata dal serial number/MAC, non dedotta dal nome della porta.
Il primo preflight GL con la vecchia porta è conservato come errore dello
strumento in `captures/phase4-gl-preflight-01.json`.

## Interruzione e ritorno GL — FAIL del recupero mDNS

Monitor autonomo `tools/network_recovery_probe.py --mode outage`, con letture
USB locali e caffeinate temporaneo. L'utente controlla lo spegnimento di GL
per circa 30 s e il successivo ripristino. Nessun comando di rete inviato
dall'agente; nessun collegamento necessario a Codex durante l'interruzione.
Risultati in `captures/phase4-network-outage-gl-01/`.

| Verifica | Esito | Evidenza |
|---|---|---|
| Perdita STA e fallback AP su entrambe | PASS | Osservati via USB durante lo spegnimento GL |
| IMU principale durante assenza STA | PASS | 13.790 acquisizioni fra due campioni offline, zero nuovi errori lettura |
| Sospensione OSC senza STA | PASS | Listener disattivo, datagrammi accettati invariati fra i campioni offline |
| Ritorno STA e chiusura AP | PASS | Entrambe su GL; nessun riavvio rilevato |
| Reti e configurazioni OSC | PASS | Nuova base GL identica al follow-up finale su entrambe |
| Ripresa runtime OSC | PASS contatori | Listener attivo e +9.558 datagrammi accettati dopo il periodo offline |
| Ricezione fisica OSC sul Mac GL | NOT RUN | Destinazione salvata ancora 192.168.1.4; non modificata |
| Unico owner axio.local dopo il ripristino | FAIL | Entrambe si dichiarano leader; interrogate direttamente, entrambe rispondono al nome comune con il proprio IP |
| Discovery reciproca finale | FAIL | Peer assenti negli snapshot del follow-up |

Primo offline di entrambe: circa 17:48:50 Europe/Rome. Secondaria nuovamente
STA alle 17:51:02 circa, principale alle 17:51:11 circa. Sono tempi delle
osservazioni: includono spegnimento, avvio del router e tentativi/backoff delle
schede; non sono una misura esatta della latenza dal ritorno della radio GL.

Il monitor iniziale termina FAIL al budget di recupero di 240 s: entrambe sono
già in STA, ma manca una risoluzione comune stabile osservabile dal Mac.
L'utente conferma poi il Mac nuovamente su GL, IP verificato 192.168.8.164.
Il follow-up in sola lettura e le query DNS dirette confermano:

- 192.168.8.159 risponde A per axio.local con 192.168.8.159;
- 192.168.8.184 risponde A per axio.local con 192.168.8.184;
- entrambe rispondono HTTP 200 usando Host axio.local;
- gli hostname individuali rispondono correttamente alle query dirette;
- le query multicast dal Mac sono intermittenti, spesso con nessun A.

Evidenze aggiuntive: `captures/phase4-gl-recovery-followup-01.json`,
`captures/phase4-gl-direct-dns-01.json` e `followup-review.json` nella cartella
dell'outage. Il vecchio criterio della sonda follow-up dava PASS perché una
singola risposta DNS osservata non esclude due owner effettivi. L'evidenza
originale è conservata; la sonda ora controlla anche il numero di owner
dichiarati. Test di regressione PASS con due owner e un solo A osservato.

Il caso GL resta aperto: va distinta la consegna multicast sulla LAN dalla
riconciliazione dei ruoli mDNS quando torna la connettività. Le osservazioni
non isolano la causa fra rete/router e firmware. Il riavvio coordinato su TIM
era PASS; non viene esteso al recupero GL. Questa anomalia è distinta dal
limite HTTP con client concorrenti già accettato dall'utente.

Indagine successiva senza modifiche firmware: cattura di traffico multicast
fra le schede e conferma dell'assenza di riconciliazione periodica nella
policy Leader. Dettagli, limiti e correzione candidata in
[phase4-mdns-investigation.md](phase4-mdns-investigation.md).

Nessuna correzione firmware, modifica al router da parte dell'agente o nuova
cancellazione di reti eseguita. Monitor e sonde terminati; nessun commit o push.

Aggiornamento successivo: [riconciliazione mDNS applicata e verifiche del nuovo
firmware](phase4-mdns-reconciliation.md). Le osservazioni FAIL sopra riportate
restano riferite al firmware precedente.
