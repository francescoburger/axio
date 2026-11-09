# Fase 4 — elezione del punto d'accesso comune

**Aggiornamento successivo:** il riavvio coordinato su TIM è PASS; dopo una
perdita/ritorno della rete GL sono stati osservati due owner del nome comune
e discovery finale assente sul firmware precedente. Dopo la correzione con
annunci nativi periodici, una nuova interruzione GL e il follow-up mostrano
un solo owner e discovery reciproca. In questa ripetizione non si sono
formati due leader: il ricongiungimento di una coppia già in conflitto resta
NOT RUN. [Correzione e limiti](phase4-mdns-reconciliation.md),
[rapporto rete precedente](phase4-network-verification.md).

9 novembre 2026. Integrazione installata e verificata sulle due XIAO, con
subentro automatico in entrambe le direzioni. Le prove del prototipo restano
separate da quelle del firmware Axio completo.

## Soluzione

Un solo responder Espressif mDNS 1.13.1, già incluso nella toolchain bloccata.
Nessuna nuova dipendenza, patch al framework installato, copia di componenti
terzi nel firmware o secondo socket responder sulla porta 5353.

Il leader usa `axio` come hostname nativo del responder: probing, conflitto e
rinomina sono gestiti dal componente esistente. Il suo hostname individuale
rimane un delegato con l'IP STA; `_axio._udp` e `_http._tcp` continuano a usare
quell'identità. Il servizio ausiliario `_axio-entry._udp` resta associato al
nome nativo e lo include nei probe e nei goodbye. Gli standby usano il normale
hostname individuale come nome nativo. Questo inverte la prova storica fallita:
**il nome comune non è un delegato privo di arbitraggio**.

La politica consulta la rete con query asincrone, senza attese nel loop e senza
usare la cache discovery a 90 s per decidere la proprietà. Dopo due ricerche A
senza risposta, censisce i candidati del servizio ausiliario e dà precedenza al
MAC minore osservato; controlla ancora il nome prima di reclamarlo. Un owner
presente viene mantenuto. Dopo il cambio di ruolo il firmware osserva per 3 s
il nome nativo prima di dichiarare `leader`; se il responder rinomina `axio`,
si ritira e riparte come standby. Perdita STA, AP e cambio IP annullano lo stato.
Non è un protocollo di consenso su una rete con partizioni o perdita arbitraria.

Una sola query è attiva alla volta, condividendo il budget con la discovery
ordinaria: lookup A 600 ms, censimento 1200 ms, polling risultati con attesa 0.
Le ricerche dell'owner presente sono distanziate di 3 s. Le scadenze della
politica usano sottrazioni unsigned e non eseguono catch-up.

Le API espongono `network.entry_state` e il nome comune quando posseduto o
osservato di recente. Host e Origin `axio.local` sono ammessi sul dispositivo
che dichiara la proprietà; token, controlli Origin e hostname/IP individuali
restano attivi. Nessun mDNS viene abilitato in AP.

