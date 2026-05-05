# Walkthrough 2: Pipeline rebuildu feature'ow

## Funkcje
- `Application::process_feature_tree_actions()`
- `Application::execute_feature_rebuild(...)`
- `FeatureTree::rebuild(...)`
- `MainOrchestrator::submit_once(...)`
- `GeometryWorker::execute(...)`

## 1) Trigger z UI
`process_feature_tree_actions()` pobiera intencje z panelu:
1. `consume_open_plane_properties_request()`.
2. `consume_open_sketch_request()` -> `sketch_document_.enter()` i `solve()`.
3. `consume_rebuild_intent()` -> buduje `model::RebuildRequest`.
4. Wywoluje `execute_feature_rebuild(request)`.

## 2) Delegate i dispatch
`execute_feature_rebuild(...)`:
1. Tworzy lambda `delegate(payload)`.
2. Lambda mapuje JSON payload na `app::ipc::GeometryRequest` (`RebuildFeature`, `feature_id`, `full_rebuild`, `start_feature_id`).
3. Wysyla zadanie przez `g_orchestrator.submit_once(...)`.
4. Tlumaczy odpowiedz IPC na `model::RebuildDelegateResult`.

## 3) Iteracja po drzewie
`FeatureTree::rebuild(...)`:
1. Wyznacza start (`root` dla full rebuild lub `start_feature_id`).
2. Buduje porzadek topologiczny (`collect_topological_order`).
3. Dla kazdego feature'a niesupresowanego:
- tworzy payload JSON,
- enqueuje wywolanie delegate,
- czeka na wynik,
- ustawia stan feature'a (`Valid`, `Warning`, `Error`).
4. Po sukcesie wywoluje callbacki etapow:
- `tessellate`,
- `upload_mesh`,
- `repaint`.

## 4) Po stronie geometrii
`MainOrchestrator::submit_once` deleguje do `GeometryWorker::execute`.

`GeometryWorker::execute` dla kluczowych komend:
- `RebuildFeature`: tworzy/proponuje solid dla feature (`createBox` proxy).
- `RebuildFromSketch`: tworzy krawedzie z segmentow (`createEdge`).
- `PickSolid`: raycast (`pickSolid`) + pobranie krawedzi (`getEdges`).

## 5) Obsluga bledow
- `worker_crashed`: `execute_feature_rebuild` otwiera popup retry i umozliwia `restart_worker()`.
- `worker_failed`: oznacza feature jako `Error`.
- brak `feature_id/start_feature_id` przy partial rebuild: blad walidacji requestu w workerze.

## Dlaczego ten pipeline jest krytyczny
- To glowna droga przejscia od akcji UI do zmiany geometrii.
- Laczy 3 warstwy: UI/model -> IPC -> kernel geometrii.
- Kazda regresja w kontrakcie request/response destabilizuje caly edytor.