# Pulsante e indicatore batteria — 7 novembre 2026

L'utente ha approvato esplicitamente pressione di 3 s e ingresso in sleep al
rilascio, e la tensione misurata nell'header in attesa di una curva percentuale.

## Cause e correzioni

Prima della correzione il pulsante era soltanto sorgente di wake: mancava
un comando fisico di sleep. Il firmware ora configura il pull-up GPIO5 al boot
e controlla il pin a ogni tick, indipendentemente da Wi-Fi, browser o margine
di lavoro HTTP. La pressione deve durare almeno 3000 ms; l'evento viene emesso
solo al rilascio stabile (antirimbalzo 30 ms). Una pressione mantenuta oltre
3 s non avvia anticipatamente la sequenza. L'evento confermato usa lo stesso
PowerController della dashboard: rilascio stabile 100 ms, CONFIG, SUSPEND
verificato, stop ordinato, wake GPIO5 LOW, sleep. Nessuna tara o nuova azione
su pressione breve in funzionamento normale.

Al boot/wake serve prima un rilascio stabile: tenere premuto il pulsante di
risveglio non riaddormenta il tracker. Una pressione durante una transizione
power, un errore power, un'attivazione rete differita o sensore non pronto non
viene recuperata in ritardo; occorre un nuovo gesto quando il dispositivo è
pronto. Debounce e soglia usano sottrazione unsigned, anche al wrap di millis.

La misura ADC funzionava già: la riproduzione fisica ha letto 3,506 V con
percentuale null. L'header selezionava solo percent e mostrava sempre «—».
Ora mostra la tensione con due decimali (es. 3.51 V), e conserva i tre decimali
nel tooltip/etichetta accessibile. Nessuna percentuale o barra di carica viene
dedotta arbitrariamente. La formula e il pin A0/GPIO2 non sono cambiati.
Se la media iniziale non è pronta, riprova dopo 1 s; quando disponibile torna
alla cadenza 30 s, senza polling in pagina nascosta. Un errore cancella anche
valore precedente, tooltip e riempimento icona. La UI Overview spiega il gesto
nella descrizione Deep Sleep esistente, senza aggiungere controlli.

## Verifiche

- PASS: 83 test automatici e 1.345 vettori baseline/core. Nuove prove coprono
  pressioni brevi, rimbalzi, soglia esatta, rilascio, pulsante tenuto al wake,
  stato busy e wrap unsigned. Il runtime C++ completo con GPIO/radio/BNO finti
  verifica ingresso sleep e SUSPEND senza rete né browser.
- PASS: browser con DOM reale su cinque pagine, percentuali di prova, volt,
  misura assente, errore dopo una misura valida e retry iniziale reale a 1 s.
  Dati di prova soltanto nella sonda, nessun fallback fittizio nel frontend.
- PASS: build firmware, flash 1.148.331 / 1.310.720 B; RAM statica 116.732 B.
  Resta il warning esterno noto della libreria OSC (implicit fallthrough).
- PASS: upload sul principale `/dev/cu.usbmodem2101`; confronto prima/dopo di
  OSC e reti identico, IMU pronta, asset nuovi e tensione disponibile (3,625 V
  nello snapshot dopo upload). Evidenza e hash artefatto:
  `captures/phase4-button-battery-update-01.json`.
- PASS: browser sulle cinque pagine servite dalla XIAO reale, tensione visibile
  e assenza di overflow a 430 CSS px/DPR 2. Catture aggiornate in
  `captures/phase4-device-ui/`, incluso `comparison.html`.
- PASS: gesto manuale pressione prolungata/rilascio, disconnessione, successivo
  wake GPIO (causa 7), ritorno IMU/STA e permanenza in stato idle. OSC e reti
  identici. L'osservatore non ha inviato sleep/reset/tare. Prima cattura:
  `captures/phase4-physical-button-01.json`. Questa sonda ha rilevato il percorso
  ma è terminata FAIL per timeout dell'ultimo GET (limite 0,8 s).
- PASS: follow-up in sola lettura con timeout 3 s, mantenendo la cattura FAIL.
  12/12 GET HTTP 200, latenza osservata 0,021–1,048 s, wake cause 7 e nessun
  nuovo sleep. IMU 99,6605 Hz su 6,562 s, zero errori lettura, configurazioni
  conservate, batteria 3,665 V. Evidenza:
  `captures/phase4-physical-button-02.json`. La latenza massima supera la soglia
  della prima sonda; la causa di quel singolo timeout non è stata isolata.
  Queste finestre non costituiscono endurance o certificazione di jitter/consumi.

La prima suite rilanciata con redirezione del log è stata bloccata dal sandbox
sui socket localhost (HTTP/UDP). Rilancio autorizzato fuori sandbox: 83/83 PASS.
Log finali: `.build/phase4-power-battery-tests-final.log`,
`.build/phase4-button-battery-upload.log`. Nessuna modifica ai riferimenti,
nessun commit/push. Il secondo tracker non era collegato e non è stato aggiornato
con questa revisione; conserva la precedente build Fase 4.
