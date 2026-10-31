# CB Skaner

**CB receiver for the Quansheng UV-K1 · Nasłuch CB na Quansheng UV-K1**

Author / Autor: **Kriss80 Elektryczny SP6HAX**

[English](#english) · [Polski](#polski)

---

## English

An overlay app for the **F4HWN Labs** firmware on the **Quansheng UV-K1 / UV-K5 V3**.
It listens to the 40 CB channels (27 MHz) and scans them. **Receive only**: the app never transmits.

### Features

- 40 CB channels, raster **"0"** (channel 19 = 27.180 MHz) or **"5"** (channel 19 = 27.185 MHz)
- AM / FM
- scanning: stops on a signal, resumes after 2 s of silence
- skip selected channels while scanning
- jump to the last active channel
- squelch that measures the noise itself and opens a set number of dB above it
- settings are kept between launches

### Keys

| Key | Action |
|---|---|
| UP / DOWN | channel ±1 (hold to repeat) |
| `*` | scan on / off |
| MENU | AM / FM |
| 1 / 7 | squelch −2 / +2 dB above the noise |
| 3 | raster "0" / "5" |
| 5 | skip the current channel while scanning (press again to restore) |
| 8 | jump to the last active channel |
| 9 | channel 19 |
| 0 | monitor (speaker always open) |
| EXIT | quit |

Defaults: channel 19, AM, raster "0", squelch 10 dB above the noise.

### Screen

```
SCAN (*)  RX                   100% [battery]
CB SKANER               AM 0 (3)
             CH 19          SKIP
         27.180 00
█████████████████|            (1-7)
-85 N-92 SQ+10 L30       v1.0 SP6HAX
```

#### Key hints on the screen

The numbers in brackets show which key changes the setting next to them:

| On screen | Key | What it does |
|---|---|---|
| `SCAN (*)` | `*` | scan on / off; the label is highlighted while scanning |
| `AM 0 (3)` | `3` | raster "0" / "5" (AM / FM is changed with MENU) |
| `(1-7)` after the signal bar | `1` / `7` | squelch −2 / +2 dB: moves the mark on the bar |

#### Labels

| Label | Meaning |
|---|---|
| `MON` | monitor on (key 0) |
| `RX` | receiving a signal |
| `WAIT` | signal just ended, waiting before scanning on |
| `SKIP` | this channel is skipped while scanning (key 5) |
| `CH 19` | channel number, frequency below it |
| `-85` | current signal (RSSI, dBm) |
| `N-92` | noise floor measured by the app (dBm) |
| `SQ+10` | squelch: how many dB above the noise a signal must be |
| `L30` | last active channel (here 30; key 8 jumps to it) |
| `v1.0 SP6HAX` | app version and the author's callsign |

The signal bar runs from −126 to −50 dBm; the vertical mark is the squelch threshold (noise + `SQ+`).
The app measures the noise floor by itself. If noise still opens the speaker, raise `SQ+` with 7;
if weak stations are missed, lower it with 1.

### Requirements

- Quansheng UV-K1 or UV-K5 V3
- **F4HWN Labs v6.3.0** firmware or newer (overlay API level 3)

### Installation

1. Download `CBSkaner-<version>.zip` from [Releases](https://github.com/Kriss80/uv-k1-cb/releases)
   and unzip it (GitHub does not accept `.app` attachments), or build it yourself (below).
2. Open [UV Studio](https://armel.github.io/uvstudio/) → **Apps** and upload the file.
   Do not use *Flash Firmware*: an `.app` is not a firmware.
3. On the radio, start **CBSkaner** from the Apps list (F, then 7).

### Building

Needs the [Arm GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)
(`arm-none-eabi-gcc`), `python3` and `curl`.

```bash
./build.sh
```

If the compiler is not on `PATH`:

```bash
ARM_TOOLCHAIN=/path/to/arm-gnu-toolchain ./build.sh
```

Output: `build/CBSkaner.app`.

The app uses the F4HWN firmware's app API. Those API files are not part of this repository:
`build.sh` fetches them at build time from
[armel/uv-k1-k5v3-firmware-custom](https://github.com/armel/uv-k1-k5v3-firmware-custom)
(the version in `F4HWN_REF`) into `build/f4hwn/`. To use a local firmware checkout instead:
`F4HWN_DIR=/path/to/uv-k1-k5v3-firmware-custom ./build.sh`.

### How it works

- Frequencies are computed from the CEPT/FCC channel plan: 10 kHz steps, five gaps in the grid
  and the swapped channels 23–25. Raster "0" is the same plan shifted by −5 kHz.
- Tuning goes through the BK4819 receiver registers. 27 MHz is below the VHF/UHF switch
  (280 MHz), so the app selects the VHF path.
- AM/FM is set the same way as in the F4HWN firmware.
- On exit the firmware restores the VFO settings.

### Notes

- The UV-K1 has no input filters for 27 MHz, so sensitivity is lower than on a CB radio.
- The stock antenna works poorly on 27 MHz; a CB antenna is much better.

### License

Apache License 2.0, see [LICENSE](LICENSE) and [NOTICE](NOTICE). Author: Kriss80 Elektryczny SP6HAX.

Runs on the [F4HWN](https://github.com/armel/uv-k1-k5v3-firmware-custom) firmware
(Armel F4HWN and contributors) and uses its app API.

---

## Polski

Aplikacja (overlay app) dla firmware **F4HWN Labs** na **Quansheng UV-K1 / UV-K5 V3**.
Nasłuch 40 kanałów CB (27 MHz) ze skanowaniem. **Tylko odbiór**: aplikacja nie nadaje.

### Funkcje

- 40 kanałów CB, raster **„0”** (kanał 19 = 27,180 MHz) lub **„5”** (kanał 19 = 27,185 MHz)
- AM / FM
- skanowanie: zatrzymanie na sygnale, wznowienie po 2 s ciszy
- pomijanie wybranych kanałów w skanowaniu
- skok na ostatnio aktywny kanał
- squelch, który sam mierzy szum i otwiera się zadaną liczbę dB powyżej niego
- ustawienia zapamiętywane między uruchomieniami

### Klawisze

| Klawisz | Działanie |
|---|---|
| UP / DOWN | kanał ±1 (przytrzymanie: powtarzanie) |
| `*` | skanowanie wł./wył. |
| MENU | AM / FM |
| 1 / 7 | squelch −2 / +2 dB powyżej szumu |
| 3 | raster „0” / „5” |
| 5 | pomiń bieżący kanał w skanowaniu (ponownie: przywróć) |
| 8 | skok na ostatnio aktywny kanał |
| 9 | kanał 19 |
| 0 | monitor (głośnik zawsze otwarty) |
| EXIT | wyjście |

Domyślnie: kanał 19, AM, raster „0”, squelch 10 dB powyżej szumu.

### Ekran

```
SCAN (*)  RX                   100% [bateria]
CB SKANER               AM 0 (3)
             CH 19          SKIP
         27.180 00
█████████████████|            (1-7)
-85 N-92 SQ+10 L30       v1.0 SP6HAX
```

#### Podpowiedzi klawiszy na ekranie

Cyfry i znaki w nawiasach pokazują, którym klawiszem zmienia się ustawienie obok:

| Na ekranie | Klawisz | Co robi |
|---|---|---|
| `SCAN (*)` | `*` | skanowanie wł./wył.; podczas skanowania napis jest podświetlony |
| `AM 0 (3)` | `3` | raster „0” / „5” (AM / FM zmienia się klawiszem MENU) |
| `(1-7)` za paskiem sygnału | `1` / `7` | squelch −2 / +2 dB: przesuwa kreskę na pasku |

#### Napisy

| Napis | Znaczenie |
|---|---|
| `MON` | monitor włączony (klawisz 0) |
| `RX` | odbiór sygnału |
| `WAIT` | sygnał się skończył, czekanie przed dalszym skanowaniem |
| `SKIP` | kanał pomijany w skanowaniu (klawisz 5) |
| `CH 19` | numer kanału, pod nim częstotliwość |
| `-85` | aktualny sygnał (RSSI, dBm) |
| `N-92` | szum zmierzony przez aplikację (dBm) |
| `SQ+10` | squelch: o ile dB sygnał musi przekroczyć szum |
| `L30` | ostatnio aktywny kanał (tu 30; klawisz 8 na niego przeskakuje) |
| `v1.0 SP6HAX` | wersja aplikacji i znak autora |

Pasek sygnału obejmuje od −126 do −50 dBm; pionowa kreska to próg squelcha (szum + `SQ+`).
Aplikacja sama mierzy poziom szumu. Jeśli szum dalej otwiera głośnik, podnieś `SQ+` klawiszem 7;
jeśli giną słabe stacje, obniż go klawiszem 1.

### Wymagania

- Quansheng UV-K1 lub UV-K5 V3
- firmware **F4HWN Labs v6.3.0** lub nowszy (API overlay poziom 3)

### Instalacja

1. Pobierz `CBSkaner-<wersja>.zip` z [Releases](https://github.com/Kriss80/uv-k1-cb/releases)
   i rozpakuj (GitHub nie przyjmuje załączników `.app`) albo zbuduj go sam (niżej).
2. Otwórz [UV Studio](https://armel.github.io/uvstudio/) → **Apps** i wgraj plik.
   Nie używaj *Flash Firmware*: `.app` to nie jest firmware.
3. W radiu uruchom **CBSkaner** z listy aplikacji (F, potem 7).

### Budowanie

Potrzebne: [Arm GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)
(`arm-none-eabi-gcc`), `python3` i `curl`.

```bash
./build.sh
```

Jeśli kompilator nie jest w `PATH`:

```bash
ARM_TOOLCHAIN=/ścieżka/do/arm-gnu-toolchain ./build.sh
```

Wynik: `build/CBSkaner.app`.

Dodatek korzysta z API aplikacji firmware F4HWN. Pliki tego API nie są częścią repozytorium:
`build.sh` pobiera je przy budowaniu z
[armel/uv-k1-k5v3-firmware-custom](https://github.com/armel/uv-k1-k5v3-firmware-custom)
(wersja w `F4HWN_REF`) do `build/f4hwn/`. Zamiast pobierania można wskazać lokalną kopię
firmware: `F4HWN_DIR=/ścieżka/do/uv-k1-k5v3-firmware-custom ./build.sh`.

### Jak to działa

- Częstotliwości są liczone wzorem z planu kanałów CEPT/FCC: krok 10 kHz, pięć przerw w siatce
  i przestawione kanały 23–25. Raster „0” to plan przesunięty o −5 kHz.
- Strojenie idzie przez rejestry odbiornika BK4819. 27 MHz jest poniżej przełącznika VHF/UHF
  (280 MHz), więc aplikacja włącza tor VHF.
- AM/FM ustawia się tak samo jak w firmware F4HWN.
- Po wyjściu firmware przywraca ustawienia VFO.

### Uwagi

- UV-K1 nie ma filtrów wejściowych na 27 MHz, więc czułość jest niższa niż w radiu CB.
- Fabryczna antena słabo działa na 27 MHz. Lepiej podłączyć antenę CB.

### Licencja

Apache License 2.0, zob. [LICENSE](LICENSE) i [NOTICE](NOTICE). Autor: Kriss80 Elektryczny SP6HAX.

Dodatek działa na firmware [F4HWN](https://github.com/armel/uv-k1-k5v3-firmware-custom)
(Armel F4HWN i współtwórcy) i korzysta z jego API aplikacji.
