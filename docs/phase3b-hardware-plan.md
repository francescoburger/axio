# Piano hardware 3B — piano storico

Le classificazioni NOT RUN sotto descrivono la fine di 3B. L'esecuzione hardware
successiva e i suoi limiti sono nel [rapporto 3D](phase3-verification.md#checkpoint-3d--collaudo-hardware).

Questo piano non è stato eseguito. Richiede autorizzazione esplicita a upload
e prove sulla XIAO, una rete di prova controllabile e un Mac con USB disponibile.
Non avvia automaticamente 3C/3D, non cancella NVS e non effettua tara o DIAG RESET.
Non modificare una rete condivisa per provocare un guasto: usare un AP di prova
dedicato o una condizione RF isolata concordata. Non eliminare le reti già salvate
per forzare il fallback.

## Preparazione dopo autorizzazione

1. Verificare porta XIAO, versione/branch e build senza cambiare configurazioni.
   Chiudere Serial Monitor prima dei tool USB. L'ultima porta storica era
   `/dev/cu.usbmodem2101`; deve essere verificata di nuovo prima dell'upload.
2. Leggere INFO, STATUS, METRICS, WIFI GET e CONFIG GET; annotare ID, reti pubbliche,
   routing OSC e contatori iniziali. Non registrare password o richieste WIFI SET.
3. Eseguire lo script upload solo dopo autorizzazione hardware, usando la porta
   verificata. Non usare erase-flash, reset delle preferenze o utility NVS.
4. Recuperare AP CREDENTIALS da USB senza log/catture e conservarle nel proprio
   gestore di password. Non eseguire AP REKEY per iniziativa del collaudo.

Comandi esemplificativi; non eseguiti da questo piano:

```sh
.venv/bin/python -B tools/hardware_test.py --list-ports
bash scripts/compile.sh
bash scripts/upload.sh --port /dev/cu.usbmodem2101
```

## Sequenza e criteri

| Passo | Azione controllata | Criterio PASS reale | Stato |
| --- | --- | --- | --- |
| Boot con rete salvata | Rete STA disponibile, leggere STATUS via USB | STA con IP, BNO ready, nessun AP dopo connessione, CONFIG GET invariato | NOT RUN |
| Identità/segreto AP | Leggere AP CREDENTIALS solo via USB | SSID con MAC completo; password fissa `12345678`; nessun segreto in INFO/STATUS/METRICS/WIFI GET | NOT RUN |
| Perdita STA | Rendere indisponibile solo la rete di prova; nessun DELETE | Acquisizioni continuano, tentativi/backoff coerenti, contatore disconnessioni aumenta | NOT RUN |
| Fallback | Misurare tempo da perdita/boot offline a AP started | Richiesta dopo circa 15 s; AP WPA2/CCMP, password errata rifiutata, nessun AP aperto osservato | NOT RUN |
| DHCP e HTTP | Collegare Mac all'AP con il segreto, aprire IP letterale | IP client AP, pagina e GET /status /networks accessibili; password mai nelle letture | NOT RUN |
| Client HTTP lento | Tenere connessione incompleta oltre 3 s, poi nuova lettura | Client chiuso; portale recupera; acquisizioni continuano e nessun blocco applicativo | NOT RUN |
| Input invalido | Tentare priorità/password non valida nel form | Errore, elenco precedente invariato, nessuna cancellazione NVS | NOT RUN |
| OSC in AP-only | Leggere due METRICS USB durante AP-only e osservare ricevitore | UDP attempts/queued non avanzano, listener false, suspended_no_sta aumenta; nessun datagramma osservato su AP | NOT RUN |
| Provisioning | Salvare una rete di prova concordata senza rimuovere le altre | Singolo upsert, persisted, tentativo STA; nessuna password nella risposta | NOT RUN |
| Ritorno STA | Ripristinare la rete dedicata o raggiungere la rete provisionata | STA con IP, AP e HTTP spenti, listener legato a STA; ripresa OSC senza CONFIG APPLY | NOT RUN |
| Isolamento interfacce | Verificare IP STA mentre AP è spento | Porta HTTP 80 di provisioning non disponibile su STA | NOT RUN |
| Reboot autorizzato | Riavvio senza erase o modifiche NVS | Elenco reti, CONFIG GET OSC e segreto AP invariati; nessuna tara automatica | NOT RUN |
| Memoria e IMU | Finestra USB e OSC indipendente, condizioni annotate | Heap libero/minimo, rate e jitter misurati e confrontati con B senza presunta equivalenza | NOT RUN |

Il browser può perdere la risposta POST quando STA riesce e AP viene spento:
verificare il risultato via USB, senza riapplicare alla cieca. Tornare con il Mac
sulla LAN per osservare OSC dopo la riconnessione. Non assumere che la sola
assenza di pacchetti sul Mac provi il blocco OSC: controllare anche i contatori
firmware e l'interfaccia del listener. Una prova con destinazione OSC temporanea
sulla subnet AP richiede una modifica concordata e successivo ripristino; non
è inclusa automaticamente in questa procedura non distruttiva.

Per una finestra USB senza tara/reset e una cattura separata, dopo conferma
della porta e della destinazione OSC già presente:

```sh
.venv/bin/python -B tools/hardware_test.py --port /dev/cu.usbmodem2101 --metrics-seconds 10 --report captures/phase3b-usb-01.json
python3 -B tools/osc_receiver.py --bind 0.0.0.0 --port 16888 --duration 12 --output captures/phase3b-osc-01.jsonl
python3 -B tools/analyze_capture.py captures/phase3b-osc-01.jsonl
```

Scegliere nomi di cattura nuovi: non sovrascrivere evidenze. Ricezione OSC non
misura il rate IMU; contatori UDP locali non confermano consegna. Le misure di
riferimento B sono 99,947 Hz IMU, 99,942 pacchetti/s Mac, heap 133.084/129.068 B,
ma restano misure storiche su un altro firmware. Tara fisica, rotazione reale
del segreto AP, fault NVS e power-cut sono esclusi finché non autorizzati a parte.
