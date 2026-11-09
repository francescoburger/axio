# Indagine mDNS dopo il recupero GL

9 novembre 2026. Stato iniziale: due owner di `axio.local`, peer vuoti.
Solo letture USB, GET HTTP, query DNS e cattura del traffico Axio su en0.
Nessun riavvio, upload, Apply, cambio del router o calibrazione in questa
indagine. Firmware di produzione invariato.

## Risultati verificati

| Controllo | Esito | Evidenza |
|---|---|---|
| Route multicast del Mac | PASS | 224.0.0.251 usa en0 |
| Query unicast A ai due responder | PASS trasporto; FAIL unicità | Ciascuno risponde con il proprio IP per axio.local |
| Query unicast dei nomi individuali | PASS | Entrambi rispondono correttamente |
| Query multicast con interfaccia en0 esplicita | Risposte intermittenti | Diverse richieste A senza risposta nella finestra; alcune risposte di servizio presenti |
| Query multicast dalla porta standard 5353 | Traffico presente | Visti pacchetti di entrambe le XIAO e una risposta A comune della principale |
| Recupero dei ruoli dopo tali query | FAIL | Entrambe rimangono leader, senza nuovi conflitti dichiarati |
| Recupero discovery applicativa | FAIL | Cache peer vuote nei controlli prima/dopo |

Evidenze: `captures/phase4-mdns-path-01.json` e
`captures/phase4-mdns-en0-01.pcap`. La cattura tcpdump dura circa 15 s ed è
filtrata su UDP 5353 con una delle due XIAO come sorgente/destinazione. Tutti
i pacchetti conservati riguardano questi endpoint; nessuna modifica ai filtri
del router o al resolver macOS. Listener diagnostico e tcpdump terminati.

## Il multicast non è completamente bloccato

La cattura mostra la secondaria inviare una query PTR multicast per
`_axio._udp.local`, la principale rispondere in multicast con PTR/SRV/TXT/A
del proprio servizio e la secondaria includere il PTR della principale come
known answer nella query successiva.

Questo dimostra che la secondaria ha appreso almeno quel record. Non dimostra
che ogni record sia consegnato in entrambe le direzioni o restituito completo
alle API asincrone; esclude però la spiegazione di un isolamento multicast
totale fra questi due dispositivi. Il contenuto di servizio corretto arriva
anche nelle risposte unicast dirette al Mac.

## Lacuna confermata nella politica di riconciliazione

`EntryElection::tick()` in stato Leader non richiede ulteriori lookup o
censimenti. Attende soltanto che il responder nativo cambi il nome da `axio`
per rilevare il conflitto e ritirarsi. `DiscoveryRuntime` continua il browse
ordinario, ma non usa i risultati per verificare l'esclusività del leader e
non pianifica annunci periodici del nome comune.

Una riproduzione locale con la policy di produzione, due leader e nome nativo
rimasto `axio` su entrambi, percorre dieci minuti di clock simulato: restano
due leader e la policy non emette alcuna azione di rete. Risultato in
`captures/phase4-mdns-leader-policy-01.json`. Non è una misura hardware di
durata, ma conferma l'assenza di una verifica periodica a questo livello.

La gestione nativa dei conflitti aveva funzionato nel precedente probe di
claim simultaneo su LAN già operativa. Non basta quindi estendere quel PASS
al caso in cui due schede si siano elette durante il recupero della rete e
il nome nativo non venga successivamente rinominato.

## Cosa rimane da isolare e correzione candidata

Resta da capire perché i risultati della discovery applicativa siano vuoti
mentre la cattura dimostra almeno un record peer appreso dal componente mDNS.
I contatori attuali non distinguono un risultato SDK vuoto da tutti i casi
di risultato parziale: serve osservare numero di record, indirizzi e metadati
consegnati all'adattatore, senza introdurre query bloccanti.

Una correzione candidata per i due leader è una riconciliazione periodica:
annunci del nome nativo mediante l'API pubblica già presente
`mdns_netif_action(..., MDNS_EVENT_ANNOUNCE_IP4)` e controllo esplicito del
conflitto, mantenendo un solo responder. Non è stata applicata né verificata
sull'hardware: annunci periodici da soli potrebbero non risolvere il problema
dei risultati di discovery o una consegna multicast asimmetrica. Va prima
provata sul caso riprodotto, misurandone anche costo e convergenza.

Fonti locali consultate: componente ufficiale Espressif mDNS 1.13.1 in
`.build/mdns-research/esp-protocols-mdns-v1.13.1/components/mdns/`, in particolare
`mdns_receive.c`, `mdns_querier.c`, `mdns_networking_lwip.c`, `mdns_pcb.c`, e
header della stessa versione della toolchain installata. Nessuna patch a
questi componenti o nuova dipendenza introdotta.

Esito: diagnosi avanzata e lacuna della policy confermata; recupero GL ancora
FAIL, correzione hardware NOT RUN. Nessun commit, merge o push.

Aggiornamento: correzione successivamente autorizzata, applicata e caricata
su entrambe le XIAO. Verifiche e limiti in
[phase4-mdns-reconciliation.md](phase4-mdns-reconciliation.md). Il presente
documento conserva le evidenze precedenti alla modifica.
