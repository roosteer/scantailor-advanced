# Guía de pruebas (ScanTailor Advanced)

## Ejecutar tests unitarios

Con el proyecto configurado con `BUILD_TESTS=ON` (por defecto):

```bash
mkdir build && cd build
cmake .. -DBUILD_TESTS=ON
cmake --build .
ctest --output-on-failure
```

O ejecutar cada suite por separado:

```bash
./core_tests --log_level=message
./foundation_tests --log_level=message
./math_tests --log_level=message
./imageproc_tests --log_level=message
./qt_tests
./app_tests
```

## Cobertura por módulo

### core_tests
- **ContentSpanFinder**: detección de intervalos de contenido.
- **DeskewParams**: parámetros del filtro de enderezado.
- **ImageId** / **PageId**: identificación de imágenes y páginas, orden, subpáginas.
- **Margins**: márgenes y serialización XML.
- **PageRange**: rangos de páginas y selección alternada.
- **ProjectDocument**: etiquetas de salida ante nombres repetidos, ida y vuelta de un proyecto (páginas, selección, rotación, RTL), proyecto movido de raíz y guardado fallido.
- **SelectContentApply**: aplicación del filtro de contenido.
- **SmartFilenameOrdering**: ordenación natural de nombres de archivo.
- **Units**: conversión y representación de unidades (px, mm, cm, in).

### foundation_tests
- **Constants**: constantes matemáticas (PI, conversiones DPI/DPM, grados/radianes).
- **LineIntersectionScalar**: intersección de líneas (escalares).
- **Proximity**: distancia entre puntos y punto-segmento.
- **Utils**: conversión numérica a cadena (locale independiente).

### math_tests
- **Hessians** (adiff): diferenciación automática.
- **LineBoundedByRect**: recorte de recta por rectángulo.
- **LinearFunction** / **QuadraticFunction**: funciones lineales y cuadráticas.
- **MatrixCalc**: operaciones matriciales.
- **SidesOfLine**: lado de una recta (puntos).
- **SqDistApproximant**: aproximación de distancia al cuadrado.

### imageproc_tests
- **BinaryImage**, **Binarize**, **Morphology**, **RasterOp**, **Scale**, **Shear**, **Transform**, etc.
- **Dpi**: resolución (DPI) y serialización XML.

### qt_tests (Qt Test)
- **Tests de lógica (TestCoreQt)**: Units, foundation::Utils, SmartFilenameOrdering, QSignalSpy (señales y argumentos).
- **Tests de UX (TestUxQt)**:
  - **testButtonClickEmitsSignal**: clic en `QPushButton` emite `clicked()` (simulación con `QTest::mouseClick`).
  - **testCheckBoxToggleChangesState**: cambio de estado con `setChecked` y señal `toggled`.
  - **testLineEditAcceptsInput**: entrada de texto con `QTest::keyClicks` y comprobación del texto.
  - **testCollapsibleGroupBoxStateAndSignal**: widget del proyecto: estado colapsado/expandido y señal `collapsedStateChanged`.
  - **testErrorWidgetCreation**: creación de `ErrorWidget` y comprobación de hijos (sin mostrar ventana).

Los tests de UX usan `QApplication` y pueden simular interacción (clic, teclado) sin abrir ventanas; sirven de base para ampliar con más widgets o diálogos.

#### Tests de la CLI (`TestCliQt`)

Enlazan la librería estática `app_cli_lib`, así que ejercitan la lógica de la CLI sin pantalla:

- **parsesDefaults** / **parsesValueFlags**: valores por defecto y flags con valor (`--margins=1,2,3,4`).
- **rejectsUnknownFlag** / **rejectsMalformedValues** / **rejectsConflictingOptions**: flags desconocidos, valores mal formados y combinaciones imposibles, todos con código 2.
- **resolvesPositionals**: modo proyecto, modo imágenes y directorio de salida.
- **flagTableIsWellFormed**: cada entrada de la tabla de flags es única, tiene handler y descripción, y se encuentra con `findFlagSpec()`.
- **resolvesFilterRange**: validación de `--start-filter` / `--end-filter`.
- **rendersReport** / **mapsExitCode**: esquema del informe JSON y código de salida asociado.


### app_tests (Qt Test)

Enlazan `app_lib`, la librería estática donde viven los widgets del editor, así que ejercitan
los diálogos reales en lugar de una copia. Necesitan `QT_QPA_PLATFORM=offscreen` (ctest lo fija)
y llevan compilados los recursos de la aplicación, porque los widgets cargan iconos de ellos.

- **saveButtonFollowsTheProjectFile**: el botón "Save Project" solo se ofrece cuando hay una ruta de proyecto conocida; sin ella queda únicamente "Save Project As".
- **savesTheDocumentAfterTheEditorIsGone**: el diálogo de falta de memoria conserva el documento que se le entrega, guarda un proyecto reabrible (páginas y página seleccionada) después de que la ventana que lo creó haya desaparecido, y puede volver a guardarlo.

## Tests de UI/UX y caminos de uso

`app_tests` cubre los widgets que se pueden construir y usar sin pantalla. Para ampliar la
cobertura de UX y caminos de uso se puede:

1. **Qt Test**: Añadir casos a `app_tests` que:
   - Creen widgets/diálogos adicionales sin mostrar ventana.
   - Simulen eventos (clic, teclado) y comprueben estado.

2. **Tests de integración**: Scripts o tests que:
   - Abran proyectos de ejemplo.
   - Ejecuten filtros y comprueben resultados en disco o en memoria.

3. **Tests E2E**: Herramientas como Squish o scripts que controlen la aplicación gráfica (más costosos de mantener).

Para medir cobertura de código con gcov/lcov (solo compiladores que lo soporten):

```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON -DCMAKE_CXX_FLAGS="--coverage"
cmake --build .
ctest
lcov --capture --directory . --output-file coverage.info
lcov --remove coverage.info '/usr/*' --output-file coverage.info
genhtml coverage.info --output-directory coverage_html
```

## Desactivar tests

En entornos sin Boost o sin dependencias de test:

```bash
cmake .. -DBUILD_TESTS=OFF
```
