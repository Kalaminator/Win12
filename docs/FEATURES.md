# Features

Win12 is a Start menu for KDE Plasma 6 with nine menu styles, a matching taskbar, matching window title bars and colours, and one settings window for everything. This guide lists what each option does, page by page, in the order of the settings window: Style, Start menu, Apps list, Start button & shortcuts, Taskbar, Windows, Appearance, Search and Advanced. Open the settings with right-click on the Start button, then *Configure Win12*, or run `win12-settings`. Start11 images are not included.

## Menu styles

Pick a style on the *Style* page.

- **Windows 10**: live-tile groups, folders and pages, apps list on the left.
- **Windows 11**: grid of pinned apps, folders and a recommended list.
- **Windows 12**: a menu based on the Windows 12 concept art, apps list beside pinned cards and widgets.
- **Windows 7**: the classic two-column menu with a places list. Variants: with icons, compact, grid.
- **Modern**: an updated Windows 7 look. Same variants as Windows 7.
- **Windows XP**: the Luna menu, classic or modernised, in Blue, Olive Green, Silver or a custom colour.
- **App**: shortcuts on top, coloured groups and tabs, all apps always open.
- **Pro**: two tones, pinned apps and a places panel on the right.
- **Launcher**: big account picture, search and a large grid of pinned apps.
- **KDE Plasma**: your own Plasma launcher look, taskbar and title bars.

## Style page

- Style cards: one click picks the Start menu, the taskbar and the window title bars together.
- Mix and match: set *Start menu*, *Taskbar* and *Window title bars* each to the same style as the card or to another one (Windows 7, 10, 11, 12, XP or KDE Plasma). With KDE Plasma, your own Plasma Style and title bars come back.
- Windows 7 group (Windows 7 and Modern): menu look Classic or Modern, and the variant of the Windows 7 list or the Modern list (with icons, compact, grid).
- Windows XP group: XP theme (Classic XP, Modern classic or Modern) and colour scheme (Blue, Olive Green, Silver, Custom).
- Use an XP icon theme while XP is active, and pick the theme. Get one on the Advanced page.
- A group only shows when a style that uses it is in play.

## Start menu page

- Size: Small, Tall, Wide (two tile columns) or Large (two tile columns), plus a custom height.
- Position: Left, Center or Right. Center puts the menu under the middle of the screen, like Windows 11. Right puts the menu at the right edge and mirrors the whole layout.
- Optionally also align the Start button and the taskbar icons with the menu (Windows 11 look). Turning it off puts the panel back as it was.
- Full screen menu, and keep the panel visible while it is open.
- Menu scale, from smaller to larger than the original size.
- Tiles: *Edit menu layout* opens design mode to drag the section dividers (see Design mode). Tile columns, space between tiles, tile icon size, hide the caption (shown as a tooltip), and lock the position of tiles and pins.
- Tile look: tile colour (soft, accent colour or custom), tile background strength, a tile image and its opacity.
- Fetch the favicon for pinned web links.
- Reset tile and pin positions: tiles, pinned icons and pin names go back to the fresh-install layout.
- Windows 12 group (Windows 12 style): apps list on the left or right, greeting with picture, Documents, Pictures and Music above the cards, clock and date above the cards.

## Apps list page

- Hide the list (tiles only), icon size, row height.
- Show recently added apps, most used apps, and highlight new applications.
- Show app categories as folders, and put folders at the bottom.
- Hide folders in the recent list (Windows 11 and 12 styles).
- All Programs group (Windows 7, Modern and XP): open folders when the mouse is over them, show the Desktop folder in All Programs, show categories as a cascading menu.
- Hidden apps: see the list and show an app again, or clear the list. (Hide an app by right-clicking it in the list.)
- Filtered folders: add a folder as its own group with a name, a folder to read, and optional include and exclude patterns (for example, only PDFs).

## Start button & shortcuts page

