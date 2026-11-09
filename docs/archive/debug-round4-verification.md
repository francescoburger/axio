# Richieste di debug aggiuntive — 9 novembre 2026

Le successive modifiche a soglia di accensione, testi e spegnimento senza
rilascio sono documentate in [round 5](debug-round5-verification.md).

Implementazione delle sei richieste in `debug_requests/requests.md`, con
riferimento ai tre screenshot nella stessa cartella. Su successiva richiesta
esplicita dell'utente, firmware caricato sulla XIAO principale e verificato via
USB/HTTP. Nessun commit; prove fisiche delle durate del pulsante ancora NOT RUN.

## Comportamento

1. Overview: i link ai peer aprono una nuova scheda con `noopener noreferrer`.
2. A dispositivo attivo una pressione debounced inferiore a 5 s emette una
   sola calibrazione al rilascio attraverso `calibrateOscRuntime()` e il callback
   esistente di `ImuManager`. Nessuna modifica alla matematica di taratura.
3. Main Address, Calibration Address e Acknowledgement Address usano il medesimo
   contenitore con slash fisso dei suffissi. I valori caricati mantengono il loro
   indirizzo completo; preview/API/salvataggio ricostruiscono lo slash. Incollare
   un indirizzo completo nei tre campi non aggiunge uno slash doppio.
4. A 5 s esatti o più il rilascio richiede sleep, senza emettere calibrazione.
   Restano debounce, disarmo durante operazioni occupate/errori, sospensione BNO
   verificata e arresto delle comunicazioni. La pressione presente al boot viene
   ignorata fino al primo rilascio stabile.
5. Solo al risveglio GPIO da deep sleep, un wake stub in memoria RTC memorizza il
   contatore hardware prima del bootloader. La prima operazione in `setup()`
   confronta il tempo trascorso con 1,8 s (inizialmente 3 s), contando quindi bootloader e avvio
   Arduino. Un rilascio stabile anticipato riporta in deep sleep prima di USB,
   inizializzazione IMU e Wi-Fi; il BNO rimane nel precedente stato SUSPEND.
   Avvio da alimentazione e restart software proseguono normalmente. Se la
   riconfigurazione wake fallisce, il controllo non attiva IMU/rete.
6. Overview (Tracker ID), OSC e Network confrontano il draft con l'ultimo valore
   salvato. Il listener `beforeunload` chiede conferma per navigazione, reload e
   chiusura; comprende chip, destinazioni, reti, riordino, password e caricamento
   di file nel modulo esistente. Validate e un errore di salvataggio non azzerano
   il draft. Apply/Save riusciti aggiornano il riferimento con il valore inviato,
   conservando eventuali ulteriori modifiche fatte mentre la richiesta era in
   corso. Ripristinare il valore salvato elimina l'avviso.

## Verifiche

| Verifica | Esito | Evidenza / limiti |
| --- | --- | --- |
| Suite host completa | PASS | 110 test; `.build/debug-round4-tests.log` |
| Matematica storica e core | PASS | 1.345 vettori invariati, inclusi nella suite |
| Pulsante host | PASS | debounce, wake premuto ignorato, 4.999/5.000/5.001 s, singolo evento, busy e rollover |
| Integrazione pulsante runtime | PASS | breve: un callback calibrazione; lunga: sospensione BNO e sleep, senza Wi-Fi/browser; hardware simulato |
| Qualificazione wake, logica host | PASS | tempo già trascorso al boot, rilascio anticipato, soglia 3 s, rimbalzo |
| Browser fixture, 430 px | PASS | cinque schermate, link/slash/preview, draft ripristinati, save fallito, Validate, Apply e baseline conservata al rendering del file importato |
| Browser con runtime HTTP host, 430 px | PASS | stesse verifiche con API C++ effettive e hardware simulato |
| Browser con runtime AP host, 1.440 px | PASS | cinque schermate, avvisi e campi OSC anche nella modalità AP simulata; nessuna prova radio fisica |
| Avviso nativo browser | PASS | navigazione da Overview, OSC e Network; annullamento conserva pagina e draft; Brave headless |
| Build XIAO ESP32C3 | PASS | 1.149.063 B flash (87%), 111.172 B RAM statica (33%); `.build/debug-round4-compile.log` |
| Wake stub: collocazione e disassembly | PASS | stub, funzione default e dato nel segmento RTC `0x50000000`; solo accessi registri/RTC e salto alla funzione default, nessuna chiamata a codice flash/IRAM ordinario |
| Upload XIAO principale | PASS | AC:27:6E:7E:32:38, `/dev/cu.usbmodem2101`, hash flash verificato e riavvio USB |
| USB/STA/HTTP dopo upload | PASS | IMU ready, STA 192.168.8.159, asset e API HTTP 200; 100 campioni web validi in circa 10 s, nessun errore di lettura IMU nelle finestre |
| Configurazioni conservate | PASS | confronto prima/dopo di reti, hostname e configurazione OSC completa incluso editor; nessuna scrittura di configurazione durante il collaudo |
| Pulsante reale e wake da sleep | NOT RUN | nessuna pressione fisica né comando sleep inviato |
| Chiusura finestra/reload reali e altri browser | NOT RUN | stesso hook di navigazione; provato il dialogo nativo durante navigazione |

