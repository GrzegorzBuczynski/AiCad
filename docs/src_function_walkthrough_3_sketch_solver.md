# Walkthrough 3: Sketch + solver ograniczen

## Funkcje
- `SketchDocument::enter()`
- `SketchDocument::exit()`
- `SketchDocument::solve()`
- `SketchSolver::solve_incremental(...)`
- `SketchSolver::estimate_dof(...)`

## 1) Aktywacja trybu szkicu
- `enter()` ustawia `active_ = true`.
- `exit()` ustawia `active_ = false`.

To flaga steruje, czy UI uruchamia logike szkicowania i czy petla aplikacji przepuszcza odpowiednie akcje.

## 2) Kroki `SketchDocument::solve()`
1. `sync_lines_from_points()` - linie aktualizowane na podstawie punktow kontrolnych.
2. `solver_.solve_incremental(entities_, constraints_)` - rozwiazywanie ukladu ograniczen.
3. `sync_points_from_lines()` - odswiezenie punktow po korektach solvera.
4. Ponowne `sync_lines_from_points()` dla konsystencji.
5. Zapis `last_result_` i zwrot `SolveResult`.

## 3) Kroki `SketchSolver::solve_incremental(...)`
1. Liczy DOF: `estimate_dof(entities, constraints)`.
2. Dla maksymalnie `max_iterations` iteracji:
- przechodzi po wszystkich constraintach,
- dla kazdego liczy residual przez `apply_constraint`,
- sledzi `max_residual`.
3. Gdy `max_residual <= 1e-3`, oznacza `converged=true` i konczy.
4. Zwraca `SolveResult` (`converged`, `dof`, `iterations`, `max_residual`).

## 4) Znaczenie DOF
`estimate_dof`:
- zlicza parametry encji (punkt/linia/arc/circle),
- zlicza rownania z constraintow,
- zwraca roznice: `parametry - rownania`.

Interpretacja:
- `DOF > 0`: szkic niedodefiniowany,
- `DOF = 0`: szkic izostatyczny,
- `DOF < 0`: szkic nadmiernie zwiazany.

## 5) Ryzyka i typowe problemy
- Brak zbieznosci przy konfliktach ograniczen.
- Lokalna stabilnosc zalezy od kolejnosci i charakteru constraintow.
- Zla synchronizacja line<->point psuje preview i profile.

## Integracja z aplikacja
- `Application::build_docked_layout()` uruchamia `solve()` gdy sketch jest aktywny.
- `Application::init()` uruchamia `solve()` raz po starcie projektu.
- Przy wyjsciu ze szkicu segmenty sa serializowane do IPC (`RebuildFromSketch`).