- Look of the button: an icon, a picture (your own PNG) or four squares.
- Icon: pick an icon, reset it, and set the icon size.
- Picture: use a custom image with three pictures stacked top to bottom: normal, mouse over, pressed.
- Four squares: colours for the first square and the other squares.
- Windows XP style: Start button colour (XP green or the colour scheme).
- Right-click menu: choose which items appear in it (see below).
- Menu shortcuts: choose which shortcuts appear on the side list, and their order: user picture, Documents, Downloads, Music, Pictures, Videos, Personal folder, Network, Computer, Trash, Recent files, Games, Printers, Users, Updates, a separator, File Explorer, Personalization, Settings, Terminal, System Monitor, Run, Win12 Settings and Power.
- Add your own location: a folder, a web address or a command, with a name and an icon.
- Icons only, names on mouse over, or always icons and names; icon size.
- Align icons to the bottom (Windows 10), smaller place rows (Windows 7, XP, Pro).
- Power: power button shows the power menu or runs one action directly, and pick what the power menu contains (Lock, Sign out, Switch user, Sleep, Hibernate, Restart, Shut down). Icon on the Shut down button (Windows 7).

## Taskbar page

- With your own Plasma Style, pick a Windows taskbar on the Style page first. Then these rows show.
- Windows 12 taskbar group:
  - Layout: One floating bar, two Split layouts (widgets left, apps centre, tray right; or Start and apps left, tray right), or 3 Islands (pinned apps, open apps and tray). In 3 Islands, drag an app icon to another island to move it. Magnetic islands: close together in the middle, or pushed to the screen edges.
  - 3 Islands and dragging apps between islands need the patched KDE packages (see the README); on stock KDE the split layouts and the Start position still work, slowly (5-10 s per change), and the page says so.
  - Show islands on: one monitor (default: the main one) or All monitors. With All monitors, every monitor gets Start and the app islands, and the tray island stays on the main monitor.
  - Open apps on other monitors (with All monitors): only the apps on that monitor (default), or all apps on every monitor.
  - Floating bar and bottom spacing. Maximised windows: fill the screen (the bar auto-hides as an island over them) or keep a gap above the taskbar. Menu gap, pop-up animation (Slide, Fade, Zoom, Rise), corner roundness.
- Taskbar colour and texture group:
  - Use a custom taskbar colour instead of your Plasma Style: custom colour and opacity, corner radius, a light line on the taskbar edge.
  - Task buttons: Plasma Style, Windows 10 (bar under the active one), Windows 11 (pill under the active one) or Windows 7 (glass). Gradient. Paint the active button less transparent.
  - Texture: your own image, tile or stretch, alignment, opacity, tint with the accent colour.
- Behaviour group: auto-hide, and a search box on the taskbar with its width.
- Plasma panel group: *Edit panel*. Panel height, position and task grouping are Plasma settings.

## Windows page

Needs a Windows title bar picked on the Style page.

- Opacity, blur behind translucent frames, corner radius and title bar height.
- Title bar picture: your own image, placement, opacity, tint with the accent colour.
- Windows XP group: use the system title bar in Firefox (restart Firefox).

## Appearance page

- Colours group: mode Light, Dark, Automatic (Plasma colours) or Custom colours (menu colour and text colour).
- Accent colour: automatic (Plasma accent) or manual, picked from the 48 Windows accent colours, your recent colours or your own.
- Show the accent colour on Start and the taskbar, and on title bars and window borders.
- The Windows XP style uses its own colour scheme from the Style page.
- Windows 12 shade group: light mode tint and dark mode shade.
- Transparency and corners group: transparency effects; make the Start menu solid, or choose how transparent its background is; Windows 12 transparency, blur and grain; readable glass (two-tone edge and shadow; an even background behind the glass with the kwin-win12 patch); round the corners of the menu, from square to very round.
- Texture: use an image as the menu background (your own file), how it is laid out (tile, stretch, fit, fill), fade it, darken it, or recolour it with the menu colour.
- Fonts: menu font and size, header font and size, item font size.
- Effects: fade the menu in when it opens, and show app icons in grey.

## Search page

- Results: group results by type, classic search (dense one-line results without the Best match card), filter icons above the results, the "search the internet" button and the search engine.
- History: remember recent searches, and clear the history.
- What to search: apps, settings, files (file search index), recent documents, places, calculator, unit converter, commands, web shortcuts (such as gg: and wp:), browser bookmarks, open windows, sessions and power, special characters, dictionary. Or use everything KRunner can search.

