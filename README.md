# DOM Engine

A lightweight C++ custom UI framework with a built-in JS-like scripting engine.

## Quick Start

You can use the unified interactive Builder Tool to compile, bundle, and clean your workspace seamlessly!

```bat
.\build.bat
```

Alternatively, run the legacy CLI scripts manually:
```bat
compile.bat         :: Development build (with console)
compile.bat min     :: Ultra-minimized release (~100KB)
compile.bat debug   :: Debug build with symbols

dom.exe           :: Runs app.dom by default
dom.exe my.dom    :: Run a specific .dom file
```

## Application Bundling (AOT & Payload Compression)

The engine can natively bundle your `.dom` documents directly into a `.exe`:

```bat
bundle.bat examples\main\app.dom -full
```
This embeds `dom.exe` and your javascript natively inside a standalone executable!

**Advanced Architectures:**
- **Ahead-of-Time (AOT)**: Generates raw C++ hardware-level execution logic for UI instantiation. Drastically drops footprint and reduces boot times to milliseconds.
- **LZNT1 / RC4 Compression**: Compresses or Secures DOM hierarchies for embedded deployments.

Use the `[2]` route inside `build.bat` to step through the graphical compilation steps to configure this safely.

## .dom File Format

A `.dom` file combines HTML-like markup, CSS-like styling, and JS-like scripting:

```xml
<import src="components/Button.dom" as="Button" />

<style>
    #app {
        --theme-bg: #1E1E2E;
        bg: var(--theme-bg);
        width: 800;
        height: 600;
    }
    Button:hover { bg: #B4BEFE; }
</style>

<ui frameless="true" id="app" onLoad="init();">
    <Text text="Hello World" x="20" y="20" color="#CDD6F4" />
    <Button id="btn" x="20" y="60" onClick="doSomething();" />
</ui>

<script>
function init() {
    sys.window.center();
    sys.log("App loaded!");
}
function doSomething() {
    var el = document.querySelector("#btn");
    el.setAttribute("text", "Clicked!");
}
</script>
```

## Architecture

```
main.cpp
├── src/Core/          Custom JS-like scripting engine
│   ├── Lexer.h        Tokenizer
│   ├── Parser.h       Recursive descent parser → AST
│   ├── AST.h          Abstract syntax tree nodes
│   ├── Interpreter.h  Tree-walking interpreter + builtins
│   ├── Value.h        Runtime value types
│   └── Environment.h  Lexical scoping + closures
├── src/DOM/           UI document object model
│   ├── Element.h      UI element with shadow DOM
│   ├── Selector.h     CSS selector engine
│   └── Event.h        Event object
├── src/Parser/
│   ├── DOMParser.h    .dom file parser
│   └── AOTCompiler.h  Native C++ Transpilation compiler
├── src/Platform/
│   ├── Hooks.h        Dynamic IAT hooking
│   ├── Window.h       sys.window / sys.screen APIs
│   ├── DLLBridge.h    sys.dllCall
│   └── Http.h         fetch() via WinHTTP
└── src/ScriptBridge.h Connects interpreter ↔ DOM
```

## Scripting API

### DOM Element Methods
Native DOM manipulation mimics standard browser conventions but natively strips away asynchronous race conditions.
*   `document.querySelector(selector: string) -> Element?`
    Searches the light DOM globally and returns the first element matching the CSS selector. Returns `null` if no match is found.
*   `document.querySelectorAll(selector: string) -> Array<Element>`
    Searches the light DOM globally and returns an array of all matching elements.
*   `document.shadowDomSelector(selector: string) -> Element?`
    Bypasses standard encapsulation boundaries to query elements inside component shadow trees.
*   `document.shadowDomSelectorAll(selector: string) -> Array<Element>`
    Returns all matching shadow elements internally isolated inside components.
*   `document.getElementById(id: string) -> Element?`
    Fast ID lookup globally mapping to the document.
*   `document.createElement(tag: string) -> Element`
    Programmatically creates a detached node memory object. Use `.appendChild` to inject it into the active tree.
*   `el.querySelector`, `el.querySelectorAll`, `el.shadowDomSelector`
    Performs exactly the same query operations, but restricts the scope entirely to the children of `el`.
*   `el.parentElement -> Element?`
    Returns the immediate parent wrapper of this element. Returns `null` if at the root.
