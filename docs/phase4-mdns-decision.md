# Fase 4 — nome comune: evidenza e proposta di verifica

Il nome comune **non è attivo nella build Axio consegnata**. Non è stata
simulata un'elezione o una risoluzione `axio.local`. Gli hostname individuali
restano operativi. Questo documento applica il punto di stop richiesto dalla
specifica, senza trasformare un requisito mancante in una funzione completata.

## Verifica effettiva

Arduino ESP32 3.3.12, IDF 5.5.5, componente `espressif__mdns` 1.13.1.
Il probe isolato `tests/hardware/AxioMdnsProbe/` è stato compilato e caricato
sulle due XIAO. Ha letto le reti dal journal in sola lettura e configurato la
radio con storage RAM. Nessuna password è stata stampata. Entrambe le schede
sono state ripristinate con Axio Fase 4 dopo il test.

Query DNS multicast dal Mac sulla stessa LAN GL, non una ricerca nella tabella
locale del responder. Evidenza grezza ignorata da Git:
`captures/phase4-mdns-pair-01.json`.

| Caso osservato | Risposte A per axio.local |
|---|---|
| Solo AC:27:6E:7E:32:38 pubblica il delegato | 192.168.8.159 |
| Anche AC:27:6E:7C:31:28 pubblica il delegato | **192.168.8.159 e 192.168.8.184 contemporaneamente** |
| Ritiro esplicito del primo | 192.168.8.184 |
| Ritiro esplicito del secondo | Nessuna |

Gli hostname individuali sono rimasti `axio-ac276e7e3238.local` e
`axio-ac276e7c3128.local`, risolvibili ai rispettivi indirizzi. Il ritiro
manuale di un delegato non è un test di elezione o subentro automatico.
Non è stato verificato il completamento dell'intera matrice avvii simultanei,
perdita pacchetti, partizioni e cache client. Non si deduce da questa prova che
l'API sia inutilizzabile: si dimostra che **l'aggiunta del delegato non fornisce
l'arbitraggio esclusivo richiesto**.

## Proposta verificabile prima di un'integrazione

1. Prototipare il nome comune come record con proprietà esclusiva, probing,
   tie-break e difesa/ritiro del nome. Riferimento normativo:
   [RFC 6762, §§8–9](https://www.rfc-editor.org/rfc/rfc6762.html#section-8).
   Non affidare la proprietà alla sola cache peer o a `mdns_hostname_exists()`.
2. Valutare una modifica locale e versionata del componente mDNS, mantenendo
   **un solo responder**. Non modificare silenziosamente il framework installato
   e non aggiungere un secondo responder sulla stessa interfaccia. Prima di
   scegliere questa strada verificarne compilazione Arduino IDE e impatto
   flash/heap; non è già approvata o implementata una nuova dipendenza.
3. Sopra quel meccanismo, provare il coordinamento richiesto: MAC minore quando
   occorre eleggere, leader sano non preemptato, lease/presenza, rinuncia al
   conflitto e nuova elezione dopo perdita del leader. Nessun server centrale.
4. Ripetere su due e poi tre XIAO avvii simultanei, interruzioni, ritorno del
   leader, perdita pacchetti e ricongiungimento della rete; osservare i pacchetti
   DNS e l'HTTP, non soltanto i ruoli dichiarati dai tracker.
5. Confrontare heap, jitter IMU e UDP con la build attuale prima di integrare.
   Le indisponibilità e gli effetti delle cache durante il subentro devono
   essere misurati e dichiarati; nessuna garanzia assoluta è deducibile dal MAC.

Questa è una proposta di lavoro da valutare, non una promessa che una patch
specifica già disponibile risolva il problema. Il test isolato attuale copre
l'API delegata e il conflitto osservato, non il futuro protocollo.