Le prove socket della prima esecuzione erano bloccate dalla sandbox; la suite
completa è poi passata fuori sandbox. La prima compilazione riportava il warning
già presente di fallthrough in `OSCMessage.cpp` della libreria OSC; nessuna
modifica alla libreria. La compilazione incrementale finale è riuscita.
Screenshot/report browser sotto `captures/phase4-ui-debug-round4/` e
`captures/phase4-runtime-ui-debug-round4/`, non destinati al commit.
Prova AP desktop in `captures/phase4-runtime-ui-ap-1440-debug-round4/`.

Upload: `.build/debug-round4-upload.log`. Riscontro hardware di sola lettura:
`.build/debug-round4-hardware.log`, `captures/debug-round4-before.json` e
`captures/debug-round4-after.json` (non destinati al commit). Build dichiarata
via INFO: `Nov 9 2026 21:15:26`. Finestra IMU a riposo circa 99,76 Hz, con
telemetria web circa 99,27 Hz: misure brevi sul dispositivo, senza dichiarazione
di equivalenza prestazionale. Nessuna ricezione OSC misurata in questo collaudo,
nessuna calibrazione o DIAG RESET. La XIAO secondaria AC:27:6E:7C:31:28 non è
stata aggiornata.

## Limiti e collaudo fisico necessario

Aggiornamento successivo: l'utente riferisce che tutto funziona sul dispositivo,
ma chiede di accorciare l'accensione di 1,2 s. Soglia software ridotta da 3 a
1,8 s; indicazioni della dashboard e test ai limiti aggiornati. Il riscontro
manuale riguarda la revisione precedente, non misura la precisione della nuova
soglia. Test host mirati e compilazione della revisione a 1,8 s: PASS
(`.build/debug-round4-wake18-tests.log` e
`.build/debug-round4-wake18-compile.log`). Nuovo upload sulla stessa XIAO
principale: PASS, hash verificato e riavvio USB
(`.build/debug-round4-wake18-upload.log`). Durata fisica della nuova soglia:
NOT RUN; resta da confermare manualmente dall'utente.
USB/IMU/STA/HTTP dopo il secondo upload e confronto delle reti e dell'intera
configurazione OSC: PASS (`captures/debug-round4-wake18-before.json`,
`captures/debug-round4-wake18-after.json` e
`.build/debug-round4-wake18-hardware.log`). Nessun comando di calibrazione,
sleep o reset metriche inviato.

Il timestamp parte dal wake stub, non dal fronte elettrico: la latenza ROM
precedente al primo accesso RTC e la tolleranza del clock devono essere misurate
sulla XIAO. Il GPIO non viene campionato durante il bootloader: se fosse già
rilasciato al primo controllo, la durata precedente non è ricostruibile e viene
considerata insufficiente. Non si dichiara precisione fisica della soglia né
equivalenza temporale senza collaudo hardware.

Provare pressioni sotto/sopra 1,8 s da sleep, almeno due cicli wake/sleep,
pressione breve a dispositivo attivo e pressioni sotto/sopra 5 s, verificando
assenza di calibrazione al wake e allo spegnimento. La verifica fisica della
calibrazione cambia intenzionalmente i riferimenti in RAM e va concordata con
l'utente; non è stata eseguita automaticamente. Conservare reti/NVS, niente
DIAG RESET o cancellazioni. Verificare separatamente restart software e avvio USB.

Il browser controlla testo e disponibilità del proprio avviso: serve interazione
dell'utente e non tutti gli scenari di terminazione forzata/mobile generano
`beforeunload`. Passare semplicemente a un'altra scheda del browser non abbandona
la pagina e non viene bloccato. Nessuna credenziale o cattura aggiunta ai sorgenti.