*   `el.children -> Array<Element>`
    Returns an array of all direct light child node descriptors.
*   `el.appendChild(child: Element)`
    Injects a detached or existing node onto the end of the element's direct children. Native DOM immediately signals a GUI repaint.
*   `el.getAttribute(key: string) -> string`
    Returns the raw string property value for `key` stored in the element's state memory.
*   `el.setAttribute(key: string, value: string)`
    Sets or updates an element's property. If the property affects layout or visuals, the engine immediately flags the GUI for redrawing.
*   `el.classList.add(cls: string)`, `.remove(cls: string)`, `.toggle(cls: string)`, `.contains(cls: string) -> bool`
    Efficient management for the element's CSS class mappings. All mutating calls automatically queue a repaint.
*   `el.textContent` / `el.innerText` (property strings)
    Gets or overrides the inner text value natively rendered inside the node.
*   `el.getBoundingRect() -> {x, y, width, height}`
    Triggers an immediate layout calculation if dirty and returns the absolute screen coordinate bounds of the element.
*   `el.show()`, `el.hide()`
    Toggles the internal visibility bitmask of the element. Invisible elements still consume layout space unless styled as `display: none`.
*   `el.focus()`, `el.blur()`
    Grants or removes UI interaction focus. Focus applies `:focused` CSS state globally.

### HTML Properties & Inline States
All CSS properties can be set inline via direct HTML attributes (e.g. `<Button border-radius="12" />`).
You can selectively map inline pseudo-state styling without writing a CSS class by prefixing the property with the target state identifier. 
**Example**: `<Button hover:bg="#ff0000" focused:border="#00ff00" active:opacity="0.5" />`

### System (sys.*)
The `sys` object binds raw OS APIs directly into the javascript environment.
*   `sys.log(msg: string)`
    Prints output to the console (`stdout`), pushes it to the Windows Visual Studio `OutputDebugString` pipe, and appends it iteratively into `sys_log.txt` locally.
*   `sys.setLogEnabled(bool: enable)`
    A global suppression toggle. `sys.setLogEnabled(false)` completely halts all system logging pipelines for pristine console control.
*   `sys.time() -> number`
    Microsecond-bound system uptime resolution. Invokes `GetTickCount64()`.
*   `sys.exec(cmd: string, hidden: bool=false) -> number`
    Forks a new native Windows sub-process using standard `CreateProcess`. The boolean parameter controls if the window operates in `SW_HIDE` mode. Returns the active Process ID.
*   `sys.readText(filepath: string) -> string`
    Performs an immediate blocking read of the given generic file path. Output is parsed entirely as UTF-8.
*   `sys.writeText(filepath: string, data: string)`
    Performs an immediate blocking overwrite stream out to the given file path.
*   `sys.loadScript(filepath: string, asNewProcess: bool=false)`
    Hot-loads and parses an external `.dom` context map directly into memory. If `asNewProcess` is true, the file spins up completely isolated inside a cloned `dom.exe` child fork.
*   `sys.dllCall(dll: string, func: string, args: Array) -> auto`
    Internal bridge for unsafe Foreign Function Interfaces. Invokes arbitrary dynamically resolved Windows `.dll` logic mapping basic JS strings/numbers to C ABI targets.
*   `sys.sendMessage(hwnd: number, msg: number, wParam: number, lParam: number)`
    The lowest-level OS IPC primitive. Dispatches raw Windows UI messages securely to internal or external process queues.
*   `sys.findWindow(title: string) -> number`
    Scans the visible Windows desktop to locate exact titlebar strings and extract their internal OS handle (`HWND`).
*   `sys.getHwnd() -> number`
    Returns the Native DOM application's core memory handle mapping (`HWND`), letting you dispatch native procedures targeting your application's frame buffer!

### Window (sys.window.*)
*   `sys.window.resize(w: number, h: number)` / `sys.window.move(x: number, y: number)` / `sys.window.center()`
    Immediately repaints and adjusts the coordinate box of the master window frame.
*   `sys.window.hide()` / `sys.window.show()`
    Alters `ShowWindow` frame toggles natively.
*   `sys.window.minimize()` / `sys.window.maximize()` / `sys.window.restore()` / `sys.window.close()`
    Native macro-level controls for state orchestration.