## Advanced page

- Icons and wallpaper: pick the system icon theme, download icon themes and wallpapers, and set your own image as the wallpaper. These apply at once, not with OK.
- Backup settings to a file, restore them from a file, or reset all settings. Tiles and pinned icons are included in the backup.

## Right-click on the Start button

A small menu with quick actions. You choose the entries on the Start button & shortcuts page: Settings, File Explorer, System Monitor, Terminal, Run, Show desktop, Edit panel, Auto-hide panel, and Lock, Sign out, Sleep, Restart and Shut down. It also has the Edit menu layout entry (opens design mode) and Edit Applications.

## Design mode

Open it with *Edit menu layout* on the Start menu page or from the right-click menu. The menu stays open and a bar appears above it. Drag the dividers between menu sections to resize them. *Reset* puts the default sizes back, *Done* ends design mode. Closing the menu also ends it.

## Search

Type as soon as the menu opens. Results come from the sources chosen on the Search page, with filter icons (apps, settings, documents, music, pictures, videos, files, web) and a search history. You can also pin apps, websites, files and folders, rename pins, hide apps, and use the keyboard to move around.

## Taskbar and windows follow the menu style

On the Style page, *Taskbar* and *Window title bars* can follow the menu style. Then the taskbar (a Plasma Style), the window title bars (Aurorae) and the colour scheme follow the chosen menu style (Windows 10, 11, 7 or XP) and the accent colour. The XP styles need the matching XP taskbar theme. Everything is undone when you pick KDE Plasma: Win12 saves your own Plasma Style, window decoration and colour scheme first and puts them back.

## Languages

English, Spanish, French, German, Italian, Brazilian Portuguese, Russian, Arabic, Simplified Chinese, Japanese and Hindi. Win12 follows your Plasma language.

## Install and update

- Works on any Plasma 6 distro (Arch, Fedora KDE, openSUSE, Kubuntu, Debian 13, KDE neon, Manjaro and others). It needs `python3` and the normal KDE tools.
- From the KDE Store: *Add or Manage Widgets*, *Get New Widgets*, search Win12. On first start the widget creates its taskbar styles, title bars, colour schemes and the `win12-settings` command by itself. Updates come through Discover.
- From git: clone, then run `./install.sh`. Update with `git pull && ./install.sh --restart`.

---

# Funciones (español)

Win12 es un menú Inicio para KDE Plasma 6 con nueve estilos de menú, una barra de tareas a juego, barras de título y colores de ventana a juego, y una sola ventana de configuración. Esta guía explica qué hace cada opción, página por página, en el orden de la ventana de configuración: Estilo, Menú Inicio, Lista de aplicaciones, Botón Inicio y accesos directos, Barra de tareas, Ventanas, Apariencia, Búsqueda y Avanzado. Abre la configuración con clic derecho en el botón de inicio y *Configurar Win12*, o ejecuta `win12-settings`. Las imágenes de Start11 no se incluyen.

## Estilos de menú

Elige el estilo en la página *Estilo*.

- **Windows 10**: grupos de mosaicos, carpetas y páginas, lista de aplicaciones a la izquierda.
- **Windows 11**: cuadrícula de apps ancladas, carpetas y lista de recomendados.
- **Windows 12**: un menú basado en el arte conceptual de Windows 12, con la lista de aplicaciones junto a tarjetas ancladas y widgets.
- **Windows 7**: el menú clásico de dos columnas con lista de lugares. Variantes: con iconos, compacto, cuadrícula.
- **Modern**: un aspecto Windows 7 actualizado, con las mismas variantes.
- **Windows XP**: el menú Luna, clásico o modernizado, en Azul, Verde oliva, Plata o un color propio.
- **App**: accesos arriba, grupos de colores y pestañas, todas las apps siempre abiertas.
- **Pro**: dos tonos, apps ancladas y un panel de lugares a la derecha.
- **Launcher**: foto de cuenta grande, búsqueda y una cuadrícula grande de apps ancladas.
- **KDE Plasma**: el aspecto de lanzador, la barra de tareas y las barras de título propios de Plasma.

## Página Estilo

