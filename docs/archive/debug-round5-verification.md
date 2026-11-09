# Pulsante e istruzioni — revisione successiva

**Segnalazione successiva: FAIL sul ciclo fisico di accensione.** L'utente
riferisce che il dispositivo non si accende più. Le verifiche sotto non
dimostrano il funzionamento del nuovo wake su hardware. Diagnosi e correzione
nel [rapporto di recupero](wake-recovery-verification.md).

Su richiesta dell'utente la soglia interna di accensione passa da 1,8 a 1,6 s.
La dashboard indica circa 3 s, durata percepita riportata dall'utente; non è una
nuova misura fisica del tempo totale. In Overview il testo è:

> Hold the Axio button for 5 seconds to sleep. Hold it for about 3 seconds to wake.

La calibrazione con pressione breve è descritta vicino al pulsante Calibration
in Motion. Anche la conferma di sleep riporta circa 3 s per l'accensione.

Lo spegnimento fisico ora parte al raggiungimento dei 5 s, con pulsante ancora
premuto. Mantiene la verifica di SUSPEND del BNO e l'arresto delle comunicazioni:
non richiede più il rilascio per iniziare o completare questa sequenza. Il breve
ritardo della sequenza di arresto resta necessario.

Se il pulsante è ancora premuto al momento dello sleep, il wake GPIO viene
configurato sul livello di rilascio e viene memorizzato un flag in RAM RTC.
Al rilascio il chip esegue un passaggio di boot che, prima di inizializzare
IMU/USB/rete nello sketch, rientra in sleep e arma la successiva pressione.
Un rimbalzo riporta all'attesa del rilascio. Il rilascio non avvia Axio e non
calibra. Se invece il pulsante è già rilasciato alla fine dell'arresto, si arma
direttamente la successiva pressione. Nessuna scrittura NVS necessaria.

La richiesta sleep via dashboard conserva l'attesa di rilascio preesistente;
la nuova gestione senza attesa riguarda il pulsante fisico. La calibrazione
breve resta al rilascio, separata dall'evento di pressione lunga.

| Verifica | Esito | Evidenza |
| --- | --- | --- |
| Suite host | PASS | 111 test, `.build/debug-round5-tests.log`; inclusi 1.345 vettori matematici invariati |
| Accensione 1,6 s | PASS su host | sotto soglia, soglia esatta, debounce, tempo di boot incluso nel timer |
| Spegnimento senza rilascio | PASS su host | evento a 5 s, singolo evento, nessuna calibrazione al rilascio; SUSPEND e sleep con pulsante ancora premuto, anche senza rete |
| Riarmo wake | PASS su host | codice di produzione con GPIO/RTC simulati: wake HIGH mentre premuto, rilascio → nuovo sleep LOW; rimbalzo; successiva accensione; reset software indipendente |
| Errori di preparazione wake | PASS su host | configurazione fallita non modifica il flag RTC; errore dopo arresto avvia ripristino sensore/rete |
| Compilazione | PASS | 1.149.239 B flash (87%), 111.172 B RAM statica (33%); `.build/debug-round5-compile.log` |
| Browser | PASS | cinque schermate, nuovi testi Overview/Motion, draft e dialoghi nativi; runtime HTTP host con hardware simulato, `.build/debug-round5-browser.log` |
| Upload XIAO principale | PASS | AC:27:6E:7E:32:38, `/dev/cu.usbmodem2101`, hash flash verificato e reset; `.build/debug-round5-upload.log` |
| Riscontro USB/HTTP e impostazioni | PASS | IMU ready, STA 192.168.8.159, asset/API HTTP 200, reti/hostname e configurazione OSC completa conservati; `.build/debug-round5-hardware.log`, `captures/debug-round5-after.json` |
| Nuove durate e rilascio su pulsante fisico | NOT RUN | richiedono prova manuale dopo upload; nessuna misura fisica inventata |

La prima esecuzione della suite è fallita per un conflitto tra le funzioni delay
dei mock; corretto il conflitto, la suite completa è PASS. La prima prova browser
usava il precedente runtime host, non ricostruito per quel conflitto: non è
evidenza della nuova interfaccia. Le prove definitive usano il runtime ricompilato.

Screenshot e report browser in `captures/phase4-runtime-ui-debug-round5/`;
cattura impostazioni prima dell'upload in `captures/debug-round5-before.json`.
Nessuna cattura o credenziale destinata al commit; nessun commit eseguito.

Restano i limiti del timestamp RTC e del GPIO non campionato durante il
bootloader documentati nel rapporto round 4. La nuova sequenza comporta un breve
risveglio tecnico al rilascio: non si dichiara un consumo costante di deep sleep
durante tale passaggio senza misure. Verificare manualmente almeno due cicli,
tenendo il pulsante premuto oltre lo spegnimento, poi rilasciando e premendo
nuovamente per accendere. Nessuna calibrazione fisica, DIAG RESET o comando
sleep inviato automaticamente durante il collaudo di upload.
