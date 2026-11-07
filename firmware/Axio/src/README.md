# Moduli — Fase 4

Arduino compila ricorsivamente questa cartella. `web/DashboardRuntime.*`
collega il server STA non bloccante, gli asset flash compressi e gli handler
`DashboardService.h`. `DashboardHttp.h` limita RX/TX e verifica Host, Origin e
token; `JsonWriter.h` produce JSON senza DOM dinamico. `ImageSize.h` legge
soltanto gli header dell'immagine, evitando la verifica flash nel loop.
`power/Battery.h` media 16 letture ADC su A0/GPIO2 senza burst; percentuale
non stimata. `power/PowerController.h` gestisce conferma/ACK, SUSPEND con
readback, arresto servizi e wake GPIO5 LOW/pull-up.
Gli asset hanno sorgenti in `../../web/`; `scripts/package_web.py` aggiorna
l'header disponibile anche per Arduino IDE. Il nome comune mDNS non è attivo.
Vedere `../../../docs/phase4-verification.md` per evidenze e limiti.

 `../Axio.ino` coordina:

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
- `connectivity/ConnectivityManager.h`: policy STA e journal Wi-Fi separato;
  `ConnectivityRuntime.*`: driver ESP32 e comandi USB. `ApRuntime.*`, `FallbackAp.h` e `ProvisioningHttp.h`/`ProvisioningSocket.h`
  aggiungono AP WPA2 e provisioning limitato, con budget e socket non bloccanti.
- `connectivity/DiscoveryManager.h`: identità e cache peer, TTL, scheduler query
  e lifecycle testabili su host. `DiscoveryRuntime.*`: hostname DHCP/mDNS,
  servizio `_axio._udp` e adattatore query asincrone ESP-IDF, solo su STA.
- `transport/LegacyTransport.*`: lettura preferenze e OSC storici;
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

Metriche e limiti: `../../../docs/phase2-verification.md` e
`../../../docs/phase2-osc.md`. `osc/` contiene schema, codec, engine e journal;
`transport/OscRuntime.*` collega UDP e NVS. Il loop non usa più il serializer
storico, conservato per regressione. Il manager STA sostituisce WiFiManager;
La dashboard non è implementata. Policy, migrazione e limiti:
`../../../docs/phase3-connectivity.md`.
