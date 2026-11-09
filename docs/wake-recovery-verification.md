# Recupero accensione dopo revisione round 5

L'utente segnala che il dispositivo non si accende dopo la modifica dello sleep
senza rilascio. Il funzionamento del ciclo fisico wake/sleep della revisione
round 5 è quindi FAIL; le verifiche USB/HTTP dopo upload erano riuscite, ma non
coprivano tale ciclo. I test host avevano verificato la logica senza modellare
le resistenze interne selezionate dal core ESP32.

## Diagnosi

Il round 5 armava il wake GPIO HIGH per intercettare il rilascio del pulsante.
Il cablaggio è pulsante verso GND con pull-up interno. Il core ESP32 installato
ha `CONFIG_ESP_SLEEP_GPIO_ENABLE_INTERNAL_RESISTORS=1`; il suo `esp_sleep.h`
documenta che `esp_deep_sleep_start()` imposta automaticamente le resistenze
secondo il modo wake. La configurazione HIGH usa pull-down: senza pull-up esterno
il rilascio può non generare un livello HIGH. È una spiegazione coerente con il
blocco segnalato, non una misura elettrica sul dispositivo.

Fonti locali: toolchain esp32c3-libs 3.3.12, `include/esp_hw_support/include/esp_sleep.h`
(note di `esp_deep_sleep_enable_gpio_wakeup`) e `qio_qspi/include/sdkconfig.h`.

## Correzione

- Eliminati wake HIGH e flag RTC `releasePending`; wake sempre LOW con pull-up.
- A 5 s il sensore viene sospeso e le comunicazioni arrestate anche se il
  pulsante rimane premuto. Un nuovo stato, `waiting_sleep_release`, mantiene
  sensore/rete disattivati fino al rilascio stabile di 100 ms.
- Solo allora viene chiamato `esp_deep_sleep_start()`. Nessun tentativo di
  entrare in deep sleep con il livello di wake ancora attivo, nessun risveglio
  intermedio al rilascio. Il vero consumo deep sleep inizia dopo il rilascio;
  non si dichiara spegnimento elettrico della scheda.
- Conservati soglia interna wake 1,6 s, istruzioni circa 3 s e calibrazione breve.
  Nessuna modifica a NVS, reti, taratura matematica o baseline storiche.

## Stato verifiche

| Verifica | Esito | Evidenza |
| --- | --- | --- |
| Segnalazione utente sul firmware round 5 | FAIL | non si accende più dopo la modifica |
| Test mirati power/wake | PASS | `.build/wake-recovery-targeted.log`, 3 test; include attesa rilascio con sensore/rete già fermi, debounce e ripristino dopo errore |
| Suite completa | PASS | 111 test; `.build/wake-recovery-tests.log`; 1.345 vettori matematici invariati |
| Compilazione recupero | PASS | `.build/wake-recovery-compile.log`; 1.149.265 B flash (87%), 111.172 B RAM statica (33%) |
| Upload recupero | PASS | utente conferma porta tornata visibile dopo RESET; firmware caricato sulla XIAO AC:27:6E:7E:32:38, hash verificato; `.build/wake-recovery-upload.log` |
| USB/HTTP dopo recupero | PASS | IMU ready, STA 192.168.8.159, 30 campioni web validi, reti/hostname e configurazione OSC completa conservati; `.build/wake-recovery-hardware.log`, `captures/wake-recovery-before.json`, `captures/wake-recovery-after.json` |
| Nuovo ciclo fisico spegnimento/accensione | PASS manuale riferito dall'utente | dopo upload del recupero, alla richiesta di provare spegnimento a 5 s, rilascio e successiva accensione con pressione di circa 3–4 s, l'utente conferma «Sì funziona»; nessuna misura strumentale di durata o consumo |

Il reset fisico richiesto è RESET della scheda, non `DIAG RESET`, cancellazione
flash o reset di fabbrica. Nessun comando di calibrazione o cancellazione
configurazione autorizzato/eseguito dal recupero.

La prima suite completa ha riportato una risposta HTTP troncata durante il
passaggio AP/STA del runtime host: `wait_snapshot()` ritenta già errori di
connessione durante questa transizione, ed è stato esteso al caso specifico
`http.client.IncompleteRead`. Nessuna modifica al server HTTP di produzione.
Prima esecuzione conservata in `.build/wake-recovery-tests-first.log`.