- Tarjetas de estilo: un clic elige a la vez el menú Inicio, la barra de tareas y las barras de título de las ventanas.
- Combinar estilos: pon *Menú Inicio*, *Barra de tareas* y *Barras de título de las ventanas* cada uno igual que la tarjeta o en otro estilo (Windows 7, 10, 11, 12, XP o KDE Plasma). Con KDE Plasma vuelven tu estilo de Plasma y tus barras de título.
- Grupo Windows 7 (Windows 7 y Modern): aspecto del menú Clásico o Moderno, y la variante de la lista de Windows 7 o de la lista moderna (con iconos, compacta, cuadrícula).
- Grupo Windows XP: tema de XP (XP clásico, Clásico moderno o Moderno) y combinación de colores (Azul, Verde oliva, Plata, Personalizado).
- Usar un tema de iconos de XP mientras XP está activo, y elegir el tema. Consigue uno en la página Avanzado.
- Un grupo solo aparece cuando se usa un estilo que lo necesita.

## Página Menú Inicio

- Tamaño: Pequeño, Alto, Ancho (dos columnas de mosaicos) o Grande (dos columnas de mosaicos), y una altura personalizada.
- Posición: Izquierda, Centro o Derecha. Centro pone el menú bajo el medio de la pantalla, como en Windows 11. Derecha pone el menú en el borde derecho y refleja todo el diseño.
- Opcionalmente alinea también el botón de inicio y los iconos de la barra de tareas con el menú (aspecto Windows 11). Al apagarlo, el panel vuelve a como estaba.
- Menú a pantalla completa, y mantener el panel visible mientras está abierto.
- Escala del menú, más pequeña o más grande que el tamaño original.
- Mosaicos: *Editar diseño del menú* abre el modo diseño para arrastrar los divisores de sección (ver Modo diseño). Columnas de mosaicos, espacio entre mosaicos, tamaño del icono, ocultar el título (se ve como ayuda emergente) y bloquear la posición de mosaicos y anclajes.
- Aspecto de los mosaicos: color del mosaico (suave, color de acento o propio), intensidad del fondo, una imagen de mosaico y su opacidad.
- Obtener el favicon de los enlaces web anclados.
- Restablecer la posición de mosaicos y anclajes: mosaicos, iconos anclados y nombres vuelven al diseño de una instalación nueva.
- Grupo Windows 12 (estilo Windows 12): lista de aplicaciones a la izquierda o a la derecha, saludo con imagen, Documentos, Imágenes y Música sobre las tarjetas, reloj y fecha sobre las tarjetas.

## Página Lista de aplicaciones

- Ocultar la lista (solo mosaicos), tamaño de iconos, alto de fila.
- Mostrar apps añadidas recientemente, las más usadas, y resaltar las aplicaciones nuevas.
- Mostrar las categorías como carpetas, y poner carpetas al final.
- Ocultar carpetas en la lista reciente (estilos Windows 11 y 12).
- Grupo Todos los programas (Windows 7, Modern y XP): abrir carpetas al pasar el ratón, mostrar la carpeta Escritorio en Todos los programas, mostrar categorías como menú en cascada.
- Apps ocultas: ver la lista y volver a mostrar una app, o vaciar la lista. (Para ocultar una app, clic derecho sobre ella en la lista.)
- Carpetas filtradas: añade una carpeta como grupo propio con un nombre, una carpeta que leer y patrones opcionales de inclusión y exclusión (por ejemplo, solo PDF).

## Página Botón Inicio y accesos directos

