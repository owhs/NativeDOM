# WebView2 Plugin Integration Guide

This document covers everything needed to embed, control, and extend Microsoft Edge WebView2 inside NativeDOM applications. The WebView2 plugin enables full browser embedding with a rich JavaScript API for navigation, content control, bidirectional messaging, lifecycle management, and capture.

## Table of Contents
1. [Architecture Overview](#architecture-overview)
2. [Quick Start](#quick-start)
3. [Building the Plugin](#building-the-plugin)
4. [WebView Component API](#webview-component-api)
5. [Navigation](#navigation)
6. [JavaScript Execution](#javascript-execution)
7. [Host ↔ Page Messaging](#host--page-messaging)
8. [Content Settings](#content-settings)
9. [Lifecycle Management](#lifecycle-management)
10. [Display & Geometry](#display--geometry)
11. [Capture & Print](#capture--print)
12. [Multi-Instance Architecture](#multi-instance-architecture)
13. [Plugin C++ Command Reference](#plugin-c-command-reference)
14. [Advanced Patterns](#advanced-patterns)
15. [Troubleshooting](#troubleshooting)

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────┐
│               NativeDOM Application                  │
│  ┌───────────┐  ┌──────────────────────────────┐    │
│  │ app.dom   │  │  WebView.dom (Component)     │    │
│  │ (JS API)  │──│  navigate(), postMessage()   │    │
│  └───────────┘  │  executeScript(), reload()   │    │
│                 └──────────┬───────────────────┘    │
│                            │ sys.dllCall()           │
│                            ▼                         │
│  ┌──────────────────────────────────────────────┐   │
│  │          webview_plugin.dll (C++)             │   │
│  │  ExecuteCommand(cmd, p1, p2, ...)            │   │
│  │  QueryString(queryId)                         │   │
│  │  ──────────────────────────────────────────── │   │
│  │  ICoreWebView2Controller  →  put_Bounds()    │   │
│  │  ICoreWebView2            →  Navigate()      │   │
│  │  ICoreWebView2Settings   →  put_IsScript..() │   │
│  └──────────────────────────────────────────────┘   │
│                            │                         │
│                            ▼                         │
│  ┌──────────────────────────────────────────────┐   │
│  │           Microsoft Edge WebView2             │   │
│  │  (Chromium-based browser engine, COM hosted)  │   │
│  └──────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────┘
```

**Why a C++ DLL?** WebView2 uses COM interfaces (`ICoreWebView2`, `ICoreWebView2Controller`, etc.) which require native Win32/COM interop. NativeDOM's JS engine cannot create COM objects or handle COM callbacks directly. The C++ DLL acts as the native bridge — but the full API surface is exposed cleanly to JavaScript through `sys.dllCall` and `sys.dllCallString`.

---

## Quick Start

### 1. Project Structure
```
your-app/
├── app.dom           ← Your application
├── WebView.dom       ← WebView component (import this)
├── dom.exe           ← NativeDOM engine
├── webview_plugin.dll← Compiled WebView2 bridge
└── WebView2Loader.dll← Microsoft runtime loader
```

### 2. Minimal app.dom
```xml
<import src="WebView.dom" as="WebView" />

<ui title="My Browser" width="1024" height="768" bg="#11111B">
    <div layout="col" width="100%" height="100%">
        <WebView id="myBrowser" url="https://example.com" width="100%" height="100%" />
    </div>
</ui>

<script>
    (function() {
        let browser = document.getElementById("myBrowser");
        
        // Navigate programmatically
        browser.navigate("https://google.com");
        
        // Execute JS inside the web page
        browser.executeScript("document.title = 'Hello from NativeDOM!'");
        
        // Listen for messages from the page
        browser.onMessage(function(msg) {
            sys.log("Page says: " + msg);
        });
    })();
</script>
```

---

## Building the Plugin

### Prerequisites
- **Microsoft Visual Studio** (with C++ Desktop Development workload)
- **WebView2 Runtime** installed on the target system (ships with Windows 11+, downloadable for Windows 10)

### Build Command
```batch
cd examples\webview\plugin
build.bat
```

The build script:
1. Downloads the WebView2 NuGet SDK automatically (first run only)
2. Compiles `webview_plugin.cpp` into `webview_plugin.dll`
3. Deploys both `webview_plugin.dll` and `WebView2Loader.dll` to the NativeDOM root

### Manual Build (Developer Command Prompt)
```batch
cl.exe /nologo /O2 /LD /MD /EHsc webview_plugin.cpp ^
    /I"sdk\build\native\include" ^
    /link /OUT:webview_plugin.dll ^
    "sdk\build\native\x64\WebView2Loader.dll.lib" ^
    User32.lib Advapi32.lib Ole32.lib Shlwapi.lib
```

---

## WebView Component API

The `WebView.dom` component exposes methods by attaching them directly to the element wrapper using the `self.methodName = function() { ... }` pattern. This is used instead of ES6 `class` syntax because the NativeDOM interpreter does not support `class` declarations.

### Defining Component Methods (self.* Pattern)

When building custom components in NativeDOM, you attach scriptable methods to the component's element wrapper retrieved via `document.getElementById()`:

```javascript
(function() {
    let self = document.getElementById("myComponent");
    
    // Define a public method callable from outside the component
    self.doSomething = function(value) {
        sys.log("Called with: " + value);
        self.setAttribute("data-result", value);
    };
    
    // Define a getter
    self.getState = function() {
        return self.getAttribute("data-state");
    };
})();
```

**Why this works:** NativeDOM caches element wrappers in an internal `elementCache`. When any script calls `document.getElementById("myComponent")`, it receives the **same object reference**. Methods attached to this wrapper persist and are accessible from any script block, including the parent app.

---

## Navigation

| Method | Description |
|---|---|
| `browser.navigate(url)` | Navigate to a URL |
| `browser.navigateToString(html)` | Load raw HTML content |
| `browser.reload()` | Reload the current page |
| `browser.stop()` | Stop loading |
| `browser.goBack()` | Navigate back in history |
| `browser.goForward()` | Navigate forward in history |
| `browser.getNavState()` | Returns `{ canGoBack: bool, canGoForward: bool }` |
| `browser.getURL()` | Returns the current page URL as a string |
| `browser.getTitle()` | Returns the current page title as a string |

### Examples
```javascript
// Basic navigation
browser.navigate("https://google.com");

// Check if we can go back
let state = browser.getNavState();
if (state.canGoBack) browser.goBack();

// Display current URL
let url = browser.getURL();
sys.log("Currently at: " + url);
```

---

## JavaScript Execution

Execute arbitrary JavaScript inside the WebView2 page:

```javascript
// Simple execution
browser.executeScript("alert('Hello!')");

// Modify page DOM
browser.executeScript("document.body.style.backgroundColor = '#1E1E2E'");

// Complex injection
browser.executeScript(`
    let heading = document.createElement('h1');
    heading.textContent = 'Injected by NativeDOM!';
    heading.style.color = '#89B4FA';
    document.body.prepend(heading);
`);
```

---

## Host ↔ Page Messaging

WebView2 provides a bidirectional messaging channel between the NativeDOM host and the web page running inside the WebView.

### Sending Messages TO the Page (Host → Page)

From your NativeDOM script:
```javascript
browser.postMessage("Hello from NativeDOM!");
browser.postMessage(JSON.stringify({ action: "update", data: 42 }));
```

Inside the web page (the page loaded in WebView2):
```javascript
// The page registers a listener on the Chrome WebView bridge
window.chrome.webview.addEventListener('message', (event) => {
    console.log('Received from host:', event.data);
});
```

### Receiving Messages FROM the Page (Page → Host)

Inside the web page:
```javascript
// The page sends a message to the NativeDOM host
window.chrome.webview.postMessage("Page data: " + document.title);
```

In your NativeDOM script:
```javascript
browser.onMessage(function(msg) {
    sys.log("Received from page: " + msg);
    
    // Parse JSON messages
    let data = JSON.parse(msg);
    if (data.action === "clicked") {
        // React to page events
    }
});
```

### Global Event Alternative
You can also listen on the document for `webview_message` events:
```javascript
document.addEventListener("webview_message", (e) => {
    sys.log("WebView message: " + e.detail);
});
```

---

## Content Settings

Control what the WebView2 instance is allowed to do:

| Method | Description |
|---|---|
| `browser.setScriptEnabled(bool)` | Enable/disable JavaScript execution in the page |
| `browser.setWebMessageEnabled(bool)` | Enable/disable the web messaging channel |
| `browser.setDialogsEnabled(bool)` | Enable/disable `alert()`, `confirm()`, `prompt()` |
| `browser.setStatusBarEnabled(bool)` | Show/hide the bottom status bar |
| `browser.setDevToolsEnabled(bool)` | Allow/block opening DevTools |
| `browser.setContextMenuEnabled(bool)` | Enable/disable right-click context menu |
| `browser.setZoomEnabled(bool)` | Enable/disable Ctrl+scroll zoom |
| `browser.setUserAgent(string)` | Override the user agent string |

### Examples
```javascript
// Create a locked-down kiosk browser
browser.setDevToolsEnabled(false);
browser.setContextMenuEnabled(false);
browser.setZoomEnabled(false);
browser.setDialogsEnabled(false);

// Mobile simulation
browser.setUserAgent("Mozilla/5.0 (iPhone; CPU iPhone OS 16_0 like Mac OS X)");
```

---

## Lifecycle Management

| Method | Description |
|---|---|
| `browser.setVisible(bool)` | Show/hide the native WebView overlay |
| `browser.openDevTools()` | Open the Chrome DevTools window |
| `browser.close()` | Destroy the WebView instance permanently |
| `browser.suspend()` | Minimize resource usage (ICoreWebView2_3) |
| `browser.resume()` | Resume from suspension |

### Visibility
When hiding the WebView, the native Chromium window is hidden but the session state (cookies, history, DOM) is preserved:
```javascript
browser.setVisible(false);  // Hidden but alive
// ... later ...
browser.setVisible(true);   // Instantly restored
```

### Suspend/Resume
For background tabs or minimized states, suspend reduces CPU/memory usage:
```javascript
// User switches away
browser.suspend();

// User returns
browser.resume();
```

---

## Display & Geometry

| Method | Description |
|---|---|
| `browser.updateGeometry()` | Sync the native overlay position to the NativeDOM layout |
| `browser.setZoom(factor)` | Set zoom level (e.g., `1.5` = 150%) |
| `browser.setBackgroundColor(r, g, b, a)` | Set the WebView background color (0-255 each) |

The WebView component automatically tracks geometry changes at ~60fps and only issues DLL calls when the bounds actually change (dirty-checked). You typically don't need to call `updateGeometry()` manually.

### Transparent Background
```javascript
browser.setBackgroundColor(0, 0, 0, 0);  // Fully transparent
```

---

## Capture & Print

| Method | Description |
|---|---|
| `browser.capturePreview(filepath, format)` | Capture the WebView content to a file |
| `browser.print()` | Open the system print dialog |

### Screenshot
```javascript
// Save as PNG
browser.capturePreview("page_screenshot.png", "png");

// Save as JPEG
browser.capturePreview("page_photo.jpg", "jpeg");
```

### System Screenshot (NativeDOM Window)
Separately from WebView2 capture, NativeDOM has built-in screenshot tools:
```javascript
// Capture the entire NativeDOM window
sys.screenshot("window.bmp");

// Capture a specific region (x, y, width, height)
sys.screenshot("region.bmp", 100, 50, 400, 300);

// Get pixel color at cursor position
let px = sys.getPixelColor();
sys.log("Color at cursor: " + px.hex);  // "#FF6600"
sys.log("RGB: " + px.r + "," + px.g + "," + px.b);

// Get pixel color at specific coordinates
let px2 = sys.getPixelColor(500, 300);
```

---

## Multi-Instance Architecture

The current WebView2 plugin supports a single browser instance per application window. To support multiple "tabs" or instances, extend the architecture:

### Approach: Multiple Plugin DLLs
Create copies of the plugin DLL with different names, each managing its own WebView2 instance:
```javascript
sys.loadExtension("webview_tab1.dll");
sys.loadExtension("webview_tab2.dll");

sys.dllCall("webview_tab1.dll", "ExecuteCommand", [2, "https://google.com", 0, 0, 0, 0, 0, 0]);
sys.dllCall("webview_tab2.dll", "ExecuteCommand", [2, "https://github.com", 0, 0, 0, 0, 0, 0]);
```

### Approach: Instance ID Parameter
A more sophisticated approach would modify the plugin to accept an instance ID parameter and maintain an internal map of controllers. This would require extending the `ExecuteCommand` signature to include an instance parameter.

### Headless / Background Fetches
For headless page loading (no visible window), initialize the WebView but keep it hidden:
```javascript
browser.setVisible(false);
browser.navigate("https://api.example.com/data");

// Wait for page load, then extract data
setTimeout(() => {
    browser.executeScript("window.chrome.webview.postMessage(document.body.innerText)");
}, 3000);

browser.onMessage(function(msg) {
    let data = JSON.parse(msg);
    sys.log("Fetched data: " + data.result);
});
```

---

## Plugin C++ Command Reference

The plugin exposes two functions: `ExecuteCommand` (numeric operations) and `QueryString` (string data retrieval).

### ExecuteCommand(cmd, p1, p2, p3, p4, p5, p6, p7) → number
Called via `sys.dllCall("webview_plugin.dll", "ExecuteCommand", [cmd, ...args])`.

| Cmd | Name | Parameters | Returns |
|-----|------|-----------|---------|
| 1 | Initialize | — | 1 on success |
| 2 | Navigate | p1=URL string ptr | 1 on success |
| 3 | Set Bounds | p1=x, p2=y, p3=w, p4=h | 1 on success |
| 4 | Execute JS | p1=script string ptr | 1 on success |
| 5 | Set Visible | p1=1 (show) / 0 (hide) | 1 on success |
| 6 | Reload | — | 1 on success |
| 7 | Stop | — | 1 on success |
| 8 | Go Back | — | 1 on success |
| 9 | Go Forward | — | 1 on success |
| 10 | Query URL | — (stores in buffer) | 1 on success |
| 11 | Query Title | — (stores in buffer) | 1 on success |
| 12 | Set User Agent | p1=UA string ptr | 1 on success |
| 13 | Post Web Message | p1=message string ptr | 1 on success |
| 14 | Set Setting | p1=setting_id, p2=enable(1/0) | 1 on success |
| 15 | Open DevTools | — | 1 on success |
| 16 | Close/Destroy | — | 1 on success |
| 17 | Set Zoom | p1=factor×100 (e.g. 150) | 1 on success |
| 18 | Print | — | 1 on success |
| 19 | Nav State | — | bitfield: bit0=canGoBack, bit1=canGoForward |
| 20 | Capture Preview | p1=filepath ptr, p2=format (0=PNG, 1=JPEG) | 1 on success |
| 21 | Poll Web Message | — | 1 if message available |
| 22 | Set BG Color | p1=R, p2=G, p3=B, p4=A | 1 on success |
| 23 | Suspend | — | 1 on success |
| 24 | Resume | — | 1 on success |

### Setting IDs (for cmd 14)
| ID | Setting |
|----|---------|
| 0 | JavaScript enabled |
| 1 | Web messaging enabled |
| 2 | Default script dialogs (alert/confirm/prompt) |
| 3 | Status bar visible |
| 4 | DevTools accessible |
| 5 | Context menus enabled |
| 6 | Zoom control enabled |

### QueryString(queryId) → const char*
Called via `sys.dllCallString("webview_plugin.dll", "QueryString", [queryId, ...])`.

| queryId | Returns |
|---------|---------|
| 0 | Current URL (populated by cmd 10) |
| 1 | Current title (populated by cmd 11) |
| 2 | Last received web message |

---

## Advanced Patterns

### CSS Injection
```javascript
browser.executeScript(`
    let style = document.createElement('style');
    style.textContent = 'body { font-family: "Inter", sans-serif !important; }';
    document.head.appendChild(style);
`);
```

### Content Blocking (via JS)
```javascript
// Block images
browser.executeScript(`
    new MutationObserver((mutations) => {
        for (let m of mutations) {
            for (let node of m.addedNodes) {
                if (node.tagName === 'IMG') node.remove();
            }
        }
    }).observe(document.body, { childList: true, subtree: true });
`);
```

### Page Data Extraction
```javascript
// Extract all links from the page
browser.executeScript(`
    let links = Array.from(document.querySelectorAll('a[href]'))
        .map(a => a.href)
        .filter(h => h.startsWith('http'));
    window.chrome.webview.postMessage(JSON.stringify({ type: 'links', data: links }));
`);

browser.onMessage(function(msg) {
    let parsed = JSON.parse(msg);
    if (parsed.type === 'links') {
        sys.log("Found " + parsed.data.length + " links");
    }
});
```

### Dark Mode Override
```javascript
browser.executeScript(`
    document.documentElement.style.filter = 'invert(1) hue-rotate(180deg)';
    document.querySelectorAll('img, video').forEach(el => {
        el.style.filter = 'invert(1) hue-rotate(180deg)';
    });
`);
```

---

## Troubleshooting

### WebView2 stuck on about:blank
**Cause:** The `class` syntax in JS scripts is not supported by the NativeDOM interpreter. Use IIFE + `self.method = function()` patterns instead.

**Fix:** Rewrite any `class Component { onMount() { ... } }` to:
```javascript
(function() {
    let self = document.getElementById("myComponent");
    self.myMethod = function() { ... };
})();
```

### Tiny/misplaced WebView overlay
**Cause:** The `<ui>` element in WebView.dom has no `width`/`height`, defaulting to 100×30.

**Fix:** Always set `<ui width="100%" height="100%">` on the component shadow root.

### Buttons/events not working
**Cause:** ES6 `class` for the app script means the methods and event bindings inside `onMount()` never execute.

**Fix:** Use IIFE `(function() { ... })();` instead of `class App { onMount() { ... } }`.

### WebView2Loader.dll not found
**Fix:** Run `build.bat` from the plugin directory — it auto-downloads and deploys the SDK. Ensure `WebView2Loader.dll` is next to `dom.exe`.

### Navigation log shows "cached" but never navigates
**Cause:** The WebView2 controller hasn't finished async COM initialization yet.

**Fix:** The plugin automatically queues pending navigations and replays them when the controller is ready. Ensure the `customMessage` event with `lParam === 999` is reaching JS.
