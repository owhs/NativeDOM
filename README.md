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
- Navigation: `document.querySelector(selector)` / `document.querySelectorAll(selector)`
- Navigation: `document.shadowDomSelector(selector)` / `document.shadowDomSelectorAll(selector)`
- Navigation: `document.getElementById(id)`
- Creation: `document.createElement(tag)`
- Element Navigation: `el.querySelector(selector)`, `el.querySelectorAll(selector)`, `el.shadowDomSelector(selector)`
- Relationships: `el.parentElement`, `el.children`, `el.appendChild(child)`
- Attributes: `el.getAttribute(key)` / `el.setAttribute(key, value)`
- Classes: `el.classList.add(cls)` / `.remove(cls)` / `.toggle(cls)` / `.contains(cls)`
- Text: `el.textContent` (property) / `el.innerText` (property)
- Geometry: `el.getBoundingRect()` returns `{x, y, width, height}`
- State: `el.show()` / `el.hide()` / `el.focus()` / `el.blur()`

### HTML Properties & Inline States
All CSS properties can be specified inline as HTML attributes (e.g. `<Button border-radius="12" />`).
You can also apply inline state-based properties by prefixing the attribute name with the state pseudo-identifier:
- `<Button hover:bg="#ff0000" focused:border="#00ff00" active:opacity="0.5" />`

### System (sys.*)
- `sys.log(msg)` — logs to console, OutputDebugString, and sys_log.txt.
- `sys.setLogEnabled(bool)` — toggle engine logging globally (e.g. `sys.setLogEnabled(false)`).
- `sys.time()` — returns `GetTickCount64()` (milliseconds since system boot).
- `sys.exec(cmd, [hidden])` — run a shell process. Returns the process ID.
- `sys.readText(filepath)` — reads file entirely into a UTF-8 string.
- `sys.writeText(filepath, data)` — writes string data to a file.
- `sys.loadScript(filepath, [asNewProcess])` — loads and parses an external `.dom` file dynamically into the application.
- `sys.dllCall(dll, func, [args])` — raw FFI bridge to call standard DLL procedures.
- `sys.sendMessage(hwnd, msg, wp, lp)` — dispatch a direct raw WIN32 system message.
- `sys.findWindow(title)` — returns the raw HWND long ID for a window matching the exact title.
- `sys.getHwnd()` — returns the HWND long ID of the underlying application window.

### Window (sys.window.*)
- `sys.window.resize(w, h)` / `sys.window.move(x, y)` / `sys.window.center()`
- `sys.window.hide()` / `sys.window.show()`
- `sys.window.minimize()` / `sys.window.maximize()` / `sys.window.restore()` / `sys.window.close()`
- `sys.window.setFullscreen(bool)` / `sys.window.isMaximized()` -> bool
- `sys.window.setTitle(title)` / `sys.window.setOpacity(0-1)` / `sys.window.setAlwaysOnTop(bool)`
- `sys.window.setIcon(filepath)` -> accepts `.ico` paths.
- `sys.window.getSize()` -> `{width, height}` / `sys.window.getPosition()` -> `{x, y}`
- `sys.window.getActiveProcessName()` — grabs internal focus from Windows Desktop.
- `sys.window.registerHotkey(id, "key")` — register a global Windows hotkey.
- `sys.window.unregisterHotkey(id)`
  ```javascript
  // Example: Listening for a global hotkey
  sys.window.registerHotkey(1, "CTRL+ALT+T");
  document.addEventListener("hotkey", (e) => {
      if (e.id === 1) { sys.log("Hotkey pressed!"); }
  });
  ```

### Screen & Inputs (sys.screen.*, sys.keyboard.*, sys.clipboard.*)
- `sys.screen.getInfo()` — returns structure containing full active `width`, `height`, hardware monitor count, and Windows `workArea` `{x,y,width,height}` boundaries. 
- `sys.screen.getDPI()` — system scale.
- `sys.screen.getMousePosition()` -> `{x, y}`
- `sys.screen.getMonitorAt(x, y)` -> `{name, x, y, width, height, isPrimary}`
- `sys.keyboard.hook(callback)` / `sys.keyboard.unhook()` — sets up global LowLevel keyboard interception hooks. The callback receives a keyEvent object. Return `true` to block the key input globally!
- `sys.clipboard.getText()` / `sys.clipboard.setText(string)` / `sys.clipboard.clear()`
- `sys.clipboard.getFormat(id)` / `sys.clipboard.setFormat(id, buffer)` — binary clipboard format interactions.

### Async
- `setTimeout(fn, ms)` / `setInterval(fn, ms)`
- `clearTimeout(id)` / `clearInterval(id)`
- `new Promise((resolve, reject) => ...)` / `.then()` / `.catch()`
- `fetch(url, { method, body, headers })` → Promise that resolves `{ ok, status, body, text(), json() }`

### Events
- **Document Level Listeners**: `document.addEventListener(type, cb)`, `document.removeEventListener(type)`
- **Element Listeners**: `el.addEventListener(type, cb)`, `el.removeEventListener(type)`
- **Dispatchers**: `document.dispatchEvent(event)`, `el.dispatchEvent(event)`
- **Event Controls**: `e.preventDefault()`, `e.stopPropagation()`, `e.stopImmediatePropagation()`
- **Global Events**: `hotkey` (provides `e.id`, `e.modifiers`, `e.vkCode`)
- **Keyboard Events**: `e.keyCode`, `e.key`
- **Mouse Events**: `e.clientX`, `e.clientY`, `e.target`
- Custom: `new CustomEvent("name", { detail: data })`

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