- Aspecto del botón: un icono, una imagen (tu propio PNG) o cuatro cuadrados.
- Icono: elegir un icono, restablecerlo y ajustar su tamaño.
- Imagen: usar una imagen propia con tres dibujos apilados de arriba abajo: normal, ratón encima, pulsado.
- Cuatro cuadrados: colores del primer cuadrado y de los otros cuadrados.
- Estilo Windows XP: color del botón de inicio (verde XP o el de la combinación de colores).
- Menú de clic derecho: elegir qué entradas salen en él (ver abajo).
- Accesos directos del menú: elige qué accesos aparecen en la lista lateral y su orden: foto de usuario, Documentos, Descargas, Música, Imágenes, Vídeos, Carpeta personal, Red, Equipo, Papelera, Archivos recientes, Juegos, Impresoras, Usuarios, Actualizaciones, un separador, Explorador de archivos, Personalización, Configuración, Terminal, Monitor del sistema, Ejecutar, Configuración de Win12 y Energía.
- Añade tu propio lugar: una carpeta, una dirección web o un comando, con nombre e icono.
- Solo iconos, nombres al pasar el ratón, o siempre iconos y nombres; tamaño de icono.
- Alinear iconos abajo (Windows 10), filas de lugares más pequeñas (Windows 7, XP, Pro).
- Energía: el botón de energía muestra el menú de energía o ejecuta una acción directa, y elige qué contiene el menú (Bloquear, Cerrar sesión, Cambiar de usuario, Suspender, Hibernar, Reiniciar, Apagar). Icono en el botón Apagar (Windows 7).

## Página Barra de tareas

- Con tu propio estilo de Plasma, elige antes una barra de tareas de Windows en la página Estilo. Entonces aparecen estas filas.
- Grupo Barra de tareas de Windows 12:
  - Distribución: Una barra flotante, dos distribuciones divididas (widgets a la izquierda, apps al centro, bandeja a la derecha; o Inicio y apps a la izquierda, bandeja a la derecha), o 3 islas (apps ancladas, apps abiertas y bandeja). En 3 islas, arrastra el icono de una app a otra isla para moverlo. Islas magnéticas: juntas en el medio, o pegadas a los bordes de la pantalla.
  - Las distribuciones divididas y de 3 islas necesitan los paquetes de KDE parcheados (ver el README). La página muestra un aviso si faltan.
  - Mostrar las islas en: un monitor (por defecto, el principal) o Todos los monitores. Con Todos los monitores, cada monitor tiene Inicio y las islas de apps, y la isla de la bandeja se queda en el monitor principal.
  - Apps abiertas en los otros monitores (con Todos los monitores): solo las apps de ese monitor (por defecto), o todas las apps en todos los monitores.
  - Barra flotante y espacio inferior. Ventanas maximizadas: ocupar la pantalla (la barra se oculta sola como una isla sobre ellas) o dejar un hueco sobre la barra de tareas. Espacio del menú, animación al aparecer (Deslizar, Fundido, Zoom, Subir), redondez de las esquinas.
- Grupo Color y textura de la barra de tareas:
  - Usar un color propio para la barra de tareas en lugar de tu estilo de Plasma: color y opacidad propios, radio de las esquinas, una línea de luz en el borde.
  - Botones de tareas: Estilo de Plasma, Windows 10 (barra bajo el activo), Windows 11 (píldora bajo el activo) o Windows 7 (cristal). Degradado. Pintar el botón activo menos transparente.
  - Textura: tu propia imagen, mosaico o estirada, alineación, opacidad, teñir con el color de acento.
- Grupo Comportamiento: ocultar automáticamente, y un cuadro de búsqueda en la barra con su ancho.
- Grupo Panel de Plasma: *Editar panel*. Altura, posición y agrupar tareas son ajustes de Plasma.

## Página Ventanas

Necesita una barra de título de Windows elegida en la página Estilo.

- Opacidad, desenfoque tras los marcos translúcidos, radio de las esquinas y altura de la barra de título.
- Imagen de la barra de título: tu propia imagen, colocación, opacidad, teñir con el color de acento.
- Grupo Windows XP: usar la barra de título del sistema en Firefox (reinicia Firefox).

## Página Apariencia

- Grupo Colores: modo Claro, Oscuro, Automático (colores de Plasma) o Colores personalizados (color del menú y del texto).
- Color de acento: automático (acento de Plasma) o manual, elegido entre los 48 colores de acento de Windows, tus colores recientes o los tuyos.
- Mostrar el color de acento en Inicio y la barra de tareas, y en barras de título y bordes de ventana.
- El estilo Windows XP usa su propia combinación de colores de la página Estilo.
- Grupo Sombra de Windows 12: tono del modo claro y sombra del modo oscuro.
- Grupo Transparencia y esquinas: efectos de transparencia; menú Inicio sólido, o elegir lo transparente que es su fondo; transparencia, desenfoque y grano de Windows 12; redondear las esquinas del menú, de cuadrado a muy redondo.
- Textura: usar una imagen como fondo del menú (tu propio archivo), cómo se coloca (mosaico, estirar, ajustar, rellenar), atenuarla, oscurecerla o teñirla con el color del menú.
- Fuentes: fuente y tamaño del menú, fuente y tamaño de los encabezados, tamaño de los elementos.
- Efectos: aparecer el menú con un fundido al abrirlo, y mostrar los iconos de las apps en gris.

