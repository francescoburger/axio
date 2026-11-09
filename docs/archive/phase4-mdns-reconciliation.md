# Riconciliazione mDNS dopo il recupero GL

9 novembre 2026. Correzione autorizzata con «Applica e verifica».

## Modifica

Il leader richiede un annuncio IPv4 nativo ogni 5 secondi tramite
`mdns_netif_action(..., MDNS_EVENT_ANNOUNCE_IP4)`. Il responder Espressif
rimane l'unico responsabile dei pacchetti e del rilevamento dei conflitti.
Quando il nome nativo cambia, la policy si ritira come prima. Gli annunci
non sono lookup del proprio nome e non introducono un secondo responder.

La scadenza usa sottrazione unsigned e riparte dall'istante corrente, senza
recuperare annunci arretrati. Le richieste condividono la coda delle azioni
di elezione; attendono la fine delle query asincrone pendenti. Un errore
dell'API incrementa un contatore e non causa retry a ogni tick. Gli annunci
sono limitati al leader con STA attiva e AP disattivo.

La diagnostica aggiunge `announcements`, `announce_failures`,
`result_records`, `empty_results`, `incomplete_results`, `accepted_results`.
Gli ultimi quattro contano solo il browse ordinario `_axio._udp` accettato
dal runtime, escludendo i risultati scartati durante il drain. Un record è
incompleto quando manca hostname, service, protocollo, TXT/lunghezze o lista
indirizzi. Il contatore `filtered` continua a identificare i record respinti
dalla validazione; `accepted_results` conta gli aggiornamenti della cache,
non il numero di dispositivi distinti.

## Verifiche

| Controllo | Esito | Evidenza |
|---|---|---|
| Suite completa finale | PASS | 110 test; `.build/mdns-reconcile-tests-final.log` |
| Policy dopo affinamento dei tempi del test | PASS | 8 test; `.build/mdns-reconcile-policy-tests-final.log` |
| Calcoli storici | PASS | 1345 vettori; baseline/core identici all'oracolo |
| Compilazione firmware | PASS | 1.148.283 B flash, 111.172 B RAM statica; `.build/mdns-reconcile-compile.log` |
| Upload secondaria e principale | PASS | Confronto esatto CONFIG GET/WIFI GET e identità prima/dopo |
| Due schede su GL, finestra 30 s | PASS | Un leader, discovery reciproca, nessuna configurazione modificata |
| Annunci effettivamente richiesti | PASS | Secondaria leader: 14 richieste riuscite, 0 errori al termine dell'osservazione |
| IMU principale nella finestra | PASS | 3004 acquisizioni in 30,116731 s: 99,7452 Hz, 0 errori lettura |
| Resolver nativo macOS | PASS | `dscacheutil`: axio.local → 192.168.8.184 |
| Nuova perdita/recupero GL, primo monitor | FAIL | Unico controllo fallito: discovery reciproca non ancora disponibile alla chiusura |
| Recupero GL, osservazione successiva | PASS | 60 s con un solo owner, discovery reciproca e HTTP comune 200; configurazioni preservate |
| Annunci sospesi offline | PASS | Contatori costanti mentre entrambe le schede erano senza STA |
| Monitor corretto | PASS software | Attende anche entrambi i peer e DNS coerente con l'owner; 3 test specifici |
| Ricongiungimento di due leader già attivi | PASS successivo, strumentato | Partizione RX mDNS e riapertura su entrambe; [rapporto dedicato](phase4-mdns-partition-verification.md) |

Artefatti ignorati da Git:

- `captures/phase4-mdns-reconcile-secondary-install-02.json`;
- `captures/phase4-mdns-reconcile-primary-install-01.json`;
- `captures/phase4-mdns-reconcile-both-01.json`;
- `captures/phase4-mdns-reconcile-outage-gl-01/report.json`;
- `captures/phase4-mdns-reconcile-outage-gl-01/post-recovery-review.json`;
- `captures/phase4-mdns-reconcile-post-outage-02.json`.

Hash binario e copie delle configurazioni sono nei report di installazione.
Il primo tentativo di upload è stato negato dalla sandbox prima di accedere
alla seriale, senza modifica della scheda. Anche la prima suite ha incontrato
il divieto di socket locali della sandbox: ripetuta fuori sandbox, PASS.

Il probe intermedio con la sola secondaria aggiornata è conservato in
`captures/phase4-mdns-reconcile-mixed-01.json`: FAIL nei controlli DNS finali
per assenza di risposta nella breve finestra multicast. Entrambi i report
mostrano che le risposte alle query multicast brevi sono intermittenti.
Il PASS finale del probe a due firmware aggiornati non equivale a una prova
di affidabilità di ogni singola query DNS.

## Ripetizione dell'interruzione GL

L'utente ha spento GL per circa 30 secondi e l'ha riaccesa. La raccolta USB
ha osservato entrambe le disconnessioni e il fallback AP, poi il rientro
della principale e, circa 27 secondi dopo, quello della secondaria.
Nessun reboot. La principale è diventata leader; la secondaria è rimasta
standby. Nessun campione ha riportato due owner contemporanei.

Il primo monitor ha chiuso la finestra circa 15 secondi dopo il rientro
della secondaria: DNS/HTTP e OSC erano già ripresi, ma il prossimo browse
della principale, pianificato ogni 30..35 secondi, non aveva ancora
aggiornato la cache. Il FAIL originale è conservato. Il monitor ora attende
anche la presenza dei peer su entrambe le schede e verifica che l'IP DNS
corrisponda al singolo owner prima di avviare la finestra stabile.
La modifica al monitor è verificata con test locali; nessuna seconda
interruzione fisica è stata richiesta per provarla.

Nella successiva osservazione di 60,104004 s la discovery era reciproca a
ogni campione, con un solo owner e HTTP comune 200. La principale ha
acquisito 5991 campioni (99,6772 Hz), senza errori. La raccolta offline
mostra 10130 acquisizioni e zero nuovi errori lettura; l'engine ha ripreso
l'invio con 1185 datagrammi nella finestra del primo monitor. Gli annunci
sono rimasti sospesi durante la perdita della STA, senza errori API al
rientro. La revisione confronta le configurazioni dall'inizio dell'outage
alla fine del follow-up: identiche.

La discovery consegna record completi e accettati dopo il recupero, ma non
è isolata la causa dei precedenti risultati vuoti. Questa ripetizione
dimostra il recupero osservato con il nuovo firmware; poiché non si sono
formati due leader, non prova direttamente la guarigione di una coppia già
in conflitto. Il contatore annunci prova l'accettazione dell'API, non la
consegna di ogni pacchetto a ogni peer. Il precedente FAIL del firmware
vecchio e il FAIL iniziale del nuovo monitor restano conservati.

Aggiornamento successivo: il caso di due leader già attivi è stato riprodotto
con un blocco RX mDNS temporaneo su una copia strumentata del firmware. Il
ricongiungimento è PASS nell'ambito provato; il binario di produzione è stato
ripristinato su entrambe. [Metodo, risultati e limiti](phase4-mdns-partition-verification.md).

Stato finale: principale 192.168.8.159 leader, IMU ready; secondaria
192.168.8.184 standby, IMU init_failed atteso. Sonde terminate.

Reti GL, destinazioni OSC, tara e parametri IMU preservati. Secondaria senza
IMU: `init_failed` atteso. Destinazione OSC ancora 192.168.1.4:16888;
ricezione OSC sul Mac GL NOT RUN. Il limite HTTP con client concorrenti
accettato dall'utente non è stato modificato. Nessun commit o push.
