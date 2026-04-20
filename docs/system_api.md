# System API

The NativeDOM engine possesses a massive, bespoke Standard Library exposing Win32 internals, COM abstractions, asynchronous routines, and native threading block logic directly to standard Javascript closures!

## Table of Contents
1. [Core OS & Asynchronous Helpers](#core-os--asynchronous-helpers)
2. [Global Application Configuration (XML Properties)](#global-application-configuration)
3. [Window Management (sys.window.*)](#window-management-syswindow)
4. [Screen & Inputs](#screen--inputs)
5. [COM Objects (sys.com.*)](#com-objects-syscom)
6. [C++ Extensions API](#c-extensions-api)

---

## Global Application Configuration
NativeDOM permits incredibly powerful OS-level instructions natively in your root boot payload via attributes on the `<ui>` declaration! These parse via C++ inherently bypassing Javascript loops:
*   `instances`
    Configures OS-level `CreateMutexA` threading lock behavior for application duplicate handling:
    - `"allow"`: (Default) Unbounded duplicates run parallel.
    - `"focusIfOpen"`: Terminates duplicate startup execution naturally locking a mutex, forcing priority rendering focus upward cleanly on the pre-existing DOM.
    - `"warn"`: Gracefully yields a Message Box querying user intent. Falls through to `forceReplace`. 
    - `"forceReplace"`: Fires a `SendMessage` SIGKILL IPC target terminating legacy DOMs directly, inheriting the mutex perfectly natively!
*   `uac`
    Configures User Access Control elevation triggers:
    - `"optional"`: Standard.
    - `"required"`: Probes OS TokenElevation on C++ constructor boot logic. Forces a `ShellExecuteA(runas)` administrative OS overlay and tears down unprivileged processes autonomously prior to engine mapping!

---

## Core OS & Asynchronous Helpers

*   `sys.log(msg: string)`
    Prints output to the console (`stdout`), pushes it to the Windows Visual Studio `OutputDebugString` pipe, and appends it iteratively into `sys_log.txt` locally.
*   `sys.setLogEnabled(bool: enable)`
    A global suppression toggle. `sys.setLogEnabled(false)` completely halts all system logging pipelines for pristine console control.
*   `sys.time() -> number`
    Microsecond-bound system uptime resolution. Invokes `GetTickCount64()`.
*   `sys.sleep(ms: number)`
    Synchronously forces the central underlying NativeDOM C++ execution OS Thread to halt operations blockingly for exact intervals explicitly (avoiding confusing `Kernel32.dll` manual memory mappings!).
*   `sys.exec(cmd: string, hidden: bool=false) -> number`
    Forks a natively isolated CMD stream parsing straight Win32 logic via `CreateProcessA`. Set `hidden=true` to force a `SW_HIDE` structure, blocking annoying Windows generic box flashes! (Contrast this vs `sys.com.create("WScript.Shell")` stream captures which lock standard JS thread buffers naturally).
*   `sys.isCompiled() -> bool`
    Parses against the AOT Engine macro payload tracking logic, letting scripts branch logic out against `.exe` vs raw `.dom` deployments natively.
*   `sys.waitForWindow(target: string, callback: function, timeoutMs?: number, pollTickMs?: number)`
    An incredibly powerful native wrapper polyfilling deep integration mapping! Asynchronously polls standard OS hooks looking to lock window availability and executes the underlying standard closure upon execution naturally without hanging UI painting blocks! 
*   `sys.readText(filepath: string) -> string`
    Performs an immediate blocking read of the given generic file path. Output is parsed entirely as UTF-8.
*   `sys.writeText(filepath: string, data: string)`
    Performs an immediate blocking overwrite stream out to the given file path.
*   `sys.loadScript(filepath: string, asNewProcess: bool=false)`
    Hot-loads and parses an external `.dom` context map directly into memory. If `asNewProcess` is true, the file spins up completely isolated inside a cloned `dom.exe` child fork.
*   `sys.dllCall(dll: string, func: string, args: Array) -> number`
    Internal bridge for unsafe Foreign Function Interfaces. Invokes arbitrary dynamically resolved Windows `.dll` logic mapping basic JS strings/numbers to C ABI targets. Returns a numeric value (the uint64 return from the C function). **Protected by native Structured Exception Handling (SEH)**, meaning if the external DLL completely crashes, NativeDOM gracefully catches the hardware exception and returns an error string instead of crashing your UI! This makes calling external crypto/compression methods incredibly robust and fail-safe.
*   `sys.dllCallString(dll: string, func: string, args: Array) -> string`
    Identical to `sys.dllCall`, but interprets the DLL's return value as a `const char*` string pointer instead of a number. Use this when the DLL function returns a string (e.g., `QueryString` from the WebView2 plugin). The pointer is safely dereferenced using SEH protection.
    ```javascript
    // Example: Reading a URL string from the WebView2 plugin
    sys.dllCall("webview_plugin.dll", "ExecuteCommand", [10, 0, 0, 0, 0, 0, 0, 0]);
    let url = sys.dllCallString("webview_plugin.dll", "QueryString", [0, 0, 0, 0, 0, 0, 0, 0]);
    sys.log("Current URL: " + url);
    ```
*   `sys.loadExtension(dllPath: string) -> bool`
    Advanced engine feature for loading comprehensive C++ extensions (e.g. video player tools, networking layers, WebView2 embeds, or cryptography suites). Loads the DLL, probes for a `NativeDOM_Init` export, and hands it the NativeDOM execution pointers safely.
*   `sys.sendMessage(hwnd: number, msg: number, wParam: number, lParam: number)`
    The lowest-level OS IPC primitive. Dispatches raw Windows UI messages securely to internal or external process queues.
*   `sys.findWindow(title: string) -> number`
    Scans the visible Windows desktop to locate exact titlebar strings and extract their internal OS handle (`HWND`).
*   `sys.getHwnd() -> number`
    Returns the Native DOM application's core memory handle mapping (`HWND`), letting you dispatch native procedures targeting your application's frame buffer!

### Asynchronous Closures
NativeDOM correctly structures loops to inherently map AST pointers straight to internal 16-Millisecond render ticks (`WM_TIMER` dispatch block)!
*   `setInterval(callback: function, ms: number) -> handle`
*   `clearInterval(handle: number)`
*   `setTimeout(callback: function, ms: number) -> handle`
*   `clearTimeout(handle: number)`

---

## Window Management (sys.window.*)

*   `sys.window.find(title: string) -> number`
    Queries arbitrary generic external windows by checking deep `FindWindowA` OS boundaries natively without COM locks.
*   `sys.window.focus(processNameOrHwnd: string|number) -> bool`
    Forcefully intercepts execution order routines shifting absolute `SetForegroundWindow` visual OS level targets automatically.
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
    *Note: The `id` is an arbitrary integer you provide. When the OS detects the hotkey, it dispatches the `hotkey` event with this exact `id` so you know which key combination was pressed.*
    ```javascript
    // Example: Listening for a global hotkey
    sys.window.registerHotkey(1, "CTRL+ALT+T");
    document.addEventListener("hotkey", (e) => {
        if (e.id === 1) { sys.log("Hotkey pressed!"); }
    });
    ```
*   `sys.window.unregisterHotkey(id: number)`
    Releases the global DWM lock over the bound keys.

---

## Screenshot & Pixel Tools

*   `sys.screenshot(filepath?: string, x?: number, y?: number, w?: number, h?: number) -> bool`
    Captures the NativeDOM application window to a BMP file using `BitBlt`. If no region is specified, captures the entire client area.
    ```javascript
    sys.screenshot("full_window.bmp");           // Entire window
    sys.screenshot("region.bmp", 50, 50, 200, 150);  // Specific region
    ```
*   `sys.getPixelColor(x?: number, y?: number) -> {r, g, b, hex, x, y}`
    Returns the pixel color at the given screen coordinates. If no coordinates are provided, uses the current cursor position. Useful for color picker tools, design utilities, and automated testing.
    ```javascript
    let px = sys.getPixelColor();       // Under cursor
    sys.log(px.hex);                     // "#FF6600"
    sys.log(px.r + ", " + px.g + ", " + px.b);  // "255, 102, 0"
    
    let px2 = sys.getPixelColor(500, 300);  // Specific coords
    ```

---

## Screen & Inputs

### sys.screen.*
*   `sys.screen.getInfo()`
    Returns the core layout details encapsulating raw `width` / `height` pixel resolutions, the hardware coordinate `monitors` count, and a `workArea {x, y, width, height}` subset boundary identifying visible space devoid of taskbars.
*   `sys.screen.getDPI() -> number`
    Extracts the display scaling percentage configured globally inside Windows Settings.
*   `sys.screen.getMousePosition() -> {x, y}`
    Provides raw active coordinates tracking exactly where the pointer rests unconditionally.
*   `sys.screen.getMonitorAt(x: number, y: number) -> {name, x, y, width, height, isPrimary}`
    Cross-references raw multi-head parameters identifying explicit dimensions for targeted coordinate arrays.

### sys.keyboard.*
*   `sys.keyboard.hook(callback: Function)` / `sys.keyboard.unhook()`
    Establishes an extreme `WH_KEYBOARD_LL` low-level intercept over the complete OS loop. Returning `true` out of the callback forces the engine to burn the keystroke directly out of Windows buffers, effectively black-holing the input!
    ```javascript
    sys.keyboard.hook((e) => {
        sys.log(`Globally Intercepted keyCode: ${e.vkCode}`);
        return true; // Completely stops Windows from detecting this key
    });
    ```

### sys.clipboard.*
*   `sys.clipboard.getText() -> string`
    Decodes the raw text currently stored implicitly globally inside the shared Windows clipboard queue.
*   `sys.clipboard.setText(string: payload)`
    Overwrites the OS buffer injecting exact memory configurations available to external processes.
*   `sys.clipboard.clear()`
    Wipes the clipboard buffer totally clean preventing accidental multi-read memory leaks.
*   `sys.clipboard.getFormat(id) -> Array<number>`, `sys.clipboard.setFormat(id, buffer)`
    Binary API wrapper. Permits direct format abstraction (mapping numerical integers like `CF_BITMAP`) across arbitrary bytes!

---

## COM Objects (sys.com.*)

NativeDOM includes a highly advanced COM (Component Object Model) and OLE Automation bridge. This allows you to instantiate and control Windows OS components and ActiveX controls directly from Javascript, without requiring intermediate C++ plugins.

Data types (Strings, Numbers, Booleans, and Arrays) are automatically mapped back and forth between JS and COM `VARIANT`s.

*   `sys.com.create(progId: string) -> object`
    Instantiates a brand new COM object using its OS ProgID (e.g. `"Scripting.FileSystemObject"`, `"SAPI.SpVoice"`, `"InternetExplorer.Application"`). Returns a wrapped native object exposing interaction methods, or `null` if the object couldn't be created.
*   `sys.com.getActive(progId: string) -> object`
    Hooks into an already-running COM object instance from the OS (via `GetActiveObject()`).
*   `sys.com.releaseAll()`
    Forcefully calls `Release()` on all instantiated COM interfaces. Good practice before closing an application to guarantee cleanup.

### Interacting with Wrapped COM Objects

The object returned by `sys.com.create` exposes the following interface:

*   `obj.call(methodName: string, ...args) -> any`
    Invokes a method on the COM interface. COM arrays (`SafeArray`) are automatically converted into Javascript Arrays.
*   `obj.get(propertyName: string, ...indexes?) -> any`
    Retrieves a property value. You can optionally supply extra parameters for parameterized properties (like retrieving an item from a collection array by index).
*   `obj.set(propertyName: string, value: any) -> boolean`
    Overwrites a property on the COM interface. Returns `true` if successful.
*   `obj.release()`
    Immediately releases this specific COM pointer from memory.
*   `obj.forEach(callback: function)`
    If the COM object is a Collection (implements `IEnumVARIANT` or the `_NewEnum` property), you can iterate entirely through it. The callback receives `(item, index)`.
*   `obj.toArray() -> Array`
    If the COM object is a Collection, attempts to aggressively iterate and map the entire collection into a standard Javascript Array!

**Example Usage**: Text-to-Speech

```javascript
let voice = sys.com.create("SAPI.SpVoice");
if (voice) {
    voice.set("Volume", 100);
    voice.call("Speak", "Warning, system breach detected.");
    voice.release();
}
```

---

## C++ Extensions API

For high-performance or complex libraries (e.g., video player toolkits, fully threaded Websocket servers, or embedding Edge WebView2 instances directly into the app), standard `sys.dllCall` integer-passing is insufficient. 

To solve this, NativeDOM allows you to author complete **Native Extensions** that plug raw C/C++ logic seamlessly into the NativeDOM OpenGL and OS environment!

### Authoring an Extension
Create a standard Windows DLL and export the exact signature ` NativeDOM_Init(void** ctxArgs)`. NativeDOM securely passes an array of its internal core memory pointers. 

**Example Extension (`my_plugin.dll`)**:
```cpp
#include <windows.h>

// NanoVG struct forward declaration if you intend to draw to the hardware DOM context
struct NVGcontext;
class Interpreter;

extern "C" __declspec(dllexport) void __stdcall NativeDOM_Init(void** ctxArgs) {
    // Standard extraction of NativeDOM runtime pointers:
    HWND windowHandle = (HWND)ctxArgs[0];
    NVGcontext* nvg = (NVGcontext*)ctxArgs[1];
    Interpreter* interp = (Interpreter*)ctxArgs[2];

    // DO ANYTHING HERE:
    // - Subclass 'windowHandle' to embed an active WebView2 HWND or Video HWND over the DOM.
    // - Register new Javascript objects via the Interpreter directly.
    // - Boot up background threads for high-speed Async networking hooks (WebSockets/TCP).
    // - Cast NanoVG commands using the 'nvg' pointer to render completely custom geometry natively inside the engine's standard redraw loop!
}
```

### Loading in Javascript
Because NativeDOM wraps the memory boundary in hardware SEH (Structured Exception Handling), you don't need to worry about typos crashing your wrapper application.
```javascript
let success = sys.loadExtension("plugins/my_plugin.dll");
if (!success) { sys.log("Graceful fail catch: Extension crashed or missing!"); }
```
This architecture makes NativeDOM endlessly adaptable to **any scenario or use case**.
