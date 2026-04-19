# System API

The `sys` object binds raw Operating System (OS) APIs directly into the NativeDOM custom Javascript environment.

## Table of Contents
1. [System Core (sys.*)](#system-core-sys)
2. [Window Management (sys.window.*)](#window-management-syswindow)
3. [Screen & Inputs](#screen--inputs)

---

## System Core (sys.*)

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

---

## Window Management (sys.window.*)

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
