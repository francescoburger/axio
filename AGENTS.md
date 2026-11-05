# Axio — regole del repository

- Progetto Axio, repository axio; Seeed XIAO ESP32C3 e BNO055, macOS,
  Arduino IDE e CLI. Sketch principale: `firmware/Axio/Axio.ino`.
- Fase corrente: **0**. Non iniziare Fase 1, firmware 2.0, dashboard o nuovo
  motore OSC senza una nuova richiesta esplicita.
- `reference/HandTrackerV1_1.ino` è immutabile: niente rinomina, correzioni,
  formattazione o sostituzione. Hash atteso in `tests/test_baseline.py` e
  `docs/baseline-v1.1.md`. Anche i difetti storici vanno documentati, non corretti.
- Preservare esattamente la taratura di quaternion **e yaw**, ordine dei
  prodotti, cast verso zero, resto C++, precisione e ordine delle operazioni.
  Non sostituire con inverse, normalizzazioni, clamp o conversioni diverse.
- Centralizzare tutti i pin tramite `#define` in
  `firmware/Axio/hardware_config.h`. GPIO provvisori, verifica fisica utente.
- In Fase 0 la copia differisce dalla baseline soltanto per l'inclusione di
  quel file al posto delle due definizioni dei pin. Il test controlla i byte.
- Mantenere compatibilità Arduino IDE: cartella e sketch omonimi. `src/`
  contiene ora solo indicazioni, nessuna implementazione futura.
- Lavorare nel repository; niente installazioni, modifiche esterne o azioni
  distruttive senza approvazione. Niente pubblicazione o push remoto.
- Evitare dipendenze inutili. Script, build, cache e test generano file in
  `.build/`; catture in `captures/`. Non commettere credenziali o catture.
- Comandi: `bash scripts/compile.sh`, `bash scripts/compile.sh --baseline`,
  `python3 -B -m unittest discover -s tests -v`. Non effettuano upload.
- Classificare ogni verifica PASS, FAIL o NOT RUN; registrare limiti e warning.
  Nessun risultato hardware inventato; ricezione OSC non misura il rate IMU.

Vincoli delle fasi future, senza implementazione autorizzata ora:

- OSC configurabile, dashboard web LAN senza autenticazione, più reti Wi-Fi,
  fallback AP protetto, mDNS, pulsante, LED, batteria e deep sleep.
- Nessun obbligo di protocollo OSC Legacy nella versione finale.
- Nessun limite fisso a destinazioni o messaggi OSC prima dei benchmark.
- Niente import/export configurazione o spegnimento automatico per batteria
  scarica. Non aggiungere questi comportamenti per iniziativa autonoma.
