# Axio — regole del repository

- Progetto Axio, repository axio; Seeed XIAO ESP32C3 e BNO055, macOS,
  Arduino IDE e CLI. Sketch principale: `firmware/Axio/Axio.ino`.
- Fase **2 completata e approvata** dall'utente, inclusi i Checkpoint A e B.
  Checkpoint 3A/3B/3C software completati; autorizzato Checkpoint 3D:
  upload e collaudo sulla XIAO, integrazione Wi-Fi/AP/mDNS/discovery/OSC.
  Preservare NVS e reti esistenti; nessuna tara, DIAG RESET o prova distruttiva.
  Fermarsi al termine di 3D. Non iniziare dashboard o Fase 4 senza approvazione.
  Nessun commit, merge o push senza approvazione esplicita.
- Password AP fissa `12345678` richiesta esplicitamente dall'utente;
  sostituisce il precedente requisito di segreto casuale. WPA2 resta obbligatorio.
- `reference/HandTrackerV1_1.ino` è immutabile: niente rinomina, correzioni,
  formattazione o sostituzione. Hash atteso in `tests/test_baseline.py` e
  `docs/baseline-v1.1.md`. Anche i difetti storici vanno documentati, non corretti.
- Preservare esattamente la taratura di quaternion **e yaw**, ordine dei
  prodotti, cast verso zero, resto C++, precisione e ordine delle operazioni.
  Non sostituire con inverse, normalizzazioni, clamp o conversioni diverse.
- Centralizzare tutti i pin tramite `#define` in
  `firmware/Axio/hardware_config.h`. Cablaggio confermato: SDA GPIO6/D4,
  SCL GPIO7/D5; indirizzo BNO055 0x29 nello stesso file. L'I²C usa i define.
- La copia Fase 0 è congelata in `tests/fixtures/phase0-Axio.ino`: differisce
  dalla baseline solo per l'include hardware. Preservare il controllo byte e
  tutti i vettori/test storici, oltre ai test del core di produzione.
- Mantenere compatibilità Arduino IDE: cartella e sketch omonimi; moduli in
  `src/core/` e `src/transport/`. Il trasporto serializza il campione senza I²C.
- Riferimenti iniziali identità/yaw zero, nessuna tara automatica. Unico ingresso
  comandi `ImuManager::calibrateReference()`. Pubblicare solo letture complete;
  in errore conservare ultimo valido e stato esplicito. Nessun retry init nuovo.
- Scheduler 10 ms senza catch-up, `millis()` unsigned e timestamp monotono 64 bit.
  Nessuna equivalenza temporale o prestazione dichiarata senza hardware.
- Lavorare nel repository; niente installazioni, modifiche esterne o azioni
  distruttive senza approvazione. Niente pubblicazione o push remoto.
- Evitare dipendenze inutili. Script, build, cache e test generano file in
  `.build/`; catture in `captures/`. Non commettere credenziali o catture.
- Comandi: `bash scripts/compile.sh`, `bash scripts/compile.sh --baseline`,
  `python3 -B -m unittest discover -s tests -v`. Non effettuano upload.
- Classificare ogni verifica PASS, FAIL o NOT RUN; registrare limiti e warning.
  Nessun risultato hardware inventato; ricezione OSC non misura il rate IMU.

Vincoli di prodotto e delle fasi future:

- OSC configurabile autorizzato al Checkpoint B; dashboard web LAN senza autenticazione, più reti Wi-Fi,
  fallback AP protetto, mDNS, pulsante, LED, batteria e deep sleep.
- Nessun obbligo di protocollo OSC Legacy nella versione finale.
- Nessun limite fisso a destinazioni o messaggi OSC prima dei benchmark.
- Niente import/export configurazione o spegnimento automatico per batteria
  scarica. Non aggiungere questi comportamenti per iniziativa autonoma.
