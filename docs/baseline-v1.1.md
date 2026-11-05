# Baseline HandTracker V1.1

Sorgente immutabile: `reference/HandTrackerV1_1.ino`, SHA-256:

```text
40c8113e8cb8d0ef0229ffc50282933e889a94c3e8795f8dac942a1968aaf259
```

All'ispezione il repository non aveva commit; il file reference era già
presente ma non tracciato. Non è stato modificato, rinominato o formattato.
La copia `firmware/Axio/Axio.ino` cambia solo le due definizioni dei pin in
un include di `hardware_config.h`. Un test confronta i byte e verifica l'hash.

## Dipendenze e inizializzazione

Core ESP32 3.3.12: WiFi, DNSServer, WiFiClient, WiFiUdp, Preferences, Wire.
Librerie: OSC CNMAT 4.0.0, WiFiManager 2.0.17, Adafruit BNO055 1.6.4, Adafruit
Unified Sensor 1.1.15, Adafruit BusIO 1.17.4. Non conosciamo le versioni usate
per la build storica: queste sono le versioni presenti e verificate ora.

`setup()` imposta CPU 160 MHz e seriale 115200, carica preferenze e avvia
Wi-Fi/portale e UDP, poi inizializza il sensore. `Wire.begin(5,6)` specifica
GPIO SDA=5 e SCL=6, non etichette D5/D6. L'oggetto BNO055 ha sensor ID 55 e
indirizzo I²C `0x29`; `bno.begin()` usa NDOF per default nella libreria presente.
Se fallisce, imposta `imuok=0` e non ritenta. Se riesce attende 1000 ms,
applica `REMAP_CONFIG_P0=0x21`, `REMAP_SIGN_P0=0x04` e abilita il cristallo esterno.
Non legge lo stato di calibrazione interna e non salva gli offset del sensore.

