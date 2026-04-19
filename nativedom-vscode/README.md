# NativeDOM VSCode Extension

Comprehensive language support for NativeDOM `.dom` files.

## Features

### Syntax Highlighting
Full TextMate grammar with embedded language support:
- **HTML-like markup** — element tags (ui, View, Text, Button, Image, TextInput, WebView), attributes, and pseudo-state inline attrs (`hover:bg="..."`)
- **CSS-like styling** — NativeDOM properties (`bg`, `layout`, `gap`, `justify`), variables (`var(--name)`), functions (`calc()`, `color-mix()`, `attr()`)
- **JavaScript** — full JS highlighting with NativeDOM API recognition (`sys.*`, `document.*`)
- **Import tags** — special highlighting for `<import>` with clickable src paths

### IntelliSense (Auto-Completion)
Context-aware completions across all three zones:
- **HTML**: Element names, attributes, pseudo-state inline props, imported component names
- **CSS**: Property names, value suggestions, functions, selectors, pseudo-classes
- **JavaScript**: Full `sys.*` API tree (`sys.window.*`, `sys.screen.*`, `sys.keyboard.*`, `sys.clipboard.*`), `document.*` methods, event types, element methods

### Hover Documentation
Rich inline documentation for:
- All NativeDOM elements (`<ui>`, `<View>`, `<Text>`, `<Button>`, etc.)
- All CSS properties and their valid values
- All `sys.*` APIs with signatures
- DOM manipulation methods
- Pseudo-classes (`:hover`, `:focused`, `:active`, `:host`)
- CSS functions (`var()`, `calc()`, `color-mix()`, `attr()`)

### Real-Time Diagnostics
Catches errors as you type:
- Unclosed `<style>`, `<script>`, `<ui>` tags
- Unbalanced `{}`/`()` in style and script blocks
- Missing `<ui>` root element
- ES6 `class` usage (unsupported in NativeDOM)
- `async function` usage warning
- `:focus` → `:focused` correction hint
- `background-color` → `bg` hint
- Import validation (missing `src`/`as`, duplicates)
- Missing CSS semicolons

### Color Decorations
Inline color swatches and color picker for all hex color formats:
- `#RGB` (shorthand)
- `#RRGGBB` (standard)
- `#RRGGBBAA` (with alpha)

### Go to Definition
Ctrl+Click navigation:
- Import `src` paths → opens the component `.dom` file
- Component tag names → resolves to their import source
- `getElementById("id")` → jumps to the element with that `id`
- `sys.loadScript("path")` → opens the referenced file

### Snippets (40+)
Comprehensive snippet library:
- `dom-app` — Full application scaffold
- `dom-component` — Component with IIFE pattern
- `dom-import` — Import statement
- `dom-ui` — Root element with attributes
- `dom-text`, `dom-button`, `dom-view`, `dom-image`, `dom-input` — Elements
- `dom-on` — Event listener with type picker
- `dom-fetch` — Async fetch request
- `dom-dllcall` — FFI bridge call
- `dom-hotkey` — Global hotkey registration
- `dom-webview` — WebView2 browser embed
- `dom-frameless` — Frameless window with custom titlebar
- `dom-host` — :host() conditional styling
- And many more...

### Commands
- **F5** — `NativeDOM: Run Current .dom File` (runs with dom.exe)
- **NativeDOM: Build Bundle** — opens build.bat
- **NativeDOM: Open Documentation** — opens docs

## Installation

```bash
cd nativedom-vscode
npm install
npm run compile
```

### Install as Local Extension
```bash
# Link to VS Code extensions
code --install-extension . --force
```

### Or Package as .vsix
```bash
npx vsce package
code --install-extension nativedom-*.vsix
```

## Development
```bash
npm run watch   # Auto-compile on changes
# Press F5 in VS Code to launch Extension Development Host
```