*   `sys.window.setFullscreen(bool: enable)`
    Seamlessly drops Windows Chrome metrics (titlebars and borders) and stretches the underlying DWM framebuffer strictly matching the native multi-monitor workarea constraint boundaries.
*   `sys.window.isMaximized() -> bool`
    Parses Windows' internal visual zoom states (`IsZoomed()`).
*   `sys.window.setTitle(title: string)`
    Alters internal Task View logic and generic frame decorations.
*   `sys.window.setOpacity(alpha: number)`
    Translates standard `0.0` - `1.0` floats into layered OS blend attributes natively updating the window structure mask.
*   `sys.window.setAlwaysOnTop(bool: ensureTop)`
    Injects internal memory hooks to force the UI bounds over standard DWM occlusion boundaries.
*   `sys.window.setIcon(filepath: string)`
    Overwrites the visual GUI container icon matching standard `.ico` paths.
*   `sys.window.getSize() -> {width, height}` / `sys.window.getPosition() -> {x, y}`
    Returns real-time hardware measurements pulled directly out of the `GetClientRect` and `GetWindowRect` endpoints.
*   `sys.window.getActiveProcessName() -> string`
    Bypasses generic limits to inject into the `GetForegroundWindow` memory space, extracting the exact `.exe` name currently managing input!
*   `sys.window.registerHotkey(id: number, keyStr: string)`
    Sets up a global DWM key hook sequence. This captures keyboard vectors even while NativeDOM is minimized!
    ```javascript
    // Example: Listening for a global hotkey
    sys.window.registerHotkey(1, "CTRL+ALT+T");
    document.addEventListener("hotkey", (e) => {
        if (e.id === 1) { sys.log("Hotkey pressed!"); }
    });
    ```
*   `sys.window.unregisterHotkey(id: number)`
    Releases the global DWM lock over the bound keys.

### Screen & Inputs (sys.screen.*, sys.keyboard.*, sys.clipboard.*)
*   `sys.screen.getInfo()`
    Returns the core layout details encapsulating raw `width` / `height` pixel resolutions, the hardware coordinate `monitors` count, and a `workArea {x, y, width, height}` subset boundary identifying visible space devoid of taskbars.
*   `sys.screen.getDPI() -> number`
    Extracts the display scaling percentage configured globally inside Windows Settings.
*   `sys.screen.getMousePosition() -> {x, y}`
    Provides raw active coordinates tracking exactly where the pointer rests unconditionally.
*   `sys.screen.getMonitorAt(x: number, y: number) -> {name, x, y, width, height, isPrimary}`
    Cross-references raw multi-head parameters identifying explicit dimensions for targeted coordinate arrays.
*   `sys.keyboard.hook(callback: Function)` / `sys.keyboard.unhook()`
    Establishes an extreme `WH_KEYBOARD_LL` low-level intercept over the complete OS loop. Returning `true` out of the callback forces the engine to burn the keystroke directly out of Windows buffers, effectively black-holing the input!
    ```javascript
    sys.keyboard.hook((e) => {
        sys.log(`Globally Intercepted keyCode: \${e.vkCode}`);
        return true; // Completely stops Windows from detecting this key
    });
    ```
*   `sys.clipboard.getText() -> string`
    Decodes the raw text currently stored implicitly globally inside the shared Windows clipboard queue.
*   `sys.clipboard.setText(string: payload)`
    Overwrites the OS buffer injecting exact memory configurations available to external processes.
*   `sys.clipboard.clear()`
    Wipes the clipboard buffer totally clean preventing accidental multi-read memory leaks.
*   `sys.clipboard.getFormat(id) -> Array<number>`, `sys.clipboard.setFormat(id, buffer)`
    Binary API wrapper. Permits direct format abstraction (mapping numerical integers like `CF_BITMAP`) across arbitrary bytes!

### Async Boundaries
*   `setTimeout(fn: Function, ms: number)`, `setInterval(fn: Function, ms: number)`
    Asynchronous event handlers queue execution exactly inside NativeDOM's generic message loop.
*   `clearTimeout(id: number)`, `clearInterval(id: number)`
    Silently unwraps pending execution arrays halting future calls natively.
*   `new Promise((resolve, reject) => ...)`, `.then()`, `.catch()`
    Standard ECMAScript implementation. NativeDOM resolves promise memory cleanly without blocking layout updates!
