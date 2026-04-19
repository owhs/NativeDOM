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

### DOM
- `document.querySelector(selector)` / `document.querySelectorAll(selector)`
- `document.shadowDomSelector(selector)` / `document.shadowDomSelectorAll(selector)`
- `document.getElementById(id)`
- `el.getAttribute(key)` / `el.setAttribute(key, value)`
- `el.classList.add(cls)` / `.remove(cls)` / `.toggle(cls)` / `.contains(cls)`
- `el.querySelector(selector)` (scoped)
- `el.addEventListener(type, callback)` / `el.removeEventListener(type)`
- `el.getBoundingRect()` / `el.show()` / `el.hide()` / `el.focus()` / `el.blur()`

### Platform
- `sys.log(msg)` — logs to console, OutputDebugString, and sys_log.txt
- `sys.window.resize(w, h)` / `.move(x, y)` / `.center()`
- `sys.window.minimize()` / `.maximize()` / `.restore()` / `.close()`
- `sys.window.setTitle(title)` / `.setOpacity(0-1)` / `.setAlwaysOnTop(bool)`
- `sys.window.getSize()` / `.getPosition()`
- `sys.screen.getInfo()` / `sys.screen.getDPI()`
- `sys.dllCall(dll, func, [args])` — call any DLL function
- `sys.sendMessage(hwnd, msg, wp, lp)` / `sys.findWindow(title)`

### Async
- `setTimeout(fn, ms)` / `setInterval(fn, ms)`
- `clearTimeout(id)` / `clearInterval(id)`
- `new Promise((resolve, reject) => ...)` / `.then()` / `.catch()`
- `fetch(url, { method, body, headers })` → Promise

### Events
- Mouse: `click`, `mousedown`, `mouseup`, `mousemove`, `mouseenter`, `mouseleave`, `contextmenu`, `wheel`
- Keyboard: `keydown`, `keypress`, `keyup`
- Focus: `focus`, `blur`
- Lifecycle: `load`, `resize`
- Custom: `document.dispatchEvent(CustomEvent("name", { detail: data }))`

### CSS Selectors
- Tag: `Text`, `Button`
- ID: `#my-id`
- Class: `.my-class`
- Attributes: `[attr]`, `[attr="val"]`, `[attr*="val"]`, `[attr^="val"]`, `[attr$="val"]`
- Pseudo: `:hover`, `:focused`, `:active`, `:first-child`, `:last-child`, `:nth-child(n)`, `:not(sel)`, `:has(sel)`
- Combinators: `parent > child`, `ancestor descendant`
- Chained: `Button.primary#submit[disabled]:hover`

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