## Página Búsqueda

- Resultados: agrupar resultados por tipo, búsqueda clásica (resultados densos de una línea, sin la tarjeta de mejor resultado), iconos de filtro sobre los resultados, el botón de buscar en internet y el buscador.
- Historial: recordar búsquedas recientes y borrar el historial.
- Qué buscar: apps, ajustes, archivos (índice de búsqueda de archivos), documentos recientes, lugares, calculadora, conversor de unidades, comandos, atajos web (como gg: y wp:), marcadores del navegador, ventanas abiertas, sesiones y energía, caracteres especiales, diccionario. O usa todo lo que KRunner pueda buscar.

## Página Avanzado

- Iconos y fondo de pantalla: elegir el tema de iconos del sistema, descargar temas de iconos y fondos, y poner tu propia imagen como fondo de pantalla. Se aplican al instante, sin pulsar Aceptar.
- Copia de seguridad de los ajustes a un archivo, restaurarlos desde un archivo, o restablecer todo. La copia incluye mosaicos e iconos anclados.

## Clic derecho en el botón de inicio

Un menú pequeño con acciones rápidas. Eliges las entradas en la página Botón Inicio y accesos directos: Configuración, Explorador de archivos, Monitor del sistema, Terminal, Ejecutar, Mostrar escritorio, Editar panel, Ocultar panel automáticamente, y Bloquear, Cerrar sesión, Suspender, Reiniciar y Apagar. También tiene la entrada Editar diseño del menú (abre el modo diseño) y Editar aplicaciones.

## Modo diseño

Se abre con *Editar diseño del menú* en la página Menú Inicio o desde el menú de clic derecho. El menú sigue abierto y aparece una barra encima. Arrastra los divisores entre secciones para cambiar su tamaño. *Restablecer* devuelve los tamaños por defecto y *Hecho* termina el modo diseño. Cerrar el menú también lo termina.

## Búsqueda

Escribe en cuanto se abre el menú. Los resultados vienen de las fuentes elegidas en la página Búsqueda, con iconos de filtro (apps, ajustes, documentos, música, imágenes, vídeos, archivos, web) e historial de búsquedas. También puedes anclar apps, sitios web, archivos y carpetas, renombrar anclajes, ocultar apps y moverte con el teclado.

## La barra de tareas y las ventanas siguen el estilo del menú

En la página Estilo, *Barra de tareas* y *Barras de título de las ventanas* pueden seguir el estilo del menú. Entonces la barra de tareas (un estilo de Plasma), las barras de título (Aurorae) y el esquema de colores siguen el estilo de menú elegido (Windows 10, 11, 7 o XP) y el color de acento. Los estilos XP necesitan el tema de barra XP correspondiente. Todo se deshace al elegir KDE Plasma: Win12 guarda antes tu estilo de Plasma, tus bordes de ventana y tu esquema de colores, y los devuelve.

## Idiomas

Inglés, español, francés, alemán, italiano, portugués de Brasil, ruso, árabe, chino simplificado, japonés y hindi. Win12 sigue el idioma de tu Plasma.

## Instalar y actualizar

- Funciona en cualquier distro con Plasma 6 (Arch, Fedora KDE, openSUSE, Kubuntu, Debian 13, KDE neon, Manjaro y otras). Necesita `python3` y las herramientas normales de KDE.
- Desde la KDE Store: *Añadir o gestionar widgets*, *Obtener nuevos widgets*, busca Win12. Al primer arranque el widget crea por sí solo sus estilos de barra, bordes de ventana, esquemas de color y el comando `win12-settings`. Las actualizaciones llegan por Discover.
- Desde git: clona y ejecuta `./install.sh`. Actualiza con `git pull && ./install.sh --restart`.
