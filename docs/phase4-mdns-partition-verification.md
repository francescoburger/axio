# Due leader mDNS già attivi: partizione e ricongiungimento

9 novembre 2026. Prova autorizzata dall'utente dopo il commit `3b144bc`.

**Esito: PASS nel caso strumentato provato.** Entrambe le XIAO sono state
osservate leader con nome nativo `axio` e risposte A distinte, poi sono
convergate automaticamente dopo il ripristino della ricezione mDNS.
Al termine è stato reinstallato su entrambe l'esatto binario di produzione
precedente alla prova e sono state confrontate le configurazioni.

## Metodo e ambito

`scripts/build_mdns_partition_probe.py` copia Axio sotto `.build/mdns-partition/`
e aggiunge soltanto nella copia un comando USB di prova e un wrapper di link
per `udp_recv`. Il wrapper intercetta esclusivamente il PCB nativo sulla
porta 5353: durante HOLD libera i pacchetti ricevuti; dopo JOIN inoltra allo
stesso callback originale. Il blocco scade automaticamente dopo 60 secondi.
Wi-Fi resta associato; non sono modificati OSC, IMU o le reti salvate.
La policy di elezione, gli annunci periodici e il componente mDNS restano
quelli di produzione. Nessun secondo responder o patch al framework.

`tools/mdns_partition_probe.py --allow-upload` salva lo stato, carica la
build di prova, attiva HOLD su entrambe e attende due leader già stabiliti.
Un listener passivo sul Mac conserva i pacchetti Axio, senza rispondere.
Prima di JOIN richiede sia due nomi nativi `axio` sia annunci A osservati
da entrambe le sorgenti. Dopo JOIN controlla ruoli, conflitti, peer, DNS e
HTTP. Il percorso `finally` ripristina i binari salvati e confronta le
configurazioni; non invia Apply, calibrazione o DIAG RESET.

È una partizione controllata della ricezione mDNS sul firmware strumentato,
con annunci in uscita ancora visibili al Mac. Non equivale a segmentare tutta
la LAN o a una perdita radio fisica; i risultati non vengono estesi a quei
casi, a tre dispositivi o a qualsiasi profilo di perdita pacchetti.

## Risultati hardware

| Controllo | Esito | Osservazione |
|---|---|---|
| Due leader già attivi | PASS | Entrambi `entry_owned=true`, nome nativo `axio`; mantenuti per almeno 8,42 s prima di JOIN |
| Due annunci A effettivi | PASS | Il Mac vede 192.168.8.159 e 192.168.8.184 per axio.local durante HOLD |
| Blocco/ripresa RX | PASS | Pacchetti scartati su entrambe durante HOLD; conteggi forwarded crescenti dopo JOIN |
| Rilevamento nativo del conflitto | PASS | La principale incrementa entry_conflicts da 0 a 1 e si ritira |
| Un solo leader osservato stabilmente | PASS | Primo campione a circa 1,86 s da JOIN; secondaria leader, principale standby |
| Convergenza con discovery e DNS | PASS | 28,73 s da JOIN, poi finestra stabile di 12,60 s |
| Query A unicast alle due schede | PASS | Solo la secondaria risponde per axio.local, con 192.168.8.184 |
| Hostname individuali | PASS | Entrambi risolvono al rispettivo IP |
| HTTP | PASS | Host comune sul leader e IP individuale della principale: 200 |
| IMU durante la build di prova | PASS | 6028 acquisizioni in 60,499747 s, 99,6368 Hz, zero errori lettura |
| OSC runtime | PASS | Contatore udp_queued crescente |
| Ricezione OSC sul Mac GL | NOT RUN | Destinazione salvata resta 192.168.1.4:16888 |
| Ripristino produzione | PASS | Upload verificato su entrambe; metadata INFO build originale ripristinati |
| Configurazioni | PASS | CONFIG GET e WIFI GET identici prima della prova e dopo il ripristino |
| Controllo finale produzione, 30 s | PASS | Un solo leader, HTTP comune 200, configurazioni preservate; principale leader e secondaria standby |

Le richieste JOIN sono state inviate a distanza di 6,36 ms. I tempi sono
osservazioni del processo host con letture USB sequenziali, non un limite
massimo garantito né una misura della latenza radio. Il primo campione con
un solo leader è distinto dalla convergenza completa con peer e DNS.

La build di prova occupa 1.149.413 B flash e 111.204 B RAM statica. Warning
esterno OSCMessage.cpp:897, implicit fallthrough, già noto. La build di
produzione è rimasta invariata: 1.148.283 B flash e 111.172 B RAM statica.
Le misure IMU sopra appartengono alla prova strumentata; non dichiarano
equivalenza prestazionale universale fra i due binari.

Sintassi Python e CLI del nuovo raccoglitore: PASS. Controllo del diff:
PASS. I sorgenti `firmware/Axio/`, reference e fixture congelate sono invariati
rispetto al commit `3b144bc`; la suite di produzione non è stata ripetuta per
questa sola aggiunta di strumenti e rapporti (ultimo esito: 110 test PASS).

## Evidenze e riproduzione

Artefatti ignorati da Git:

- `captures/phase4-mdns-partition-01/report.json`: stato, comandi, osservazioni,
  risultati e confronto prima/dopo; esito automatico PASS, 17 controlli;
- `captures/phase4-mdns-partition-01/mdns-packets.jsonl`: 91 pacchetti Axio
  conservati con sorgente, istante, fase e payload DNS esadecimale;
- `captures/phase4-mdns-partition-restored-01.json`: controllo in sola lettura
  del firmware di produzione dopo il ripristino;
- `.build/mdns-partition/manifest.json`, `compile.log`, `restore/`, `artifacts/`.

SHA256 produzione ripristinata:
`437ede9536b827517ea22eddc1999a0eeba00b622ee44308b2e4f2cc8dc9d1a0`.
SHA256 build di prova:
`40a71ab84a428a25068025e52a495ae088ea9d5df391eb1501aa32e678ec9e13`.

La costruzione usa una cartella nuova e non sovrascrive artefatti precedenti.
Richiede la toolchain già installata e il binario corrente in `.build/artifacts/`.
L'esecuzione richiede esplicitamente `--allow-upload`, una cartella catture
nuova e l'IP dell'interfaccia Mac sulla LAN. Esempio dell'esecuzione provata:

```sh
python3 -B scripts/build_mdns_partition_probe.py
.venv/bin/python -B tools/mdns_partition_probe.py \
  --interface-ip 192.168.8.164 \
  --run-dir captures/phase4-mdns-partition-01 --allow-upload
```

Nessuna nuova calibrazione o modifica delle destinazioni OSC. Il secondario
resta senza IMU (`init_failed` atteso). Listener e processi di prova terminati.
Nessun nuovo commit o push eseguito.
