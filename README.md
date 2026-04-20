# NativeDOM Engine

A lightweight C++ custom UI framework with a built-in JS-like scripting engine. NativeDOM abstracts away Windows DWM/GDI hurdles and provides an HTML/CSS/JS-like abstraction layer heavily focused on speed and native OS integration!

---

## Master Directory

Welcome to the comprehensive NativeDOM documentation map. Everything you need to architect massive systems or tweak low-level window bindings has been cleanly routed and documented into verbose chapters.

### [1. Getting Started](docs/getting_started.md)
*   Quick Start & CLI commands.
*   `.dom` file architecture parsing.
*   Application bundling (AOT compilation & LZNT1 compressions).
*   Master Engine internal file graph.
*   Debugging pointers & Component basics.

### [2. DOM Engine API](docs/dom_api.md)
*   Light/Shadow element selections and insertions.
*   DOM property bounds mapping (e.g., `getBoundingRect`).
*   HTML Attribute passing overrides.
*   **Layout & Positioning Engine Map** (`auto-layout`, `row`, `col`).
*   Core Event propagation loops (`addEventListener`, Event Controls).

### [3. System Binding (sys.*)](docs/system_api.md)
*   Sub-process executions (`sys.exec`) and external DLL loading bindings (`sys.dllCall`).
*   Complete Window overrides (`sys.window.*`), including IPC `sys.sendMessage`.
*   Direct Screen monitor data (`sys.screen.*`).
*   Extremely low-level Thread Hooking for Kernel level key interception (`sys.keyboard.hook`).
*   Binary Clipboard data manipulation (`sys.clipboard.*`).
*   **COM/OLE Object Integration** for scripting ActiveX and OS components natively (`sys.com.*`).
*   See the **[WebView2 Integration Guide](docs/webview2_integration.md)** for embedding modern browser components.

### [4. Scripting Standard Library](docs/scripting_standard_library.md)
*   Exhaustive mapping of built-in `Math` modules.
*   String & Array prototyping functions (Includes heavily chained modifiers like `map`/`filter`/`reduce`).
*   Native Async Promisification models & Event loop boundaries.

### [5. CSS Features & Selectors](docs/css_features.md)
*   Supported styling combinators natively (General/Adjacent siblings).
*   Live runtime Math and attributes via `calc()` / `attr()` binding.
*   Shadow DOM experimental pseudo-host states (`:host()`).

---

## Example Usage

Run the unified Build interface directly from your repository root!

```bat
.\build.bat
```

Select `1` to run a legacy compilation string targeting `examples/main/app.dom`.
