import * as vscode from 'vscode';
import { UI_ATTRIBUTES, CSS_PROPERTIES, CSS_PSEUDO_CLASSES, CSS_FUNCTIONS, DOM_ELEMENTS } from './completionProvider';

// ---- Hover Documentation Database ----

const HOVER_DOCS: Record<string, string> = {
    // HTML Tags
    'ui': '### `<ui>` — Root Window Element\n\nThe master root node of a `.dom` file. Maps directly to a Win32 window.\n\n**Attributes:** `title`, `frameless`, `resizable`, `maximizable`, `transparent`, `system-shadow`, `centered`, `position`, `position-x`, `position-y`, `bg`, `width`, `height`, `id`, `onLoad`',
    'View': '### `<View>` — Container Element\n\nA generic container for grouping and laying out child elements. Supports `layout`, `gap`, `justify`, `align`.',
    'div': '### `<div>` — Container Element\n\nAlias for `<View>`. Generic container for layout.',
    'Text': '### `<Text>` — Text Display\n\nRenders text content. Set text via the `text` attribute.\n\n**Key props:** `text`, `color`, `font-size`, `font-family`, `font-weight`, `text-align`, `vertical-align`, `white-space`',
    'Button': '### `<Button>` — Interactive Button\n\nClickable button element. Usually a component imported from `Button.dom`.\n\n**Key props:** `onClick`, `bg`, `color`, `cursor`',
    'TextInput': '### `<TextInput>` — Text Input Field\n\nEditable text input component. Responds to keyboard events and manages focus state.\n\n**Key props:** `onKey`, cursor styles, `:focused` state',
    'Image': '### `<Image>` — Image Display\n\nRenders an image from a file path.\n\n**Key props:** `src` (file path or `"system:exe"` for app icon), `width`, `height`',
    'WebView': '### `<WebView>` — Embedded Browser\n\nEmbeds a Microsoft Edge WebView2 browser instance. Requires `webview_plugin.dll`.\n\n**Key props:** `url`, `width`, `height`\n\n**Methods:** `navigate()`, `executeScript()`, `postMessage()`, `onMessage()`, etc.',
    'import': '### `<import>` — Component Import\n\nImports an external `.dom` file as a reusable component.\n\n```xml\n<import src="components/Button.dom" as="Button" />\n```\n\nThe imported component becomes available as a custom tag `<Button ... />`.',
    'style': '### `<style>` — CSS-like Styling Block\n\nDefines CSS-like rules for styling elements. NativeDOM CSS supports:\n- Element, class, ID selectors\n- Pseudo-states: `:hover`, `:focused`, `:active`\n- Variables: `var(--name, default)`\n- Functions: `calc()`, `color-mix()`, `attr()`\n- `:host()` for component host queries',
    'script': '### `<script>` — JavaScript Block\n\nContains JavaScript code executed by NativeDOM\'s custom interpreter.\n\n**Note:** ES6 `class` syntax is NOT supported. Use IIFE + `self.method = function()` pattern for components.',
    'component': '### `<component>` — Inline Component Definition\n\nDefines an inline component (used in bundled output). Equivalent to an imported `.dom` file.',

    // CSS features
    'var': '### `var()` — CSS Variable Reference\n\nReferences a CSS custom property with optional fallback.\n\n```css\nbg: var(--theme-bg, #1E1E2E);\n```\n\nVariables cascade through the DOM tree (parent → child) and through shadow DOM boundaries.',
    'calc': '### `calc()` — Math Expression\n\nPerforms arithmetic on CSS values.\n\n```css\nwidth: calc(100% - 20);\nheight: calc(var(--base-height) * 2);\n```\n\nSupports `+`, `-`, `*`, `/` operators.',
    'color-mix': '### `color-mix()` — Color Blending\n\nInterpolates between two colors.\n\n```css\nbg: color-mix(#FF0000, #0000FF, 0.5);\n```\n\nThird parameter is mix ratio (0.0 = first color, 1.0 = second color).',
    'attr': '### `attr()` — Attribute Interpolation\n\nBinds an HTML attribute value into a CSS property.\n\n```css\ntext: attr(custom-property, default-value);\n```',

    // sys.* API
    'sys': '### `sys` — System API Object\n\nExposes OS-level APIs: logging, file I/O, process management, DLL calls, window control, screen info, keyboard hooks, and clipboard.',
    'sys.log': '### `sys.log(msg: string)`\n\nPrints to console (stdout), `OutputDebugStringA`, and appends to `sys_log.txt`.',
    'sys.time': '### `sys.time() → number`\n\nReturns microsecond-resolution system uptime via `GetTickCount64()`.',
    'sys.exec': '### `sys.exec(cmd: string, hidden?: bool) → number`\n\nForks a Windows subprocess via `CreateProcess`. Returns the Process ID.\n\n`hidden` flag runs process with `SW_HIDE`.',
    'sys.readText': '### `sys.readText(filepath: string) → string`\n\nBlocking file read, parsed as UTF-8.',
    'sys.writeText': '### `sys.writeText(filepath: string, data: string)`\n\nBlocking file write/overwrite.',
    'sys.loadScript': '### `sys.loadScript(filepath: string, asNewProcess?: bool)`\n\nHot-loads a `.dom` file. If `asNewProcess=true`, spawns isolated `dom.exe` child.',
    'sys.dllCall': '### `sys.dllCall(dll, func, args[]) → number`\n\nFFI bridge invoking arbitrary DLL functions. Returns numeric value.\n\n**Protected by SEH** — DLL crashes are caught gracefully.',
    'sys.dllCallString': '### `sys.dllCallString(dll, func, args[]) → string`\n\nLike `dllCall` but interprets return as `const char*` string pointer.',
    'sys.loadExtension': '### `sys.loadExtension(dllPath: string) → bool`\n\nLoads a C++ extension DLL. Probes for `NativeDOM_Init(void** ctxArgs)` export.\n\nThe extension receives: `[HWND, NVGcontext*, Interpreter*]`.',
    'sys.sendMessage': '### `sys.sendMessage(hwnd, msg, wParam, lParam)`\n\nRaw Win32 `SendMessage` IPC.',
    'sys.findWindow': '### `sys.findWindow(title: string) → number`\n\nFinds a window by title, returns HWND.',
    'sys.getHwnd': '### `sys.getHwnd() → number`\n\nReturns this application\'s HWND.',
    'sys.screenshot': '### `sys.screenshot(path?, x?, y?, w?, h?) → bool`\n\nCaptures the window to a BMP file via `BitBlt`.',
    'sys.getPixelColor': '### `sys.getPixelColor(x?, y?) → {r, g, b, hex, x, y}`\n\nReturns pixel color at coordinates (or cursor position).',
    'sys.com.create': '### `sys.com.create(progId: string) → object`\n\nInstantiates a new COM object (via `CoCreateInstance`). Returns a wrapped NativeDOM object with methods: `.call(method, ...args)`, `.get(prop, ...indexes)`, `.set(prop, value)`, and `.release()`.',
    'sys.com.getActive': '### `sys.com.getActive(progId: string) → object`\n\nGets an active, running COM object (via `GetActiveObject`).',
    'sys.com.releaseAll': '### `sys.com.releaseAll()`\n\nReleases all COM objects instantiated or retrieved by scripts. You should call this before exit to ensure proper cleanup.',

    // DOM API
    'document.querySelector': '### `document.querySelector(selector) → Element?`\n\nReturns first matching element in the light DOM.',
    'document.querySelectorAll': '### `document.querySelectorAll(selector) → Element[]`\n\nReturns all matching elements in the light DOM.',
    'document.getElementById': '### `document.getElementById(id) → Element?`\n\nFast ID lookup. Returns cached element wrapper (same object reference every call).',
    'document.createElement': '### `document.createElement(tag) → Element`\n\nCreates a detached element. Use `.appendChild()` to add to DOM.',
    'document.shadowDomSelector': '### `document.shadowDomSelector(selector) → Element?`\n\nBypasses shadow DOM encapsulation to query inside component trees.',
    'document.shadowDomSelectorAll': '### `document.shadowDomSelectorAll(selector) → Element[]`\n\nReturns all matching elements across shadow DOM boundaries.',

    // Element API
    'getAttribute': '### `el.getAttribute(key) → string`\n\nReturns the raw property value for the given key.',
    'setAttribute': '### `el.setAttribute(key, value)`\n\nSets or updates an element property. Triggers GUI repaint if visual.',
    'getBoundingRect': '### `el.getBoundingRect() → {x, y, width, height}`\n\nTriggers layout calculation and returns absolute screen bounds.',
    'appendChild': '### `el.appendChild(child)`\n\nAppends a child element. Triggers immediate GUI repaint.',
    'parentElement': '### `el.parentElement → Element?`\n\nReturns the immediate parent element or null at root.',
    'shadowDomSelector': '### `el.shadowDomSelector(selector) → Element?`\n\nQueries within this element\'s shadow tree.',
    'addEventListener': '### `el.addEventListener(type, callback)`\n\nRegisters an event listener for the specified event type.',
    'removeEventListener': '### `el.removeEventListener(type)`\n\nRemoves the event listener for the specified event type.',
    'dispatchEvent': '### `el.dispatchEvent(event)`\n\nManually dispatches an event on this element.',

    // Pseudo classes
    ':hover': '### `:hover` Pseudo-state\n\nActive when the mouse cursor is over the element.',
    ':focused': '### `:focused` Pseudo-state\n\nActive when the element has UI focus.\n\n**Note:** NativeDOM uses `:focused` (not `:focus`).',
    ':active': '### `:active` Pseudo-state\n\nActive while the element is being pressed/clicked.',
    ':host': '### `:host()` Component Selector\n\nAllows components to style based on attributes passed to their host element.\n\n```css\n:host([variant="primary"]) .btn-text {\n    color: #FFFFFF;\n}\n```',
};

