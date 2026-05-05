# Walkthrough 4: I/O modelu (deserializacja i serializacja)

## Funkcje
- `ModelDeserializer::from_file(path)`
- `ModelDeserializer::from_string(json_text)`
- `ModelDeserializer::resolve_parameters(parameters)`
- `ModelSerializer::to_json(tree, options)`
- `ModelSerializer::save_to_file(path, tree, options)`

## 1) Ladowanie z dysku
`from_file(path)`:
1. Otwiera plik binarnie.
2. Czyta cala zawartosc do stringa.
3. Wywoluje `from_string(data)`.

Bledy:
- brak pliku,
- pusty plik,
- blad odczytu.

## 2) Parse i walidacja modelu
`from_string(json_text)`:
1. Parsuje JSON (`parse(..., false)`), sprawdza `is_discarded`.
2. Weryfikuje root object.
3. Sprawdza `schema_version` (aktualnie `1.0` lub integer `1`).
4. Wymaga `features[]`.
5. Opcjonalnie rozwiązuje `parameters` przez `resolve_parameters`.
6. Parsuje features do tymczasowej postaci `ParsedFeature`.
7. Buduje runtime `FeatureTree`:
- mapy id zrodlo->runtime,
- relacje parent/children,
- ochrona przed cyklami (`visiting`/`visited`),
- tworzenie node'ow `create_feature`.
8. Ustawia stany dodatkowe (`expanded`, `suppressed`, `object_data`).
9. Zwraca gotowe drzewo lub blad `ModelError`.

## 3) Rozwiazywanie parametrow
`resolve_parameters(parameters)`:
1. Dla kazdego klucza uruchamia `resolve_parameter_impl`.
2. Obsluguje odwolania przez prefiks `#`.
3. Cache'uje rozwiazane wartosci.
4. Blokuje cykle referencji.

## 4) Budowanie JSON
`ModelSerializer::to_json(tree, options)`:
1. Rekurencyjnie zbiera features (`collect_features`).
2. Doklada payload per feature, metadata, parameters, session.
3. Dla extrude utrzymuje logiczny `sketch_id`.
4. Ustawia zaleznosci `dependencies.id` jako lancuch kolejnosci.
5. Sortuje features po `id`.
6. Sklada finalny root z `schema_version`, `name`, `units`, `features`, `metadata`, `session`.

## 5) Zapis do pliku
`save_to_file(path, tree, options)`:
1. Tworzy katalogi docelowe.
2. Otwiera plik z `trunc`.
3. Zapisuje wynik `to_string`.
4. Zwraca `output.good()`.

## Kontrakt wejscia/wyjscia
- Deserializator: `expected<FeatureTree, ModelError>`.
- Serializator: `bool` i jawny zapis do pliku.

## Ryzyka regresji
- Zmiana schematu bez migracji.
- Rozjazd mapowania typow enum <-> string.
- Bledna obsluga referencji parametrow `#name`.
- Niepelne odtworzenie zaleznosci parent/child i suppression state.