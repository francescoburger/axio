# Icona OSC, logo e vista isometrica — 9 novembre 2026

Applicati i tre punti approvati dall'utente, con riferimenti in
`debug_requests/images/correzione_008.png`, `correzione_009.png` e `ypr.webp`.

## Modifiche

- Overview / Device Status: sostituito il piccolo cerchio OSC punteggiato con
  una forma d'onda SVG. Il tratto eredita il viola delle altre icone
  (`--purple2`, #6420ac), senza dipendere dai glifi del font. Stato e logica OSC
  invariati; la dicitura Active/Suspended continua a rappresentare lo stato.
- Header: soltanto Axio, preceduto da un simbolo SVG a tre assi con frecce.
  Rimossi il nome della pagina e l'aggiornamento JS di `pagetitle`.
  Logo bianco sul viola, batteria a destra e selezione della pagina nella
  navigazione conservati. Nessun asset raster o dipendenza aggiunta.
- Motion: vista isometrica lungo la diagonale (1,1,1), con Y in alto,
  X verso destra e Z verso sinistra nella posa neutra. Come precisato
  dall'utente, gli assi ruotano insieme al box: yaw su Y, pitch su X, roll su Z.
  Assi rossi/verdi/blu con frecce, etichette e prolungamenti negativi
  tratteggiati, disegnati sopra al box per restare leggibili. Il colore delle
  facce resta legato alla faccia del box, anziché al suo ordine di disegno.

La camera fissa proietta i tre assi unitari della posa neutra con lunghezza
uguale, sqrt(2/3), e angoli di 120 gradi. Durante il movimento le proiezioni
cambiano con la posa. Il canvas è responsive e usa il device pixel ratio fino a 2 per
mantenere leggibili linee e testo. Ordine delle facce calcolato nella nuova
vista, widget ingrandito rispetto ai precedenti 140×100 px.

Solo nel rendering viene applicato il cambio di base dal sensore Z-up alla
vista Y-up: `(w,x,y,z) → (w,x,z,-y)`. Vertici e assi usano la medesima rotazione.
I componenti quaternion ricevuti e i campi numerici yaw/pitch/roll del firmware
rimangono invariati. La normalizzazione per la sola visualizzazione esisteva
già e resta confinata al browser. Per il riferimento del sensore si veda la
[guida Bosch BNO055, pagina 4](https://www.bosch-sensortec.com/media/boschsensortec/downloads/application_notes_1/bst-bno055-an007.pdf).
Nessuna nuova modifica al C++ di acquisizione, calibrazione, rete,
protocollo OSC, scheduler o controlli di alimentazione.

Un quaternion assente, nullo o non finito cancella il disegno e mostra lo stato
esplicito. La mancanza del contesto 2D mantiene disponibili i valori quaternion
e mostra il messaggio di indisponibilità del rendering. L'attivazione della
vista usa subito l'ultimo campione valido ricevuto, anche quando la telemetria
web è in pausa, senza attendere la prossima richiesta HTTP.

## Verifiche

| Verifica | Esito | Evidenza e limite |
|---|---|---|
| Sintassi JS e packaging gzip | PASS | `node --check`; `scripts/package_web.py`; asset rigenerati |
| Browser responsive finale | PASS | Cinque pagine a 320/430/1440 CSS px, DPR 2; nessun overflow osservato |
| Header e stato OSC | PASS | Header con testo Axio, logo presente, vecchio titolo assente; navigazione attiva; SVG OSC effettivamente viola |
| Proiezione e rotazioni | PASS | Posa neutra isometrica; rotazioni di 90° pitch/X, yaw/Y, roll/Z; assi e vertici solidali |
| Rendering e dati | PASS | Etichette XYZ disegnate; quaternion sorgente non mutato; invalidi, trasparenza, contesto mancante e recupero; attivazione durante pausa |
| Browser con runtime C++ | PASS | Ricompilato con gli asset nuovi; cinque pagine e matrice dei controlli a 430 px |
| Sorgenti congelati | PASS | Nessuna differenza in baseline, fixture, oracolo/vettori e harness matematico storico |
| Build firmware | PASS | 1.144.705 / 1.310.720 B flash; 110.980 B RAM statica; nessun warning nella build incrementale |
| Upload sulla XIAO principale | PASS | MAC AC:27:6E:7E:32:38, porta /dev/cu.usbmodem2101; hash della scrittura verificato |
| IMU e impostazioni dopo upload | PASS | IMU ready, configurazione OSC completa, reti e hostname identici a prima dell'upload |
| Asset realmente installati | PASS | HTML/CSS/JS ricevuti via HTTP identici byte per byte ai sorgenti aggiornati |
| Browser sul dispositivo reale | PASS | Cinque pagine, 430 e 1440 CSS px; header, icona e vista Motion con disegno effettivamente presente |
| Calibrazione/DIAG RESET/prove power fisiche | NOT RUN | Nessun comando inviato dalla sonda; osservate 5 richieste di calibrazione e 0 errori, senza attribuirne la sorgente |
| Suite C++ completa e vettori matematici rieseguiti | NOT RUN | C++ di produzione invariato in questo intervento; non si attribuiscono alla nuova revisione i 90 test del rapporto precedente |
| Accuratezza assoluta della posa e convenzioni meccaniche | NOT RUN | Nessun nuovo collaudo con rotazioni fisiche controllate; questa verifica riguarda proiezione e rendering |

La prova browser aggiunge una posa controllata neutra e rotazioni positive di
90° su pitch/X, yaw/Y e roll/Z; i relativi screenshot sono test del rendering,
non letture fisiche. Le catture live restano separate e usano campioni del tracker.

Il primo controllo di cancellazione del canvas pretendeva che ogni canale RGBA
fosse zero anche per pixel trasparenti. Corretto il criterio del probe sulla
trasparenza (alpha zero); i controlli successivi PASS. Nessun guasto del firmware
è stato dedotto dal primo risultato del probe.

Una prima cattura live aveva il canvas ancora vuoto al momento dello screenshot:
corretti il disegno immediato all'attivazione e l'attesa della sonda, che ora
verifica la presenza di pixel non trasparenti prima della cattura. Le versioni
intermedie con suffissi `round3` e `round3-final` precedono la richiesta degli
assi solidali; le evidenze finali hanno suffisso `round3-body`.

## Evidenze hardware e limiti

Upload eseguito sulla stessa XIAO già autorizzata, dopo lettura delle impostazioni
attuali. `EraseFlash=none`; nessuna cancellazione NVS, nessuna modifica di reti,
destinazioni OSC o riferimenti di calibrazione. Il solo riavvio è quello ordinario
dell'uploader. Hash SHA-256 dell'applicazione:
`f7b4dcbbe1f1c785806c8a50a3e3f271e7a528cc407cbfb423207e5167bf62b8`.

Il primo controllo STA dopo il riavvio è terminato FAIL: il dispositivo era
in fallback AP. La normale riconnessione è poi riuscita, senza interventi sulle
credenziali, all'indirizzo `192.168.1.15`. Il rapporto iniziale è conservato e
non sostituito dal successivo PASS. La causa del ritardo non è stata isolata.

- `.build/debug-round3-body-build.log`, `.build/debug-round3-body-upload.log`.
- `captures/debug-round3-body-before-01.json`: lettura precedente all'upload.
- `captures/debug-round3-body-startup-01.json`: FAIL Wi-Fi nella prima finestra.
- `captures/debug-round3-body-reconnect-01.json`: PASS riconnessione STA.
- `captures/debug-round3-body-after-01.json`: PASS impostazioni e IMU via USB.
- `captures/debug-round3-body-http-01.json`: FAIL della sola assunzione della
  sonda «calibrazioni richieste = 0»; asset e impostazioni già PASS.
- `captures/debug-round3-body-http-02.json`: PASS asset, impostazioni e API;
  contatori osservati, senza richiedere che siano zero e senza azzerarli.
- `captures/phase4-ui-{320,430,1440}-round3-body/`: dati di test,
  proiezione isometrica, assi solidali e pose controllate.
- `captures/phase4-runtime-ui-430-round3-body/`: runtime C++ con hardware simulato.
- `captures/phase4-device-ui-{430,1440}-round3-body/`: dispositivo reale.

Nessun nuovo risultato di timing IMU, consegna OSC o stabilità prolungata è
dedotto dai controlli grafici. Nessun processo di prova lasciato attivo.

## File interessati

- `firmware/Axio/web/index.html`, `style.css`, `app.js`.
- `firmware/Axio/src/web/WebAssets.h`, generato dai sorgenti web.
- `scripts/phase4_browser_probe.mjs`.
- `README.md`, questo rapporto.

Le modifiche già presenti nel repository all'inizio di questo intervento restano
conservate. Nessun commit, merge o push.
