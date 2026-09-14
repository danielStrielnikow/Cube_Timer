# Cube Timer

Projekt na ESP32. Ma być timerem do kostki Rubika, który wykrywa ruch kostki
za pomocą akcelerometru i pokazuje dane na małym wyświetlaczu OLED.

## Co to robi teraz

Program czyta dane z akcelerometru MPU6050 (osie X, Y, Z) przez I2C i
wyświetla te wartości na ekranie OLED SSD1306 (128x32 pikseli). Do tego
mierzy czas: gdy wykryje ruch kostki, uruchamia stoper, a gdy kostka
stoi bez ruchu przez sekundę, stoper się zatrzymuje. Czas jest pokazywany
na czwartej linii wyświetlacza.

## Sprzęt

- ESP32 (płytka esp32dev)
- Akcelerometr MPU6050, adres I2C: 0x68
- Wyświetlacz OLED SSD1306 128x32, adres I2C: 0x3C

## Podłączenie (I2C)

- SCL -> GPIO 22
- SDA -> GPIO 21

## Czego potrzeba do zbudowania

- Zainstalowany PlatformIO
- Framework: ESP-IDF (w platformio.ini ustawione jako `espidf`)

## Jak zbudować i wgrać program

```
pio run
pio run --target upload
pio device monitor
```

## Struktura projektu

- `src/main.c` - główny plik programu, punkt wejścia to `app_main()`
- `components/esp_ssd1306` - biblioteka do obsługi wyświetlacza OLED
- `components/esp_type_utils` - pomocnicze funkcje i typy
- `platformio.ini` - konfiguracja PlatformIO (płytka, framework)

## Jak działa pomiar czasu

Stoper ma trzy stany: czeka na ruch, idzie, zatrzymany.

- Program za każdym razem porównuje nowy odczyt X/Y/Z z poprzednim. Jeśli
  różnica jest większa niż `MOVEMENT_THRESHOLD`, uznajemy że ktoś rusza
  kostką.
- Pierwszy wykryty ruch startuje stoper.
- Jeśli kostka stoi bez ruchu dłużej niż `STILL_TIME_TO_STOP_US`
  (obecnie 1 sekunda), stoper się zatrzymuje - zakładamy, że kostka
  została odłożona po ułożeniu.
- Kolejny ruch po zatrzymaniu zeruje i uruchamia stoper od nowa.

To proste podejście na podstawie progu różnicy w odczytach z
akcelerometru, bez żadnego filtrowania szumu. Progi (`MOVEMENT_THRESHOLD`,
`STILL_TIME_TO_STOP_US`) mogą wymagać dostrojenia pod konkretny
egzemplarz czujnika.

## Zrobione poprawki

- Dodane sprawdzanie błędów (`ESP_ERROR_CHECK` / logowanie) po
  wywołaniach I2C i po `ssd1306_init`.
- Magiczne liczby (adres rejestru, próg ruchu) mają teraz nazwy, np.
  `MPU6050_REG_ACCEL_XOUT_H`, `MOVEMENT_THRESHOLD`.
- Usunięte nieużywane stałe pinów, zamiast nich jedna para
  `PIN_I2C_SCL` / `PIN_I2C_SDA` używana w konfiguracji I2C.
- `src/main` (skompilowany plik binarny) i `.idea/` dodane do
  `.gitignore` i usunięte z repo.

## TODO

- Dostroić progi wykrywania ruchu (`MOVEMENT_THRESHOLD`,
  `STILL_TIME_TO_STOP_US`) na prawdziwym sprzęcie
- Dodać wyświetlanie najlepszego/ostatniego czasu, nie tylko
  aktualnego
- Dodać jakiś sposób ręcznego resetu stopera (np. przycisk)
