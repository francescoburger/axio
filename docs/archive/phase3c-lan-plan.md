# Piano LAN 3C — piano storico

Le classificazioni NOT RUN sotto descrivono la fine di 3C. L'esecuzione LAN
successiva e i suoi limiti sono nel [rapporto 3D](phase3-verification.md#checkpoint-3d--collaudo-hardware).

Richiede almeno due tracker con firmware 3C e autorizzazione separata agli
upload. Nessun dispositivo è stato caricato o riconfigurato durante il lavoro
software. Questo piano non avvia 3D, dashboard, tara, DIAG RESET o cancellazioni
NVS. La password AP concordata resta `12345678`.

## Preparazione

Verificare branch/build, porte USB e hardware prima di qualsiasi upload
autorizzato. Registrare INFO, STATUS, CONFIG GET e WIFI GET: l'ID OSC e il
routing già salvati devono rimanere invariati. I nomi hardware vengono da
`connectivity.mdns_hostname`, senza suffisso `.local`; usare il nome effettivo,
compreso un eventuale suffisso di conflitto.

Connettere i tracker e il Mac alla stessa LAN di prova con multicast abilitato.
Non cambiare una rete condivisa e non eliminare reti salvate per forzare un
guasto. I dati mDNS sono pubblici e non richiedono autenticazione LAN.
Il solo rilevamento di un annuncio sintetico su Mac non dimostra il funzionamento
del componente ESP32: tenere separati i due risultati.

## Comandi Mac di osservazione

`dns-sd` è già presente su macOS, senza installazioni. Questi comandi rimangono
in ascolto: interrompere con Ctrl-C dopo la finestra prevista, senza usarli
come scansioni continue durante i benchmark.

```sh
dns-sd -B _axio._udp local
dns-sd -L axio-aabbccddeeff _axio._udp local
dns-sd -G v4 axio-aabbccddeeff.local
```

Sostituire il nome di esempio con quello della XIAO verificata. Browse per circa
15 s dopo la connessione; resolve/A per circa 5 s. Sul tracker, attendere fino
al successivo ciclo discovery (massimo nominale 35 s più 2 s di query e latenze
driver) prima di leggere la cache. Non interpretare una cache Bonjour del Mac
come prova che il tracker stia ancora rispondendo: annotare TTL e usare anche
USB, nuovi client/query e osservazione della rete quando serve.

```text
AXIO 1 STATUS
AXIO 2 DISCOVERY GET
AXIO 3 METRICS
AXIO 4 CONFIG GET
```

DISCOVERY GET è una lettura della cache: non avvia query e non aggiunge
destinazioni OSC. La porta SRV deve coincidere con listen_port di CONFIG GET.
Non provare HTTP sul nome mDNS: la dashboard LAN non è implementata.

## Criteri di collaudo

| Verifica | Risultato da registrare | Stato |
| --- | --- | --- |
| Due identità hardware | Nomi/MAC diversi, invariati dopo reboot autorizzato; ID OSC non modifica i nomi | NOT RUN |
| Risoluzione `.local` | A/IPv4 corrisponde a STATUS sulla STA del tracker | NOT RUN |
| Servizio Axio | Browse/resolve `_axio._udp`, SRV porta listener e TXT solo model/schema/hw pubblici | NOT RUN |
| Discovery reciproca | Ogni XIAO elenca l'altra con IP/porta corretti, senza sé stessa o credenziali | NOT RUN |
| Cadenza delle query | Una ricerca pendente alla volta, intervalli circa 30–35 s senza raffiche | NOT RUN |
| Peer scomparso | Cache scade entro TTL limitato a 90 s dall'ultimo avvistamento, salvo nuovi dati ricevuti | NOT RUN |
| Perdita STA durante query | Stato draining/offline, cache vuota; IMU continua, niente handle duplicati al ritorno | NOT RUN |
| Cambio IPv4 | Resolver ricreato, peer senza vecchio IP dopo nuova osservazione; nessuna CONFIG APPLY necessaria | NOT RUN |
| AP-only | Nessun servizio/query sul netif AP, OSC sospeso; provisioning AP ancora utilizzabile | NOT RUN |
| Ritorno STA | AP spento, mDNS/discovery ripartono, OSC/configurazione invariati | NOT RUN |
| Conflitto nome controllato | Eventuale suffisso numerico riportato da STATUS; identità hw resta riconoscibile | NOT RUN |
| Risorse | Heap libero/minimo e andamento dopo cicli STA/AP/query annotati, senza trasferire misure B | NOT RUN |

Cambiare temporaneamente listen_port per verificare SRV richiede una modifica
OSC concordata e il ripristino verificato: non è un'azione automatica di questo
piano. Anche prove di conflitto, riavvii e indisponibilità della rete richiedono
un ambiente di test concordato. Non eseguire power-cut o corruzioni NVS.

Le reti guest, VLAN, isolamento client e filtri multicast possono impedire
mDNS pur lasciando STA/OSC unicast funzionanti. In tal caso registrare il limite
della rete; non dichiarare discovery universale e non introdurre scanner IP.
Per il collaudo prestazionale completo attendere approvazione 3D.