*   `fetch(url: string, { method, body, headers })`
    Fully async standard XHR networking bridge utilizing internal multi-threading over Windows HTTP layers.
    Returns a `Promise` resolving to:
    ```javascript
    {
        ok: boolean,
        status: number,
        body: string, // raw string output
        text: () => string, // callable payload text mapping 
        json: () => object // callable JSON object parser boundary
    }
    ```

### Event System
The framework binds robustly to both generic OS hooks and specific visual interactions!

*   **Document Level Listeners**: `document.addEventListener(type: string, cb)`, `document.removeEventListener(type)`
    Captures bubbling triggers traversing directly to the core document root.
*   **Element Listeners**: `el.addEventListener(type: string, cb)`, `el.removeEventListener(type)`
    Maps specifically onto encapsulated coordinate hits or specialized logic.
*   **Dispatchers**: `document.dispatchEvent(event)`, `el.dispatchEvent(event)`
    Manually queues execution trees firing all listening logic.
*   **Event Controls**: `e.preventDefault()`, `e.stopPropagation()`, `e.stopImmediatePropagation()`
    Halts bubbling/capturing propagation completely inline simulating exact DOM Level 3 definitions.
*   **Global Event: Native** `hotkey`
    Provided via `sys.window.registerHotkey`. Triggers properties: `e.id`, `e.modifiers`, `e.vkCode`.
*   **Global Event: Input** `keydown`, `keypress`, `keyup`
    Triggers properties: `e.keyCode`, `e.key`.
*   **Global Event: Mouse** `click`, `dblclick`, `mousedown`, `mouseup`, `mousemove`, `mouseenter`, `mouseleave`, `contextmenu`, `wheel`
    Injected with positional boundaries `e.clientX`, `e.clientY`, and the bounding object `e.target`.
*   **Global Event: Control** `focus`, `blur`
    Maps activation routines across specific tree nodes.
*   **Global Event: Status** `load`, `resize`
    Standard workflow hooks allowing robust bootstrapping arrays.
*   **Global Event: OS Integrations** `drop`
    Injected upon OS Drag-and-Drop operations tracking target filepaths inside `e.file`.
*   **Custom Dispatch Hooks**: `new CustomEvent("name", { detail: object })`
    Synthesizes brand new pseudo-logic wrappers for custom component APIs.

### CSS Features & Selectors
*NativeDOM parses selectors top-to-bottom natively.*
- Elements: `Text`, `Button`
- Identifiers: `#my-id`
- Classes: `.my-class`
- Attributes: `[attr]`, `[attr="val"]`, `[attr*="val"]`, `[attr^="val"]`, `[attr$="val"]`
- Pseudo-States: `:hover`, `:focused`, `:active`, `:first-child`, `:last-child`, `:nth-child(n)`, `:not(sel)`, `:has(sel)`
- Combinators: descendant `parent child`, direct child `parent > child`, adjacent sibling `+`, general sibling `~`.
- Commas/Multiple: `header, main > .container`
- Wildcards: `*` or `parent > *`
- Host Injection (Experimental Component Spec): `:host([cond]) selector` allows components to internally detect properties passed to their encapsulating host shadow boundaries.
- Variables: `--var-name: val;` inside elements can be referenced via `var(--var-name, default)`. 
- Attribute Interpolation: Native `attr(custom-property, default)` binds HTML attribute text values into styles.
- Live Math: Native `calc(100% - 20)` mathematical interpolation (supports `+, -, *, /`).
- Color Blends: Native `color-mix(#color1, #color2, percentage)` interpolation mixing.

## Debugging

- **Console**: `compile.bat` (dev) prints logs to stdout
- **File**: All `sys.log()` output appends to `sys_log.txt`
- **VS Debugger**: Logs via `OutputDebugStringA` (visible in Output window)
- **Error traces**: Parse and runtime errors print file/line info

## Components

Components are `.dom` files that act as templates. They use shadow DOM:

```xml
<!-- components/Card.dom -->
<style>
    Card { bg: #181825; border-radius: 12; }
    Card:hover { bg: #1E1E2E; }
</style>
<ui>
    <Text class="card-title" text="Title" />
    <Text class="card-body" text="Content" />
</ui>
```

Use in your app:
```xml
<import src="components/Card.dom" as="Card" />
<Card id="my-card" x="20" y="20" width="300" height="200" />
```

Shadow DOM primitives are only visible via `shadowDomSelector`.
