# Fase 4 — primo collaudo finale: endurance di un'ora

9 novembre 2026, dalle 14:24:54 alle 15:24:54 Europe/Rome. Base firmware
`5e4790d`, branch `test/phase4-endurance`; nessun upload o modifica al firmware
durante questa prova. Raccolta autonoma senza chiamate a Codex. Processo e
assertione caffeinate terminati, verificati anche nella tabella dei processi.

**Durata PASS, esito complessivo FAIL:** la finestra richiesta è stata
completata, ma sono stati osservati errori HTTP. Le evidenze originali restano
in `captures/phase4-endurance-hour-01/`; non sono state riscritte.

**Accettazione successiva:** l'utente ha accettato la limitazione HTTP con
client concorrenti e richiesto di mantenere il firmware attuale. Il FAIL
strumentale resta registrato; questa limitazione non è più un blocco alla
prosecuzione. Il confronto sonda + browser e il caso due browser ancora
NOT RUN sono distinti nel [rapporto dedicato](phase4-http-contention-verification.md).

| Verifica | Esito | Risultato e limite |
|---|---|---|
| Durata richiesta | PASS | 3.600,003 s di raccolta; finestra contatori IMU 3.600,085 s |
| IMU principale | PASS funzionale | 359.161 acquisizioni, media 99,7646 Hz, zero nuovi errori di lettura |
| HTTP campioni | FAIL | 33.035 campioni validi / 33.045 richieste; 10 errori, timeout negli esempi conservati |
| Frequenza HTTP effettiva | Misurata | 9,1764 campioni validi/s a fronte di target 10 Hz; nessuna raffica di recupero |
| Snapshot HTTP principale | FAIL | Un timeout nella lettura dello snapshot, con limite della sonda di 3 s |
| OSC ricevuto sul Mac | PASS formato/ricezione | 345.954 datagrammi validi, 1.037.862 messaggi, nessun errore di decodifica; media 96,0983 datagrammi/s |
| Invio UDP del firmware | PASS nella finestra osservata | 345.961 accettati localmente, zero nuovi errori; cumulativo 7 prima e dopo |
| mDNS | PASS nei campioni | 113 query, sempre un solo A 192.168.1.15; nessun errore DNS registrato |
| Riavvii e perdita STA | PASS nei controlli osservati | Uptime crescente su entrambe le schede, nessun reset rilevato e zero nuove disconnessioni nei contatori |
| Configurazioni OSC e reti | PASS | Confronto iniziale/finale identico su entrambe le schede |
| Secondaria senza BNO055 | Stato atteso | `init_failed` prima e dopo; non è una verifica PASS dell'IMU secondaria |

Il confronto fra datagrammi accettati e ricevuti **non misura sette pacchetti
persi**: le finestre iniziale/finale di ricevitore e contatori non coincidono.
Zero perdite e zero duplicati restano NOT RUN/non dimostrati. Il rate IMU è
misurato separatamente via USB.

## Memoria e timing

Heap libero campionato della principale: 101.196–105.836 B. Primo/ultimo
snapshot: 105.604 → 105.596 B. Blocco massimo libero: 86.004–94.196 B;
primo/ultimo: 90.100 → 94.196 B. Minimo heap cumulativo: 91.240 B al termine.

Secondaria: heap libero 107.296–110.256 B; primo/ultimo snapshot
109.232 → 109.188 B; blocco massimo libero 98.292 B nei campioni.
Gli estremi e il confronto iniziale/finale non certificano l'assenza di leak
o frammentazione in ogni condizione.

Il massimo intervallo IMU cumulativo rimane 20.837 µs, invariato rispetto a
prima del test: non è il massimo esatto della sola ora. Intervalli UDP host
osservati fino a 32,534 ms; non sono misure dirette del jitter IMU o radio.

## Errori HTTP e lavoro successivo

Gli esempi salvati mostrano timeout dei campioni concentrati intorno alle
14:51 e 15:13 Europe/Rome. Il timeout dello snapshot della principale è alle
15:00:35 circa. Il suo record contiene già il risultato DNS corretto, mentre
la lettura HTTP dello snapshot non è terminata entro il budget.

Il raccoglitore conserva al massimo dieci esempi di errore, pur contando tutti
gli eventi: per gli undici eventi complessivi un dettaglio individuale non è
disponibile. Non si attribuisce quindi arbitrariamente una causa a tutti e
dieci gli errori dei campioni. Non sono stati registrati errori USB, errori di
lettura IMU, nuovi errori UDP o errori di formato OSC.

La stabilità HTTP non soddisfa il criterio di questa prova. La causa rimane
da isolare tra server firmware, gestione delle connessioni e rete/host; i dati
disponibili non dimostrano che il firmware sia l'unica causa. Il prossimo
intervento utile è una diagnosi HTTP mirata con tempi delle singole richieste
e conservazione incrementale di ogni errore, seguita da una nuova endurance
solo se giustificata dai risultati o da una correzione.

Aggiornamento di contesto confermato dall'utente: la dashboard è stata aperta
dal browser un paio di volte durante l'ora per controllarne lo stato.
La prova comprende quindi traffico della sonda e accessi browser reali.
Dispositivo, pagina e orari precisi non sono noti.
È quindi presente un possibile carico browser aggiuntivo, non misurato dalla
sonda. Il server attuale gestisce una richiesta/connessione alla volta e
prevede un timeout assoluto di 5 s; la sonda dei campioni attende 2 s. Una
connessione browser lenta o in attesa può ritardare le richieste della sonda;
la pagina Motion aggiunge anche polling di telemetria. Questa è un'ipotesi
coerente con il codice, non una causa dimostrata dai log. Il confronto mirato
fra sonda sola e sonda con browser concorrente è il prossimo caso da isolare.
Il risultato originale FAIL rimane invariato: l'accesso alla dashboard è un
uso normale del prodotto da verificare, non un errore attribuito all'utente.

Successivamente, su autorizzazione dell'utente, il confronto breve A/B/A ha
riprodotto il problema: 0 errori senza browser, 9 con browser, 0 dopo la sua
chiusura. Firmware e configurazioni invariati. Tracce e interpretazione in
[phase4-http-contention-verification.md](phase4-http-contention-verification.md).

L'analisi iniziale del rapporto orario non ha avviato nuove prove; il confronto
successivo autorizzato è documentato separatamente sopra. Nessuna calibrazione,
DIAG RESET, Apply, azione power, commit aggiuntivo o push.
