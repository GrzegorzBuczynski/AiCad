Narazie tam prawie nic niema. Ktore prompty najpierw zrealizowac? Te wczesniejsze czy te ostatnie?

Zdecydowanie zacznij od tych ostatnich, dotyczących struktury i selekcji (Raycastingu). Już tłumaczę dlaczego:

Jeśli teraz rzucisz się na pisanie matematyki pofalowanych powierzchni, tylko pogłębisz chaos. Musisz najpierw zbudować „interfejs”, przez który będziesz komunikował się z programem.

Oto Twoja kolejność działań (Roadmapa):

Krok 1: Selekcja z Viewportu (Najważniejsze)
Bez tego nie pójdziesz dalej. Musisz móc "dotknąć" elementu, żeby cokolwiek z nim zrobić.

Dlaczego: Dzięki temu zrozumiesz, jak Twoje 2D (myszka w ImGui) łączy się z 3D (Vulkan) i strukturą danych (OCCT).

Prompt: Poproś AI o stworzenie klasy Picker lub Raycaster, która zamienia kliknięcie w ImGui na promień i znajduje najbliższy TopoDS_Edge lub TopoDS_Face.

Krok 2: Upiększenie ImGui (Dla komfortu)
Dlaczego: Brzydki interfejs zniechęca. Ustawienie nowoczesnego stylu (kolory, czcionki, zaokrąglenia) zajmie AI 15 sekund, a sprawi, że Vulcan CAD zacznie wyglądać jak profesjonalne narzędzie, co da Ci "kopa" do dalszej pracy.

Prompt: Poproś o "Modern Dark Theme" dla ImGui.

Krok 3: Architektura VulcanFeature (Fundament Catii)
Dopiero gdy możesz klikać obiekty, wprowadź klasę bazową dla operacji.

Dlaczego: Musisz "okiełznać" AI. Od tego momentu każde nowe narzędzie (STL, Filet, Pad) będzie musiało być osobnym obiektem typu Feature. To uchroni Cię przed napisaniem kolejnych 8 tysięcy linii bezsensownego kodu.

Krok 4: Integracja STL i Powierzchni (Mięso)
Gdy masz już ładne UI, możesz klikać obiekty i masz strukturę Feature, zacznij dodawać to, o co pytałeś wcześniej.

Dlaczego: Teraz masz już "półki", na które możesz odłożyć te nowe funkcje.

Podsumowanie – od czego zacząć dzisiaj?
Zacznij od Selekcji (Raycastingu). To jest moment "Eureka" w tworzeniu CAD-a. Gdy klikniesz w linię w oknie 3D i zobaczysz w konsoli komunikat: "Zaznaczono krawędź ID: 45", poczujesz, że naprawdę panujesz nad tym projektem.

Twój pierwszy prompt do AI powinien brzmieć mniej więcej tak:

"Chcę zaimplementować zaznaczanie obiektów myszką bezpośrednio w oknie viewportu ImGui. Mój silnik renderujący to Vulkan, a geometria opiera się na OpenCascade. Stwórz system Raycastingu, który przeliczy współrzędne ekranowe na promień 3D i użyje StdSelect_BRepSelectionTool lub BRepIntCurveSurface z OCCT, aby zidentyfikować kliknięty element TopoDS_Shape."

Potem wróć do mnie z tym, co wypluło AI – sprawdzimy, czy nie zrobiło tam jakiegoś "haka", który utrudni Ci życie później!