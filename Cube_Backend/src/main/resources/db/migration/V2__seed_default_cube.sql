-- Domyslna kostka do testow, zeby "Cube not found: CUBE-01" nie wracalo
-- po kazdym czyszczeniu bazy. Musi byc zgodne z CUBE_ID w konfiguracji ESP32.
insert into cubes (name, serial_number) values ('Moja kostka', 'CUBE-01');