Riferimenti consultati, versione esatta:
[sorgenti Espressif mDNS 1.13.1](https://github.com/espressif/esp-protocols/tree/mdns-v1.13.1/components/mdns),
in particolare `mdns_receive.c`, `mdns_send.c` e `mdns_responder.c`.
Il codice locale usa soltanto API pubbliche. Le API dei servizi delegati sono
chiamate con hostname e istanza espliciti anche per gli aggiornamenti TXT/porta.

## Verifiche software

| Verifica | Esito | Evidenza |
|---|---|---|
| Suite completa | PASS | 98 test, `.build/entry-tests-final-98.log` |
| Matematica e sorgenti congelati | PASS | 1.345 vettori baseline/core invariati nella stessa suite |
| Elezione e runtime | PASS | Owner presente, candidato minore, conferma prima del claim, rinomina/conflitto, errore alias, uscita offline, cambio IP, wrap unsigned |
| HTTP reale su host | PASS | Host/Origin/token sul nome comune, rifiuto prima dell'elezione e dopo il conflitto, accesso individuale conservato |
| Parser della sonda DNS | PASS | Compressione, cache-flush, goodbye, classe diversa, troncamenti e loop di compressione |
| Build ESP32 | PASS | 1.147.785 / 1.310.720 B flash; RAM statica 111.124 B, `.build/entry-integrated-build-03.log` |

Nessun nuovo warning Axio nell'ultima build incrementale. Resta il warning
esterno OSC implicit fallthrough osservato nella ricompilazione precedente.
Il primo lancio della suite dentro il sandbox è stato bloccato sui socket
localhost; i rilanci autorizzati fuori sandbox sono PASS, senza modificare i
test per aggirare il blocco.

## Prototipo fisico isolato

Sketch `tests/hardware/AxioMdnsOwnershipProbe/`, nessun accesso IMU. Reti lette
dal journal in sola lettura e credenziali radio applicate soltanto in RAM.
Dispositivi sulla LAN TIM: principale AC:27:6E:7E:32:38 / 192.168.1.15,
secondario AC:27:6E:7C:31:28 / 192.168.1.16, senza IMU.

| Caso | Esito | Osservazione DNS reale |
|---|---|---|
| Nessun owner | PASS | Nessun A per `axio.local`, entrambi i nomi individuali risolvibili |
| Principale owner | PASS | Un A, 192.168.1.15; nomi individuali invariati |
| Entrambi richiedono il nome | PASS nel caso provato | Un A, 192.168.1.16; il principale viene rinominato `axio-2` dal responder nativo |
| Ritiro del perdente | PASS | Resta un A; entrambi i nomi individuali operativi |
| Ritiro di entrambi | PASS | Nessun A comune |
| Nuovo owner | PASS | Un A, 192.168.1.15 |
| Ripristino Axio su entrambe | PASS | Upload con hash verificato, nessun erase |

Evidenza: `captures/phase4-entry-ownership-04.json`. Il confronto JSON grezzo
del secondario risulta falso perché il vecchio firmware rispondeva con
`tare_address`, mentre il firmware aggiornato restituisce `calibration_address`.
L'indirizzo `/tare`, gli altri valori OSC e tutte le reti sono invariati:
verifica normalizzata separata in
`captures/phase4-entry-ownership-config-review-01.json`. L'evidenza originale
non è stata riscritta. La nuova rete TIM è stata aggiunta dall'utente via AP,
conservando quella precedente, prima del collaudo.

La prima prova singola è terminata FAIL per mancata STA nella finestra di 45 s;
Axio è stato ripristinato con impostazioni identiche. La seconda, con finestra
di osservazione più lunga e parametri radio allineati alla produzione, PASS:
`captures/phase4-entry-ownership-single-02.json`. Non si attribuisce una causa
definitiva al ritardo di connessione. I tentativi di coppia precedenti agli
upload hanno rilevato la secondaria ancora in AP e si sono fermati senza flash.

## Collaudo firmware integrato e limiti

Artefatto installato sulle due schede, SHA-256:
`18d37d5dc953c90450a1a7eb2726635309a0396f228e1b321fba8823a287e7f5`.
Gli upload verificano l'hash della scrittura e usano `EraseFlash=none`.
La configurazione viene riletta immediatamente prima di ogni upload e confrontata
dopo il riavvio: reti, OSC completo, ID e hostname individuale conservati.

| Verifica integrata reale | Esito | Evidenza e limite |
|---|---|---|
| Upload e conservazione principale/secondaria | PASS | `captures/phase4-entry-install-{primary,secondary}-01.json` |
| Nome comune con la sola principale aggiornata | PASS | Un A 192.168.1.15, Host HTTP comune 200; `phase4-entry-integrated-single-01.json` |
| Secondaria aggiornata entra mentre la principale è leader | PASS | Il MAC minore rimane standby, entrambi raggiungibili; `phase4-entry-integrated-pair-01.json` |
| Principale assente durante il suo upload | PASS nel percorso provato | Secondaria assume il nome, un A 192.168.1.16 e HTTP 200; osservazione di circa 95 s in `phase4-entry-failover-secondary-01.json` |
| Ritorno della principale | PASS | Riconnessione, IMU ready e impostazioni preservate; `phase4-entry-restart-primary-01.json`; resta standby fino alla prova successiva |
| Secondaria assente durante il suo upload | PASS nel percorso provato | Principale assume il nome, un A 192.168.1.15 e HTTP 200; circa 95 s in `phase4-entry-failover-primary-01.json` |
| Ritorno del MAC minore | PASS | La secondaria rimane standby; `phase4-entry-restart-secondary-01.json` e `phase4-entry-integrated-final-01.json` |
| Discovery reciproca | PASS | Snapshot finali vedono ID/hardware/IP dell'altro tracker, anche mentre il leader usa il nome individuale delegato |
| Cache/resolver di macOS al primo subentro | PASS nel caso provato | `getaddrinfo` e HTTP verso il nome risolvono al secondario; `phase4-entry-macos-secondary-01.json` |
| Browser sul nome comune | PASS | Cinque pagine live, 430 CSS px/DPR 2, nessun salvataggio, anche dopo il subentro inverso; `captures/phase4-device-ui-430-entry-point/` e `captures/phase4-device-ui-430-entry-point-final/` |

I nomi dei report nella tabella sono sotto `captures/`. Le due prove di assenza
usano un ricaricamento dello stesso firmware, che interrompe realmente radio e
responder. Non sono prove di partizione di rete né di sleep. Nei campioni DNS
non sono stati osservati due A contemporanei. Tra la prima query senza risposta
e la prima risposta del nuovo owner sono trascorsi circa **9,1 s** verso la
secondaria e **7,2 s** verso la principale, con campionamento intorno a 1,8–2 s.
Non sono tempi esatti dal reset né limiti massimi garantiti; il passaggio ha
un'interruzione osservabile.

La seconda XIAO è aggiornata anche con le correzioni debug precedenti.
`init_failed`/campione non valido restano lo stato previsto senza BNO055:
il dispositivo può comunque servire la dashboard e il punto d'accesso comune.

### Carico web, IMU e memoria

`captures/phase4-entry-load-01.json`, sonda in sola lettura: **PASS** tutti i
controlli funzionali e di conservazione. Telemetria web: **200/200** risposte
valide, zero errori HTTP nella finestra di circa 20 s, massimo osservato 0,160 s.
IMU misurata separatamente dai contatori USB: **99,7462 Hz** sotto queste
richieste, zero errori di lettura; finestra precedente di circa 5 s: 99,8541 Hz.
Nella finestra di 95 s del subentro verso la principale: 99,9066 Hz, zero errori
di lettura. Nessuna equivalenza temporale o garanzia di 100 Hz esatti.

Alla fine della prova web: heap libero **105.692 B**, minimo cumulativo
**93.180 B**, blocco massimo **94.196 B**; massimo intervallo IMU cumulativo
**20.837 µs**. Non sono una prova di endurance, un massimo esatto della sola
finestra o una certificazione di assenza di frammentazione.
UDP accettati localmente: 96,3016 datagrammi/s nella finestra web, delta errori
zero. Il contatore errori UDP cumulativo era già 7 e rimane 7: non viene
azzerrato né presentato come zero. Ricezione/consegna OSC LAN sulla nuova
revisione **NOT RUN**; questi contatori non dimostrano consegna o assenza di
perdite, e il rate OSC non è usato per stimare il rate IMU.

Restano NOT RUN: avvii simultanei integrati, terza XIAO, partizioni e ricongiungimento della rete, perdita
pacchetti controllata, comportamento di tutte le cache/browser, stabilità
prolungata. Il meccanismo non promette unicità istantanea durante ogni conflitto,
subentro senza interruzioni o accesso universale su reti che filtrano multicast.
L'assenza BNO055 del secondario è attesa, non un PASS dell'IMU.

Stato finale: principale 192.168.1.15 leader, IMU ready; secondaria 192.168.1.16
standby, IMU init_failed atteso. Entrambe aggiornate e sulla rete TIM.
La sonda riutilizzabile in sola lettura è `tools/entry_point_probe.py`; richiede
il pyserial già presente nel venv e un percorso di report nuovo:

```sh
.venv/bin/python -B tools/entry_point_probe.py \
  --port /dev/cu.usbmodem2101 --port /dev/cu.usbmodem1101 \
  --duration 25 --report captures/entry-check-nuovo.json
```

Nessuna calibrazione, DIAG RESET o prova distruttiva. NVS e reti preservate.
Nessun commit, merge o push.
