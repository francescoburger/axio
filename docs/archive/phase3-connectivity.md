# Connettività — Checkpoint 3A, 3B, 3C e collaudo 3D

Implementati STA multi-rete (3A), AP protetto con provisioning minimo (3B),
hostname hardware/mDNS/discovery (3C). Collaudo XIAO 3D eseguito sui due dispositivi
autorizzati, con limiti e prove ancora NOT RUN nel
[rapporto](phase3-verification.md#checkpoint-3d--collaudo-hardware).
Fase 3 chiusa con le riserve documentate nel rapporto; dashboard e Fase 4
richiedono una nuova autorizzazione.
Le sezioni seguenti conservano la base 3A e le successive estensioni 3B/3C.

## Scenario operativo e decisioni di prodotto

Axio e il ricevitore OSC operano normalmente sulla **stessa LAN**. OSC attraverso
NAT o reti separate è fuori dallo scenario operativo previsto e non ha requisiti
prestazionali garantiti. Il FAIL storico di riconciliazione dei datagrammi sul
percorso NAT resta aperto, ma non blocca l'accettazione della Fase 3 in questo
ambito. Le misure LAN non dimostrano assenza assoluta di perdite o duplicazioni.

Il fallback AP serve esclusivamente alla configurazione e al recupero della
connettività, **senza OSC**. Il funzionamento prolungato con AP attivo e IMU
collegata non costituisce un requisito di accettazione del prodotto; la prova
prolungata in AP-only resta NOT RUN. Durante la perdita della STA l'acquisizione
IMU deve continuare secondo il comportamento già implementato, mentre OSC viene
sospeso; alla STA valida e con AP spento riprende con la configurazione esistente.

I minimi heap transitori restano un rischio da monitorare: la loro causa e la
frammentazione non sono state determinate. Il recupero dell'heap libero non
dimostra assenza di leak o frammentazione. Questo rischio dovrà essere seguito
specialmente con l'introduzione della dashboard nella Fase 4, quando autorizzata.
Risultati storici e limiti: [3D-Final](phase3-verification.md#3d-final).

## Separazione e dipendenze

`src/connectivity/ConnectivityManager.h` contiene configurazione, validazione,
journal Wi-Fi e macchina a stati, compilabili su host. `ConnectivityRuntime.cpp`
adatta Preferences e il driver ESP32; non include IMU, Wire o motore OSC.
Il loop chiama un tick e continua con lo scheduler IMU esistente.

WiFiManager 2.0.17 è stato esaminato insieme al trasporto che lo chiamava:
`setConfigPortalBlocking(false)` non elimina le attese iniziali di `autoConnect`.
Il suo lifecycle è stato sostituito dal manager; la libreria resta registrata
nella toolchain per baseline e riproducibilità, ma non è linkata nel firmware 3A.
Il serializzatore Legacy resta per la regressione storica; non ha listener attivo.
Le preferenze OSC Legacy `configs` sono ora solo lette, anche al primo avvio.
Schema, journal e scheduler OSC sono invariati.

API del core 3.3.12 esaminate localmente: `STA.cpp`, `WiFiSTA.cpp`,
`WiFiGeneric.cpp` e `NetworkInterface.h`. `WiFi.begin(ssid,...)` può attendere
e disconnettere con timeout: non viene usato dal tick. Solo in setup si eseguono
`WiFi.STA.begin(false)` (bootstrap del driver, attesa interna fino a 1 s) e la
configurazione DHCP. I tentativi usano `esp_wifi_set_config` su RAM e
`esp_wifi_connect`; la disconnessione usa `esp_wifi_disconnect` senza polling.
Il core può eseguire un singolo retry interno al primo errore, anche con
auto-reconnect disattivato; resta entro il budget del tentativo applicativo.
Non viene promessa una latenza massima delle API sincrone SDK/NVS.

## Reti e policy

- Otto reti iniziali: ogni record contiene 33 B SSID, 65 B password e 1 B priorità.
  Configurazione 793 B, blob persistente 809 B per slot. Due buffer journal
  occupano 1.618 B statici; nessuna allocazione per tick. Otto è un limite tecnico
  iniziale per mantenere l'elenco sotto 1 KiB, non un limite definitivo di prodotto.
- SSID: 1–32 byte, confronto esatto e case-sensitive, nessun NUL interno.
  Nessuna normalizzazione. Password vuota per una STA aperta, oppure 8–63
  caratteri ASCII stampabili, oppure esattamente 64 cifre hex per PSK.
  Password protette usano almeno WPA2; reti enterprise/WEP escluse.
- La configurazione completa rifiuta duplicati. Il comando SET aggiorna
  esplicitamente il record di uno SSID esistente, preservandone la posizione.
  Priorità 0–255, valore maggiore prima; parità risolta dall'ordine di inserimento.
- Nessuna scansione applicativa preventiva o periodica. Il driver cerca solo
  l'SSID del tentativo corrente; non si sceglie la rete in base all'RSSI.
  Se più reti sono disponibili vince la prima che si connette nell'ordine
  configurato. Una connessione riuscita resta attiva, senza preemption.
- Ogni tentativo ha 10.000 ms di budget (da 5.000 ms dopo le prove 3D), anche in caso di errore immediato del
  driver. Alla scadenza parte una disconnessione; prima di proseguire si attende,
  attraverso tick successivi, conferma di assenza connessione e almeno 200 ms.
  Se il driver non conferma la disconnessione non si avvia una raffica di tentativi.
- A fine elenco: backoff 1, 2, 4, 8, 16, 30 s, poi 30 s. Il backoff si azzera
  dopo un successo o una modifica esplicita. Nessun catch-up dopo una pausa.
- Dopo perdita STA si ricomincia dalla priorità maggiore, dopo disconnessione,
  intervallo di assestamento e backoff di 1 s. DHCP senza IP non abilita OSC.
- Durate mediante sottrazioni unsigned `uint32_t`, verificate al rollover.
  `offline_ms` è modulo 2^32; timestamp e scheduler IMU restano quelli precedenti.

## Persistenza e migrazione

Namespace dedicato `axio-wifi-v1`, separato da `axio-osc-v1`, `configs` e dalle
credenziali SDK. Due blob fissi `slot0`/`slot1`: magic AXW1, generazione, lunghezza,
CRC32 e record a byte espliciti, senza dipendenza dal padding C++.
Il selettore `active` atomico è il punto di commit. Prima si scrive lo slot
inattivo, poi si verifica rileggendo ogni byte, infine si aggiorna il selettore.
Solo dopo successo si attiva la configurazione RAM e si richiede la disconnessione.
Un errore lascia la configurazione attiva invariata. Nessun riavvio o DIAG RESET.

Al boot, il driver carica la configurazione SDK esistente; prima di qualsiasi
`set_config` il runtime legge `esp_wifi_get_config` e seleziona esplicitamente
`WIFI_STORAGE_RAM`. Non si cancella né si sostituisce la copia SDK originale.
Senza journal Wi-Fi esistente, si valida e salva una copia della singola rete
storica; le altre reti non sono ricostruibili dallo storage SDK singolo.
Un fallimento della migrazione usa la copia storica in RAM (`legacy_volatile`).
Un journal esistente invalido non viene sovrascritto automaticamente: si prova
solo a recuperare una generazione precedente committata; altrimenti si usa
la rete SDK valida in RAM (`recovery_legacy_volatile`) oppure si resta senza reti
con errore esplicito. Un comando USB valido può salvare una nuova configurazione.
Un elenco vuoto salvato è intenzionale: al reboot non reimporta la rete SDK.

Dopo una scrittura non committata, lo slot inattivo può non contenere più
la generazione precedente: se anche lo slot attivo si corrompe, la doppia
anomalia richiede il recupero SDK/USB. Nessuna garanzia di recupero universale
da NVS fisicamente danneggiata. Credenziali persistite senza nuova cifratura
applicativa; accesso fisico/USB e NVS restano nel modello di fiducia precedente.

## Comandi USB

Framing invariato `AXIO <id> <comando>`, JSONL correlato e nessuna telemetria
periodica. I comandi sono separati da CONFIG OSC; nessun import/export JSON.

```text
AXIO 1 WIFI GET
AXIO 2 WIFI SET 10 4c6162 70617373776f7264
AXIO 3 WIFI DELETE 4c6162
```

SET usa `<priorità decimale> <SSID hex> <password hex oppure ->`.
L'esempio fittizio codifica SSID `Lab` e password `password`; non usarla come
segreto reale. Hex è un encoding di trasporto, non cifratura. Non registrare
le richieste SET contenenti segreti. `-` indica una rete STA aperta; non crea AP.
GET restituisce solo `ssid_hex` e priorità, nell'ordine di inserimento.
DELETE rimuove uno SSID; anche l'ultima rete può essere rimossa intenzionalmente.
SET/DELETE salvano prima di attivare: la risposta conferma `persistent:true`
solo dopo commit verificato. Nessun comando restituisce password.

INFO, STATUS e METRICS conservano i campi esistenti e aggiungono `connectivity`:
stato STA, connessione, SSID corrente (byte non ASCII escaped), IP STA/AP,
numero reti, tentativi, perdite, riconnessioni, transizioni, tempo offline,
ultima ragione, sorgente configurazione, errore storage e driver pronto.
Contatori connettività dalla partenza; DIAG RESET conserva questi contatori.
In 3A `ap_active:false`, `mdns_hostname:null`, `fallback_ms:null` e discovery
`not_implemented_3a` indicano funzioni ancora escluse, non servizi attivi.

## OSC e limiti

Il runtime verifica STA effettivamente connessa e IPv4 non nullo. Senza STA
non esegue lo scheduler OSC, non invia dati o ACK e non legge comandi UDP;
il listener viene chiuso. Con STA, il listener viene legato all'IP STA esplicito.
Cambio IP o perdita STA chiudono il socket precedente; un bind fallito viene
ritentato al massimo una volta al secondo. Nessuna riapplicazione CONFIG OSC
alla riconnessione, nessuna duplicazione persistente dei listener.
La tara USB continua a usare l'unico ingresso del manager IMU.

METRICS.engine distingue `suspended_no_sta` (snapshot sospesi),
`suspended_listener`, `listener_failures` e `udp_errors` (attempts meno queued).
I reset espliciti e quelli CONFIG APPLY già esistenti azzerano la finestra OSC;
una transizione STA non la azzera. L'accettazione UDP locale non prova consegna.

L'assenza di invii senza STA è verificata su host; un AP reale non è presente
in 3A. Isolamento della subnet AP, provisioning e transizioni con AP simultaneo
richiedono implementazione 3B e verifiche hardware. Non sono dichiarati PASS.
Identità stabile, mDNS e discovery restano 3C; reti che filtrano multicast o
isolano i client possono impedirli. Nessuna promessa di discovery universale.

## Estensione 3B — AP protetto e provisioning minimo

`FallbackAp.h` aggiunge policy AP e gestione credenziali testabili su host;
`ApRuntime.*` adatta driver, RNG e NVS. `ProvisioningHttp.h` e
`ProvisioningSocket.h` sono separati dal manager STA e rimovibili in Fase 4.
Non si usa WebServer/WiFiManager e non si introducono dipendenze esterne.

### Credenziali e avvio sicuro

SSID `Axio-<12 cifre hex MAC STA>`: usa tutti i sei byte del MAC per stabilità
e distinzione fra tracker, senza dipendere dall'ID OSC modificabile. Questa è
solo l'identità AP; non è stato implementato hostname mDNS.

Per richiesta esplicita successiva al Checkpoint 3B, la password WPA2 è fissa:
`12345678`. Questa scelta sostituisce il precedente requisito di segreto casuale.
Il token HTTP di sessione resta casuale e indipendente dalla password AP.

Namespace `axio-ap-v1`, blob atomico `secret` da 40 B: magic AXP2, password
seguita da padding zero e CRC32. Scrittura e migrazione verificate con readback.
Un record AXP1 valido della versione precedente viene migrato automaticamente
a questa password al prossimo avvio del nuovo firmware. Nessuna modifica alle
reti STA, al journal OSC o alle credenziali SDK. Un record corrotto o un errore
NVS mantengono AP disabilitato, con errore esplicito; nessun AP aperto.

```text
AXIO 1 AP CREDENTIALS
AXIO 2 AP REKEY
```

AP CREDENTIALS mostra SSID e password solo su richiesta USB esplicita.
AP REKEY è conservato per compatibilità come ripristino esplicito della password
fissa: risponde `restored:true`, non genera una password diversa. Chiude prima
portale/AP e riprepara il profilo radio. Password e token restano esclusi dalle
risposte STATUS/INFO/METRICS e dalle letture HTTP.

`softAP` e `softAPdisconnect` non sono usati: nel core 3.3.12 la prima abilita
l'AP prima di configurarlo, la seconda chiama `AP.clear()` e ne azzera il profilo.
Nel setup, dopo migrazione SDK e selezione `WIFI_STORAGE_RAM`, la radio viene
fermata; si seleziona temporaneamente APSTA, si installano SSID e WPA2-PSK/CCMP,
massimo due client e beacon 100 ms, poi si ritorna a STA **prima** del riavvio
radio. Anche un errore di configurazione tenta il ritorno a STA. Solo un profilo
sicuro completato abilita il futuro fallback. L'AP non viene trasmesso durante
questa preparazione. Le transizioni normali usano solo `WiFi.enableAP(true/false)`
con profilo già protetto e non cancellano la configurazione. Nessuna scrittura
nel namespace Wi-Fi SDK: il profilo AP del runtime è solo in RAM.

La preparazione radio avviene al boot o durante AP REKEY esplicito, non a ogni
tentativo STA o fallback. Il ripristino può interrompere momentaneamente STA;
resta fuori dalle finestre benchmark. Un errore nel riavvio radio richiede un
reboot per recuperare il driver; nessuna cancellazione NVS automatica.

### Lifecycle AP

- Dopo 15.000 ms senza connessione STA con IPv4 si richiede APSTA mantenendo
  la policy multi-rete 3A. Vale al boot, senza reti salvate e dopo perdita STA.
- Il portale si avvia solo dopo conferma AP started e IPv4 non nullo.
  IP/DHCP sono quelli del netif AP predefinito del core: `192.168.4.1/24`.
  Nessun captive DNS, redirect automatico, NAT o inoltro IP viene abilitato.
- Un avvio AP non confermato entro 3 s viene chiuso; retry non più frequenti
  di 10 s. Un bind HTTP fallito viene ritentato al massimo una volta al secondo.
- Alla connessione STA si chiudono subito client/listener HTTP e si disabilita
  l'AP. Uno spegnimento fallito viene ritentato a intervalli di 1 s. La STA
  non viene disabilitata per chiudere AP e non si reinizializza l'IMU.
- In assenza di credenziali/profilo sicuro, l'AP resta spento e USB espone l'errore.
  Il loop non attende eventi, client o completamento della disconnessione.

Le API driver sono sincrone e non hanno un tempo massimo dimostrato; nessun
`delay`, scansione applicativa o attesa di connessione è aggiunto al loop.
Canale AP e STA condividono la radio: associazioni e cambi canale possono
interrompere temporaneamente i client AP. La tolleranza reale va misurata.

### Interfaccia provvisoria

Collegarsi con la password `12345678` e aprire manualmente `http://192.168.4.1/`.

| Endpoint | Contenuto |
| --- | --- |
| GET `/` | Modulo SSID/password/priorità, pulsanti salva o elimina e link alle letture |
| GET `/status` | Solo stato connettività/AP e contatori pubblici |
| GET `/networks` | SSID in hex e priorità, nessuna password |
| POST `/network` | Una singola modifica STA, stesso validatore/journal di USB |

La password STA nel modulo è un input password senza valore precompilato.
Vuota significa rete STA aperta, non conservazione implicita del vecchio segreto.
Per cambiare la priorità via web occorre reinserire la password della rete.
Salvare aggiorna lo stesso SSID; eliminare rimuove quel record. Nessun
import/export JSON, editor OSC, tara remota, reset, OTA o dashboard.

Il socket ascolta solo sull'IP AP, mai su ANY o sull'IP STA, e viene chiuso
quando torna STA. HTTP senza TLS è protetto dall'accesso WPA2 all'AP; chi possiede
la password AP è considerato autorizzato a configurare le reti. Non viene aggiunta
autenticazione obbligatoria sulla LAN. Token casuale di sessione nel form,
Host IP letterale e Origin verificato se presente impediscono modifiche da form
esterni; nessuna CORS. Le risposte usano no-store, nosniff e CSP senza script.

Un client HTTP alla volta, backlog 1, header massimo 1.024 B e body 512 B;
buffer richiesta 1.537 B e risposta 4.096 B. Socket non bloccanti, massimo
256 B RX oppure TX per tick, timeout assoluto di 3 s anche con input/output
stallati, senza rinnovo per ogni byte. Duplicati, Transfer-Encoding, lunghezze
invalide e NUL sono rifiutati. Le risposte parziali gestiscono backpressure.
Un client lento può occupare il portale per 3 s, senza attesa applicativa IMU.
Il formato del modulo limita gli SSID a 32 byte: per byte arbitrari resta USB hex.

Un successo STA può chiudere il portale prima che il browser riceva la conferma;
verificare via USB lo stato e l'elenco salvato, senza ripetere ciecamente SET.

### Integrazione OSC e diagnostica 3B

OSC richiede STA con IPv4 **e modalità AP disabilitata**. Finché l'AP è abilitato
non ci sono invii dati/ACK o letture tara UDP, anche nel breve intervallo con
STA già connessa o se lo spegnimento AP fallisce. Quando AP è spento, il listener
si lega all'IPv4 STA e riprende la configurazione OSC già attiva. Non si limita
lo schema OSC né si riscrive il journal. La rete AP non è disponibile come
interfaccia di trasmissione OSC.

METRICS.engine aggiunge `suspended_ap`: snapshot sospesi con STA presente ma
AP ancora abilitato; `suspended_no_sta` resta per l'effettiva assenza STA.
Nessuna sospensione è conteggiata come errore UDP.
`connectivity.ap_active` è lo stato AP started effettivo, `ap_ip` il relativo IP;
`fallback_ms:15000` è la soglia. L'oggetto `connectivity.ap` contiene `enabled`
(modalità AP), `ready` (profilo sicuro), SSID/IP pubblici, `http_active`, richieste
di avvio, spegnimenti, fallimenti, ultimo tempo fallback e motivo/errore.
Nessuno di questi campi contiene password o token del form.

Identità mDNS, servizio Axio e discovery restano esclusi fino a 3C.

## Estensione 3C — hostname, servizio Axio e discovery LAN

`DiscoveryManager.h` contiene identità, cache e policy; `DiscoveryRuntime.*`
adatta il componente mDNS già incluso nel core ESP32 3.3.12. Nessuna nuova
libreria installata. Non include IMU/Wire e non modifica la configurazione OSC.

### Identità e servizio

Prima di inizializzare STA, `esp_read_mac(ESP_MAC_WIFI_STA)` genera un hostname
`axio-<12 cifre hex minuscole del MAC completo>`, applicato con WiFi.setHostname.
Non dipende dall'ID OSC modificabile e non richiede NVS. Risoluzione locale
`axio-<MAC>.local` solo quando il servizio mDNS è attivo sulla STA con IPv4.
Un errore di lettura MAC o applicazione hostname disabilita discovery con errore
esplicito senza bloccare IMU o la policy di rete.

| Record | Dato pubblico |
| --- | --- |
| Servizio | `_axio._udp.local.` |
| Istanza iniziale | `axio-<MAC>` |
| Host | `axio-<MAC>.local` |
| SRV port | Porta listener OSC della configurazione schema 1 attiva |
| TXT `model` | `Axio` |
| TXT `schema` | `1` (schema OSC supportato) |
| TXT `hw` | MAC STA completo, 12 cifre hex minuscole |

Non vengono annunciati SSID, password, token HTTP, destinazioni OSC o campioni
IMU. Non viene annunciato un servizio HTTP sulla LAN: la dashboard non esiste
ancora. SRV indica il listener OSC, non una porta HTTP o una garanzia di streaming.
La porta segue CONFIG APPLY senza richiedere nuove configurazioni persistenti.

MAC diversi producono nomi diversi. In caso di conflitto DNS, il resolver può
assegnare un suffisso numerico: una lettura locale del nome effettivo ogni secondo
aggiorna `mdns_hostname` e `name_changes`, senza output periodico. La cache
accetta il nome base o un suffisso `-<numero>` coerente col TXT `hw`. Copie con
lo stesso MAC non sono distinguibili tramite questa identità hardware; non
si promette unicità per dispositivi clonati. I dati mDNS non sono autenticati.

### Query e cache

- Usate esclusivamente `mdns_query_async_new` e
  `mdns_query_async_get_results(..., 0, ...)`. Evitati queryService/query_ptr
  sincroni, che nel wrapper Arduino possono attendere tre secondi.
- Prima query dopo 2–5 s dalla STA, poi ogni 30–35 s; lo scostamento stabile
  deriva dagli ultimi byte del MAC per distanziare tracker avviati insieme.
  Una sola query attiva, durata richiesta 2 s, nessun catch-up o comando USB
  che possa forzare raffiche di scansioni.
- Inizializzazione mDNS fallita: retry non prima di 5 s. Una query non avviata
  viene ritentata solo al prossimo intervallo ordinario. Nessuna attesa
  applicativa per completare una query.
- Cache iniziale di **16 peer** in RAM, un record per `hw`, massimo 16 risultati
  SDK per query. Limite tecnico della discovery, distinto dai limiti OSC;
  il record contiene host, IPv4, porta e tempi, senza TXT arbitrari o segreti.
  Risposta JSON dedicata con buffer 4.096 B. Nessun salvataggio dei peer in NVS.
- Richiesti `model=Axio`, `schema=1` e `hw` valido, senza duplicati dei tre
  campi. Accettati solo risultati della netif STA, IPv4 unicast e hostname
  coerente. Il tracker stesso e il proprio IPv4 sono esclusi. Elaborazione
  limitata a 16 risultati, 16 TXT per record e otto indirizzi per record.
- Una nuova osservazione aggiorna IP/porta/nome dello stesso peer. Scadenza
  dopo il TTL ricevuto, con massimo 90 s senza nuovo avvistamento. Rollover
  millis verificato. Un goodbye completo con TTL zero rimuove il record;
  record incompleti possono rimanere fino alla scadenza.
- Cache piena: record nuovi scartati, contatore `overflow` incrementato.
  Il limite risultati del resolver può già troncare la risposta: overflow zero
  non dimostra che siano stati scoperti tutti i tracker presenti sulla LAN.

### Interfacce, disconnessione e cambio IP

mDNS si avvia solo con STA connessa, IPv4 non nullo e modalità AP disabilitata.
La netif AP predefinita viene rimossa dal responder prima di aggiungere il
servizio; non vengono avviate query o annunci su AP-only.

Alla perdita STA, al cambio IPv4 o al ritorno in AP, vengono disabilitata e
scollegata la netif STA dal responder e svuotata la cache. Se una query è ancora
attiva, il manager entra in `draining`: controlla la fine senza attese, scarta
i risultati e libera risultati e handle. `mdns_query_async_delete` è chiamata
solo dopo completamento, come richiesto dal contratto ESP-IDF; poi mdns_free
chiude il responder. Non si libera un resolver sotto una query in corso.
Alla successiva STA valida si ricrea il servizio sulla nuova interfaccia/IP.

Le API SDK di init/configurazione/free restano sincrone: la scelta asincrona
elimina l'attesa applicativa della ricerca, non dimostra una latenza massima
del loop. Se un driver non completa una query, il servizio resta disabilitato
in draining con stato esplicito, senza avviare query sovrapposte. Heap, jitter
e correttezza radio del lifecycle richiedono ancora verifica sulla XIAO.

### USB e limiti LAN

`AXIO <id> DISCOVERY GET` restituisce una fotografia della cache:

```json
{"state":"ready","peers":[{"hw":"aabbccddeeff","hostname":"axio-aabbccddeeff.local","ip":"192.168.1.42","port":16889,"age_ms":1000}]}
```

Nessuna query è avviata dalla lettura. INFO/STATUS/METRICS mantengono i campi
precedenti e aggiungono hostname effettivo, stato discovery e
`discovery_details`: servizio/porta, peer, query pendente, avvii, fallimenti,
completamenti, filtri, scarti, overflow, conflitti nome e ultima transizione.
Stati: `offline`, `ready`, `querying`, `draining`, `identity_error`; `ready`
significa servizio configurato, non verifica di raggiungibilità da altri host.
I contatori discovery restano dal boot e non vengono azzerati da DIAG RESET.
La discovery non aggiunge destinazioni OSC né cambia routing o tara.

mDNS dipende dal multicast locale UDP 5353. VLAN diverse, isolamento client,
reti guest, firewall e router che filtrano multicast possono impedire risoluzione
e discovery anche con STA funzionante. Non viene implementato un relay o uno
scanner IP alternativo. Non promettere discovery universale su ogni router.
Piano per reti e tracker reali: [phase3c-lan-plan.md](phase3c-lan-plan.md).