export class DomHoverProvider implements vscode.HoverProvider {
    provideHover(
        document: vscode.TextDocument,
        position: vscode.Position,
        _token: vscode.CancellationToken
    ): vscode.Hover | null {
        const config = vscode.workspace.getConfiguration('nativedom');
        if (!config.get('enableHoverDocs', true)) return null;

        const range = document.getWordRangeAtPosition(position, /[\w.:$#-]+/);
        if (!range) return null;
        const word = document.getText(range);
        const lineText = document.lineAt(position).text;

        // Check extended patterns first (sys.window.center etc.)
        const lineBefore = lineText.substring(0, position.character + word.length);
        
        // Match sys.something.method patterns
        const fullSysMatch = /sys\.\w+\.\w+/.exec(lineBefore);
        if (fullSysMatch) {
            const fullKey = fullSysMatch[0];
            if (HOVER_DOCS[fullKey]) {
                return new vscode.Hover(new vscode.MarkdownString(HOVER_DOCS[fullKey]));
            }
        }

        // Match sys.method patterns
        const sysMatch = /sys\.\w+/.exec(lineBefore);
        if (sysMatch) {
            const sysKey = sysMatch[0];
            if (HOVER_DOCS[sysKey]) {
                return new vscode.Hover(new vscode.MarkdownString(HOVER_DOCS[sysKey]));
            }
        }

        // Match document.method patterns
        const docMatch = /document\.\w+/.exec(lineBefore);
        if (docMatch) {
            const docKey = docMatch[0];
            if (HOVER_DOCS[docKey]) {
                return new vscode.Hover(new vscode.MarkdownString(HOVER_DOCS[docKey]));
            }
        }

        // Direct word lookups
        if (HOVER_DOCS[word]) {
            return new vscode.Hover(new vscode.MarkdownString(HOVER_DOCS[word]));
        }

        // Check CSS properties
        if (CSS_PROPERTIES[word]) {
            const info = CSS_PROPERTIES[word];
            let md = `### \`${word}\`\n\n${info.desc}`;
            if (info.values) {
                md += `\n\n**Values:** ${info.values.map(v => `\`${v}\``).join(', ')}`;
            }
            return new vscode.Hover(new vscode.MarkdownString(md));
        }

        // Check UI attributes
        if (UI_ATTRIBUTES[word]) {
            const info = UI_ATTRIBUTES[word];
            let md = `### \`${word}\` attribute\n\n${info.desc}`;
            if (info.values) {
                md += `\n\n**Values:** ${info.values.map(v => `\`${v}\``).join(', ')}`;
            }
            return new vscode.Hover(new vscode.MarkdownString(md));
        }

        // Check pseudo classes
        if (word.startsWith(':')) {
            const pseudoKey = word;
            if (HOVER_DOCS[pseudoKey]) {
                return new vscode.Hover(new vscode.MarkdownString(HOVER_DOCS[pseudoKey]));
            }
        }

        // Check element methods
        if (HOVER_DOCS[word]) {
            return new vscode.Hover(new vscode.MarkdownString(HOVER_DOCS[word]));
        }

        // Hex color preview
        const colorMatch = /#(?:[0-9a-fA-F]{8}|[0-9a-fA-F]{6}|[0-9a-fA-F]{3})/.exec(word);
        if (colorMatch) {
            return new vscode.Hover(new vscode.MarkdownString(`**Color:** \`${colorMatch[0]}\``));
        }

        return null;
    }
}
