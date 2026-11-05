# Moduli Fase 2 — Checkpoint A

Arduino compila ricorsivamente questa cartella. `../Axio.ino` coordina:

- `core/BnoDevice.h`: adattatore hardware con letture controllate; pin esclusivamente
  da `../hardware_config.h` (relativo a `src`, ovvero `../../hardware_config.h`
  da `core`). Indirizzo, remap e cristallo invariati.
- `core/ImuManager.h`: inizializzazione, letture complete e ingresso unico tara.
  Device e clock sono parametri template minimi, senza classi astratte o task.
- `core/Calibration.h`: matematica originale, coniugato e offset yaw.
- `core/Sample.h`: dati grezzi, quaternion relativo, valori normalizzati,
  timestamp fine lettura e sequenza. Nessuna allocazione dinamica per campione.
- `core/TrackerState.h`, `core/Diagnostics.h`: ultimo valido, stato e metriche RAM.
- `core/Scheduler.h`: periodo storico 10 ms, senza catch-up, rollover unsigned.
- `transport/LegacyTransport.*`: preferenze, portale Wi-Fi e OSC storici;
  trasmette soltanto il campione passato, senza acquisire sensori.
- `diagnostics/SerialProtocol.h`: framing ASCII, ID, timeout e sintassi JSON limitata.
- `diagnostics/DiagnosticService.h`: risposte JSONL, stato, metriche e tara comune.
- `diagnostics/UsbDiagnostics.h`: poll USB con buffer fissi e budget RX/TX;
  nessuna stampa periodica, attesa o comando di rete.

Il campione è completo ma non atomico: i quattro blocchi BNO055 sono letti in
sequenza. `latest.valid` significa che esiste un ultimo campione valido; lo stato
`ReadError` segnala che l'ultimo tentativo è fallito e il campione è precedente.
Sequenza e timestamp non avanzano in caso di errore. I campioni falliti non sono
inviati. Non confondere l'assenza di errori I²C con qualità della fusione.

Metriche e limiti: `../../../docs/phase2-verification.md`. Il nuovo motore OSC
attende conferma al Checkpoint A; NetworkManager e dashboard non implementati.