Il core installato definisce SDA=6/SCL=7 e D3=5/D4=6. La baseline usa quindi
D3/D4. Questi valori sono provvisori: non sono stati corretti automaticamente.
Fonte: [variante ufficiale XIAO ESP32C3, core 3.3.12](https://github.com/espressif/arduino-esp32/blob/3.3.12/variants/XIAO_ESP32C3/pins_arduino.h).

## Wi-Fi e preferenze

Namespace NVS `configs`; al primo avvio chiave `init=true`, `targetIp=0.0.0.0`,
`port=16888`, `id=0`. Le credenziali Wi-Fi sono gestite da WiFiManager.
Parametri del portale: IP (buffer 40), porta (5), ID (3). Il callback converte
ID con `atoi`, porta con `atol` verso unsigned int, usa `IPAddress.fromString`,
salva NVS e riavvia il portale web. Nessuna validazione esplicita degli esiti.

Hostname e SSID `Tracker_<DEVICE_ID>`, buffer 16; configurazione non bloccante
con `setBreakAfterConfig(true)` e `setConfigPortalBlocking(false)`. `autoConnect`
è chiamata senza password AP. Se restituisce false viene stampato un errore,
ma setup continua. `startWebPortal()` avvia il portale storico; `wm.process()`
viene eseguito a ogni loop. Il nome viene costruito solo in setup, perciò un
cambio ID non lo aggiorna immediatamente. Non sono implementati qui i requisiti
Wi-Fi/dashboard della versione futura.

## Ciclo e protocollo OSC storico

Ogni loop esegue il portale, controlla `millis()-lastSensorUpdate >= 10`,
assegna `lastSensorUpdate=currentMillis` e invoca `bno_loop()`, poi controlla OSC
entrante. Non recupera intervalli persi. L'unsigned subtraction tollera il wrap
di `millis()` nella normale finestra temporale; non costituisce uno scheduler
real time. Se `imuok=0`, `bno_loop()` ritorna subito.

Ogni acquisizione legge quaternion, Euler, accelerazione lineare e giroscopio
in operazioni distinte e invia un bundle con quattro messaggi, in quest'ordine:

| Indirizzo | Tipi | Valori |
| --- | --- | --- |
| `/ypr<ID>` | `,fff` | yaw, pitch, roll normalizzati |
| `/real<ID>` | `,fff` | accelerazione lineare sensore /35 |
| `/world<ID>` | `,fff` | accelerazione ruotata con quaternion relativo /35 |
| `/gyro<ID>` | `,fff` | componenti gyro troncate /1500 |

Destinazione `outIp:outPort`, default porta 16888; socket locale 16889. Gli
indirizzi vengono formattati in `char addr[16]` con `snprintf`. Nessun quaternion
è trasmesso. Con OSC 4.0.0 il bundle di default usa timetag immediato, non un
timestamp di acquisizione. Nessun contatore, stato IMU o controllo dell'esito UDP.

In ricezione viene consumato al massimo un datagramma a loop, interpretato come
**OSCMessage**, non OSCBundle. Se valido, `dispatch("/tare", osctare)` invoca
la taratura, indipendentemente dagli argomenti; nessuna risposta o autenticazione.
La taratura non è protetta da `imuok`: anche con sensore assente può leggere I²C.

## Taratura e quaternion: formule da conservare

La taratura utente è una tara di riferimento, distinta dalla calibrazione
interna Bosch. Le due letture non sono atomiche:

```cpp
imu::Quaternion quat = bno.getQuat();
imu::Vector<3> euler = bno.getVector(Adafruit_BNO055::VECTOR_EULER);
quatOffset = quat.conjugate();
yawOffset = euler.x();
```

`yawOffset` è `int`: assegnare un double tronca verso zero (359.9375 → 359,
-0.9375 → 0). Il quaternion usa double nella libreria installata. Dato il
quaternion di tara `t=(tw,tx,ty,tz)`, offset `o=(tw,-tx,-ty,-tz)`: **coniugato**,
senza divisione per norma al quadrato. Prima della prima tara, offset identità
e yawOffset=0. Non c'è tara automatica in setup, né persistenza della tara.

A ogni ciclo `quatRelative = quatOffset * quat` in quest'ordine. Con quaternion
corrente `u=(uw,ux,uy,uz)`, il prodotto Hamilton della libreria è:

```text
w = ow*uw - ox*ux - oy*uy - oz*uz
x = ow*ux + ox*uw + oy*uz - oz*uy
y = ow*uy - ox*uz + oy*uw + oz*ux
z = ow*uz + ox*uy - oy*ux + oz*uw
```

Le operazioni di ogni riga sono valutate da sinistra a destra come nel C++.
Poi i quattro double vengono assegnati a `float q0,q1,q2,q3` (w,x,y,z).
Nessuna normalizzazione o scelta del segno q/-q. Alla stessa posa, solo se t è
unitario il prodotto con il coniugato dà identità; per t=(2,0,0,0) dà (4,0,0,0).
Tare ripetute sostituiscono il riferimento precedente, non lo accumulano.

## Euler e cast esatti

```cpp
yaw   = ((((int)euler.x() - yawOffset + 360 + 180) % 360) / 360.0) * 2 - 1;
roll  = -1 * (int)euler.z() / 180.0;
pitch = (int)euler.y() / 90.0;
```

1. Cast ogni componente verso zero, non floor/round. Lo yawOffset era già
   troncato separatamente: `int(x)-int(t)` non equivale a `int(x-t)`.
2. Per yaw: sottrazione int, aggiunta **360 poi 180**, resto int `%360`, divisione
   double per 360.0, moltiplicazione per 2, sottrazione 1, assegnazione float.
3. Per roll: `-1 * int(z)` avviene come intero prima della divisione double.
   Pitch divide `int(y)` per 90.0; entrambi vengono assegnati a float.

Per heading e riferimento nell'intervallo normale [0,360), lo yaw produce
livelli a passi di 1/180 in [-1,179/180], con discontinuità a differenza +180°:

| heading | riferimento | yaw prima dell'arrotondamento float |
| --- | --- | --- |
| 0 | 0 | 0 |
| 179.9375 | 0 | 179/180 |
| 180 | 0 | -1 |
| 359.9375 | 0 | -1/180 |
| 0 | 359.9375 | 1/180 |
| 10.9 | 10.1 | 0 (entrambi troncati a 10) |

Il resto C++ ha il segno del numeratore: con x=-901 e riferimento 0 il numeratore
è -361, il resto -1 e yaw è circa -1.00556. Il +540 non è un wrap generale per
qualsiasi input. Nessun clamp su yaw/pitch/roll. I commenti storici attribuiscono
pitch a ±90 e roll a ±180, ma il codice usa y/90 e -z/180: verificare convenzioni
e orientamento fisico, senza scambiare assi in Fase 0. Input non finiti, cast
fuori dal range int o overflow delle operazioni int non hanno un risultato C++
portabile: non inventare valori di riferimento per questi casi.

## Accelerazione: ordine e precisione

`VECTOR_LINEARACCEL` restituisce m/s². Le letture double sono assegnate ai float
axReal, ayReal, azReal; poi la rotazione usa i float quaternion **relativi**:

```cpp
axWorld = (1 - 2 * q2 * q2 - 2 * q3 * q3) * axReal + (2 * q1 * q2 - 2 * q0 * q3) *
          ayReal + (2 * q1 * q3 + 2 * q0 * q2) * azReal;
ayWorld = (2 * q1 * q2 + 2 * q0 * q3) * axReal + (1 - 2 * q1 * q1 - 2 * q3 * q3) *
          ayReal + (2 * q2 * q3 - 2 * q0 * q1) * azReal;
azWorld = (2 * q1 * q3 - 2 * q0 * q2) * axReal + (2 * q2 * q3 + 2 * q0 * q1) *
          ayReal + (1 - 2 * q1 * q1 - 2 * q2 * q2) * azReal;
```

Prodotti e somme qui sono float, con parentesi e associatività riportate; le
componenti sensore restano non scalate finché tutte e tre le righe sono calcolate.
Poi, nell'ordine axReal, ayReal, azReal, axWorld, ayWorld, azWorld, ogni componente
esegue `value /= 35.0`: promozione a double, divisione e ritorno a float.
Il commento storico cita ±40 m/s², ma il divisore effettivo è **35**. Nessun clamp:
70 m/s² produce 2. La denominazione «world» usa un riferimento dipendente dalla
tara, non garantisce un sistema mondo assoluto. Quaternion non unitari deformano
anche la matrice. Non anticipare la divisione o sostituire questa matrice.

## Giroscopio

```cpp
gx = int(gyro.x()) / 1500.0;
gy = int(gyro.y()) / 1500.0;
gz = int(gyro.z()) / 1500.0;
```

Cast double→int verso zero, divisione double, assegnazione float. Esempi:
±0.99 → 0; 1.99 → 1/1500; -1.99 → -1/1500; 3000 → 2. Nessun clamp.
Il commento V1.1 dice rad/s, mentre `getVector(VECTOR_GYROSCOPE)` nella libreria
1.6.4 divide i registri per 16, con unità dichiarata gradi/s; non applica la
conversione in radianti usata altrove da `getEvent`. Conserviamo il codice.
Fonte: [implementazione ufficiale Adafruit BNO055 1.6.4](https://github.com/adafruit/Adafruit_BNO055/blob/1.6.4/Adafruit_BNO055.cpp).

## Rischi storici documentati, non corretti

- Pin diversi dalla variante XIAO; indirizzo 0x29 e cristallo esterno da verificare.
- Taratura Euler/quaternion e letture IMU sequenziali: non rappresentano
  necessariamente lo stesso istante. Nessun controllo della qualità della fusione.
- Troncamenti, discontinuità yaw, nessun clamp, coniugato senza normalizzazione,
  frame «world» relativo alla tara, commenti non coerenti con scalatura/unità.
- Target iniziale 0.0.0.0, assenza di validazione e controlli degli errori di rete.
  AP storico senza password; nessun filtro del mittente della tara.
- `sprintf(wifiName, "Tracker_%d", DEVICE_ID)` in buffer 16 non limita la
  lunghezza: ID int arbitrari da NVS possono eccederlo. I buffer del portale
  possono limitare l'immissione di porte a cinque cifre e ID lunghi (lo spazio
  include il terminatore); verificare UI reale. Non cambiare in questa fase.
- Nessun retry applicativo IMU, tara consentita anche se inizializzazione fallita;
  `bnoInitialized` dichiarata ma inutilizzata. Assenza di timestamp/sequence ID.
- Periodo richiesto 10 ms: non prova 100 acquisizioni distinte/s, né ricezione
  affidabile a 100 pacchetti/s. Elaborazione portale, I²C e rete incidono sul timing.

I test host eseguono taratura, handler di tara, acquisizione/calcoli e controllo
IMU assente con adattatori, usando le vere operazioni imumaths installate. Un
oracolo Python distinto riproduce float32, cast e resto C++. Copertura: 1.345
vettori deterministici, gradi yaw e riferimenti frazionari, limiti/segni, quaternion
non commutativi/non unitari, accelerazioni oltre scala, gyro sotto un grado/s,
250 campioni casuali con seed fisso. Confronto con tolleranza assoluta 1e-7 o
relativa 2e-6 per differenze host/target; non è una prova bit per bit ESP32 né
un test fisico. La fedeltà testuale della copia è verificata separatamente.

## Aggiornamento hardware successivo alla Fase 0

L'utente ha confermato SDA GPIO6 (D4), SCL GPIO7 (D5), BNO055 0x29.
La configurazione corrente è in `firmware/Axio/hardware_config.h`; i valori
5/6 riportati sopra descrivono soltanto la baseline storica, che resta immutata.
La conferma del cablaggio non sostituisce le prove funzionali hardware NOT RUN.
