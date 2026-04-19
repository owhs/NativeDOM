import * as vscode from 'vscode';

// ---- NativeDOM Knowledge Base ----
// All the element tags, attributes, CSS properties, and JS APIs

export const DOM_ELEMENTS = [
    'ui', 'View', 'div', 'Text', 'Button', 'TextInput', 'Image', 'WebView',
];

export const UI_ATTRIBUTES: Record<string, { desc: string; values?: string[] }> = {
    'id':             { desc: 'Unique element identifier' },
    'class':          { desc: 'CSS class name(s), space-separated' },
    'text':           { desc: 'Inner text content of the element' },
    'title':          { desc: 'Window title (on <ui>) or tooltip text' },
    'width':          { desc: 'Element width (pixels or percentage like "100%")', values: ['100%', '50%', 'auto'] },
    'height':         { desc: 'Element height (pixels or percentage like "100%")', values: ['100%', '50%', 'auto'] },
    'x':              { desc: 'X position offset from parent origin' },
    'y':              { desc: 'Y position offset from parent origin' },
    'right':          { desc: 'Position from right edge of parent' },
    'bottom':         { desc: 'Position from bottom edge of parent' },
    'bg':             { desc: 'Background color (hex: #RRGGBB or #RRGGBBAA)' },
    'color':          { desc: 'Text/foreground color (hex)' },
    'font-size':      { desc: 'Font size in pixels' },
    'font-family':    { desc: 'Font family name(s), comma-separated', values: ['Segoe UI', 'Segoe Fluent Icons', 'Inter', 'Roboto'] },
    'font-weight':    { desc: 'Font weight', values: ['normal', 'bold'] },
    'border':         { desc: 'Border color (hex)' },
    'border-width':   { desc: 'Border stroke width in pixels' },
    'border-radius':  { desc: 'Corner radius in pixels' },
    'padding':        { desc: 'Uniform padding on all sides (pixels)' },
    'padding-left':   { desc: 'Left padding in pixels' },
    'padding-right':  { desc: 'Right padding in pixels' },
    'padding-top':    { desc: 'Top padding in pixels' },
    'padding-bottom': { desc: 'Bottom padding in pixels' },
    'opacity':        { desc: 'Element opacity (0.0 to 1.0)' },
    'cursor':         { desc: 'Cursor type when hovering', values: ['pointer', 'text', 'default'] },
    'display':        { desc: 'Display mode', values: ['none'] },
    'overflow':       { desc: 'Overflow clipping behavior', values: ['visible', 'hidden'] },
    'z-index':        { desc: 'Stacking order (higher = on top)' },
    'layout':         { desc: 'Layout mode for children', values: ['absolute', 'row', 'col'] },
    'gap':            { desc: 'Gap between children in row/col layout (pixels)' },
    'justify':        { desc: 'Main-axis alignment in row/col', values: ['start', 'center', 'end', 'space-between'] },
    'align':          { desc: 'Cross-axis alignment in row/col', values: ['start', 'center', 'end'] },
    'text-align':     { desc: 'Horizontal text alignment', values: ['left', 'center', 'right'] },
    'vertical-align': { desc: 'Vertical text alignment', values: ['top', 'center', 'bottom'] },
    'white-space':    { desc: 'Text wrapping mode', values: ['normal', 'nowrap'] },
    'transition':     { desc: 'CSS transition (e.g. "bg .2s ease")' },
    'pointer-events': { desc: 'Event interaction mode', values: ['catch', 'pass', 'none', 'block', 'catchAndPass'] },
    'events':         { desc: 'Event interaction mode (alias for pointer-events)', values: ['catch', 'pass', 'none', 'block', 'catchAndPass'] },
    'drag':           { desc: 'Enable window dragging on this element', values: ['true', 'false'] },
    'tabindex':       { desc: 'Tab navigation order index' },
    'src':            { desc: 'Image source path or "system:exe"', values: ['system:exe'] },
    'min-width':      { desc: 'Minimum width in pixels' },
    'max-width':      { desc: 'Maximum width in pixels' },
    'min-height':     { desc: 'Minimum height in pixels' },
    'max-height':     { desc: 'Maximum height in pixels' },
    // <ui> specific
    'frameless':      { desc: 'Strip WS_CAPTION and borders for frameless window', values: ['true', 'false'] },
    'resizable':      { desc: 'Allow window edge resizing', values: ['true', 'false'] },
    'maximizable':    { desc: 'Allow maximize button', values: ['true', 'false'] },
    'transparent':    { desc: 'Enable WS_EX_LAYERED for transparent window', values: ['true', 'false'] },
    'system-shadow':  { desc: 'Force or suppress Windows 11 DWM drop-shadow', values: ['true', 'false'] },
    'centered':       { desc: 'Center window on screen at startup', values: ['true'] },
    'position':       { desc: 'Window startup position', values: ['center'] },
    'position-x':     { desc: 'Explicit X startup coordinate' },
    'position-y':     { desc: 'Explicit Y startup coordinate' },
    // Event handlers
    'onLoad':         { desc: 'Inline JS handler called when element loads' },
    'onClick':        { desc: 'Inline JS handler called on click' },
    'onKey':          { desc: 'Inline JS handler called on key press' },
    // Component
    'url':            { desc: 'Initial URL for WebView component' },
};

export const CSS_PROPERTIES: Record<string, { desc: string; values?: string[] }> = {
    'bg':               { desc: 'Background color', values: ['#1E1E2E', '#181825', '#11111B', 'transparent', '#00000000'] },
    'color':            { desc: 'Text/foreground color', values: ['#CDD6F4', '#A6ADC8', '#6C7086', '#11111B'] },
    'width':            { desc: 'Width (pixels or percentage)' },
    'height':           { desc: 'Height (pixels or percentage)' },
    'x':                { desc: 'X position offset' },
    'y':                { desc: 'Y position offset' },
    'right':            { desc: 'Right-edge anchor position' },
    'bottom':           { desc: 'Bottom-edge anchor position' },
    'min-width':        { desc: 'Minimum width constraint' },
    'max-width':        { desc: 'Maximum width constraint' },
    'min-height':       { desc: 'Minimum height constraint' },
    'max-height':       { desc: 'Maximum height constraint' },
    'border':           { desc: 'Border color (hex)' },
    'border-width':     { desc: 'Border stroke width' },
    'border-radius':    { desc: 'Corner radius' },
    'font-size':        { desc: 'Font size in pixels' },
    'font-family':      { desc: 'Font family names, comma-separated' },
    'font-weight':      { desc: 'Font weight', values: ['normal', 'bold'] },
    'text-align':       { desc: 'Horizontal text alignment', values: ['left', 'center', 'right'] },
    'vertical-align':   { desc: 'Vertical text alignment', values: ['top', 'center', 'bottom'] },
    'padding':          { desc: 'Uniform padding' },
    'padding-left':     { desc: 'Left padding' },
    'padding-right':    { desc: 'Right padding' },
    'padding-top':      { desc: 'Top padding' },
    'padding-bottom':   { desc: 'Bottom padding' },
    'opacity':          { desc: 'Element opacity (0.0-1.0)' },
    'cursor':           { desc: 'Cursor type', values: ['pointer', 'text', 'default'] },
    'display':          { desc: 'Display mode', values: ['none'] },
    'overflow':         { desc: 'Overflow clipping', values: ['visible', 'hidden'] },
    'z-index':          { desc: 'Stacking order' },
    'layout':           { desc: 'Child layout mode', values: ['absolute', 'row', 'col'] },
    'gap':              { desc: 'Gap between layout children' },
    'justify':          { desc: 'Main-axis alignment', values: ['start', 'center', 'end', 'right', 'bottom', 'space-between'] },
    'align':            { desc: 'Cross-axis alignment', values: ['start', 'center', 'end', 'right', 'bottom'] },
    'white-space':      { desc: 'Text wrapping', values: ['normal', 'nowrap'] },
    'transition':       { desc: 'Animated transitions (e.g. "bg .2s ease, width .2s ease")' },
    'pointer-events':   { desc: 'Event interaction', values: ['catch', 'pass', 'none', 'block', 'catchAndPass'] },
    'events':           { desc: 'Event interaction (alias)', values: ['catch', 'pass', 'none', 'block', 'catchAndPass'] },
    'drag':             { desc: 'Enable window dragging', values: ['true', 'false'] },
};

export const CSS_PSEUDO_CLASSES = [
    'hover', 'focused', 'active', 'first-child', 'last-child', 'nth-child', 'not', 'has',
];

export const CSS_FUNCTIONS = [
    { name: 'var', desc: 'Reference a CSS variable: var(--name, default)' },
    { name: 'calc', desc: 'Math expression: calc(100% - 20)' },
    { name: 'color-mix', desc: 'Blend two colors: color-mix(#color1, #color2, 0.5)' },
    { name: 'attr', desc: 'Use HTML attribute value: attr(propertyName, default)' },
];

export const EVENT_TYPES = [
    'click', 'dblclick', 'mousedown', 'mouseup', 'mousemove',
    'mouseenter', 'mouseleave', 'contextmenu', 'wheel',
    'keydown', 'keypress', 'keyup',
    'focus', 'blur', 'load', 'resize', 'close', 'drop',
    'hotkey', 'customMessage', 'webview_ready', 'webview_message',
];

// ---- Completion Provider ----
export class DomCompletionProvider implements vscode.CompletionItemProvider {
    provideCompletionItems(
        document: vscode.TextDocument,
        position: vscode.Position,
        _token: vscode.CancellationToken,
        context: vscode.CompletionContext
    ): vscode.CompletionItem[] {
        const lineText = document.lineAt(position).text;
        const textBefore = lineText.substring(0, position.character);
        const fullText = document.getText();
        
        const zone = this.getZone(document, position);

        const items: vscode.CompletionItem[] = [];

        if (zone === 'html') {
            return this.getHtmlCompletions(textBefore, fullText, document, position);
        } else if (zone === 'css') {
            return this.getCssCompletions(textBefore, document, position);
        } else if (zone === 'js') {
            return this.getJsCompletions(textBefore, document, position);
        }

        return items;
    }

    private getZone(document: vscode.TextDocument, position: vscode.Position): 'html' | 'css' | 'js' {
        const text = document.getText(new vscode.Range(new vscode.Position(0, 0), position));

        // Count open/close style and script tags
        const styleOpens = (text.match(/<style>/gi) || []).length;
        const styleCloses = (text.match(/<\/style>/gi) || []).length;
        if (styleOpens > styleCloses) return 'css';

        const scriptOpens = (text.match(/<script>/gi) || []).length;
        const scriptCloses = (text.match(/<\/script>/gi) || []).length;
        if (scriptOpens > scriptCloses) return 'js';

        return 'html';
    }

    private getHtmlCompletions(
        textBefore: string,
        fullText: string,
        document: vscode.TextDocument,
        position: vscode.Position
    ): vscode.CompletionItem[] {
        const items: vscode.CompletionItem[] = [];

        // Detect imported component names
        const importedComponents = this.getImportedComponents(fullText);

        // Inside a tag — suggest attributes
        const insideTag = /<(\w[\w-]*)\s[^>]*$/.exec(textBefore);
        if (insideTag) {
            const tagName = insideTag[1];
            for (const [attr, info] of Object.entries(UI_ATTRIBUTES)) {
                const item = new vscode.CompletionItem(attr, vscode.CompletionItemKind.Property);
                item.documentation = new vscode.MarkdownString(info.desc);
                if (info.values) {
                    item.insertText = new vscode.SnippetString(`${attr}="\${1|${info.values.join(',')}|}"`);
                } else {
                    item.insertText = new vscode.SnippetString(`${attr}="$1"`);
                }
                items.push(item);
            }

            // Pseudo-state inline attributes
            for (const pseudo of CSS_PSEUDO_CLASSES) {
                if (pseudo === 'first-child' || pseudo === 'last-child' || pseudo === 'nth-child' || pseudo === 'not' || pseudo === 'has') continue;
                for (const [prop] of Object.entries(CSS_PROPERTIES)) {
                    const item = new vscode.CompletionItem(
                        `${pseudo}:${prop}`, vscode.CompletionItemKind.Property
                    );
                    item.documentation = `Inline ${pseudo} state for ${prop}`;
                    item.insertText = new vscode.SnippetString(`${pseudo}:${prop}="$1"`);
                    item.sortText = `z${pseudo}:${prop}`;
                    items.push(item);
                }
            }

            return items;
        }

        // After < — suggest element names
        if (textBefore.endsWith('<') || /^<\s*$/.test(textBefore.trim())) {
            for (const tag of DOM_ELEMENTS) {
                const item = new vscode.CompletionItem(tag, vscode.CompletionItemKind.Class);
                item.documentation = `NativeDOM <${tag}> element`;
                item.insertText = new vscode.SnippetString(`${tag} $1>$0</${tag}>`);
                items.push(item);
            }

            // Add imported components
            for (const comp of importedComponents) {
                const item = new vscode.CompletionItem(comp, vscode.CompletionItemKind.Module);
                item.documentation = `Imported component <${comp}>`;
                item.insertText = new vscode.SnippetString(`${comp} id="$1" $2/>`);
                items.push(item);
            }

            // Special block tags
            for (const block of ['style', 'script', 'import']) {
                const item = new vscode.CompletionItem(block, vscode.CompletionItemKind.Keyword);
                if (block === 'import') {
                    item.insertText = new vscode.SnippetString(`import src="$1" as="$2" />`);
                } else {
                    item.insertText = new vscode.SnippetString(`${block}>\n$0\n</${block}>`);
                }
                items.push(item);
            }
        }

        return items;
    }

    private getCssCompletions(
        textBefore: string,
        document: vscode.TextDocument,
        position: vscode.Position
    ): vscode.CompletionItem[] {
        const items: vscode.CompletionItem[] = [];

        // Inside declaration block (after {, before })
        const inBlock = /\{\s*([^}]*)$/.test(
            document.getText(new vscode.Range(new vscode.Position(0, 0), position))
        );

        if (inBlock) {
            // After : — suggest values
            const afterColon = /:\s*([^;]*)$/.exec(textBefore);
            if (afterColon) {
                // Find which property we're completing values for
                const propMatch = /\b([\w-]+)\s*:\s*[^;]*$/.exec(textBefore);
                const propName = propMatch?.[1] || '';
                const propInfo = CSS_PROPERTIES[propName];

                if (propInfo?.values) {
                    for (const v of propInfo.values) {
                        const item = new vscode.CompletionItem(v, vscode.CompletionItemKind.Value);
                        items.push(item);
                    }
                }

                // CSS functions
                for (const fn of CSS_FUNCTIONS) {
                    const item = new vscode.CompletionItem(fn.name, vscode.CompletionItemKind.Function);
                    item.documentation = fn.desc;
                    item.insertText = new vscode.SnippetString(`${fn.name}($1)`);
                    items.push(item);
                }

                // Common color values
                const colors = ['#1E1E2E', '#181825', '#11111B', '#CDD6F4', '#A6ADC8', '#89B4FA',
                    '#F38BA8', '#A6E3A1', '#F9E2AF', '#CBA6F7', '#B4BEFE', '#74C7EC', '#313244',
                    '#585B70', '#6C7086', 'transparent', '#00000000', '#FFFFFF'];
                for (const c of colors) {
                    const item = new vscode.CompletionItem(c, vscode.CompletionItemKind.Color);
                    item.sortText = `zz${c}`;
                    items.push(item);
                }

                return items;
            }

            // Property names
            for (const [prop, info] of Object.entries(CSS_PROPERTIES)) {
                const item = new vscode.CompletionItem(prop, vscode.CompletionItemKind.Property);
                item.documentation = info.desc;
                item.insertText = new vscode.SnippetString(`${prop}: $1;`);
                items.push(item);
            }

            // CSS variable declarations
            const varItem = new vscode.CompletionItem('--', vscode.CompletionItemKind.Variable);
            varItem.insertText = new vscode.SnippetString('--${1:var-name}: ${2:value};');
            varItem.documentation = 'Declare a CSS custom property (variable)';
            items.push(varItem);

            return items;
        }

        // Selector context
        for (const tag of DOM_ELEMENTS) {
            const item = new vscode.CompletionItem(tag, vscode.CompletionItemKind.Class);
            item.documentation = `NativeDOM <${tag}> element selector`;
            items.push(item);
        }

        // Pseudo classes
        if (textBefore.endsWith(':')) {
            for (const pseudo of CSS_PSEUDO_CLASSES) {
                const item = new vscode.CompletionItem(pseudo, vscode.CompletionItemKind.Enum);
                item.documentation = `Pseudo-class :${pseudo}`;
                items.push(item);
            }
        }

        // :host() pattern
        const hostItem = new vscode.CompletionItem(':host', vscode.CompletionItemKind.Keyword);
        hostItem.insertText = new vscode.SnippetString(':host([${1:attr}="${2:value}"]) ${3:selector}');
        hostItem.documentation = 'Host conditional selector for components';
        items.push(hostItem);

        return items;
    }

    private getJsCompletions(
        textBefore: string,
        document: vscode.TextDocument,
        position: vscode.Position
    ): vscode.CompletionItem[] {
        const items: vscode.CompletionItem[] = [];

        // sys.* completions
        if (textBefore.endsWith('sys.')) {
            const sysMembers = [
                { name: 'log', desc: 'Print to console, OutputDebugString, and sys_log.txt', sig: 'log(msg: string)' },
                { name: 'time', desc: 'Microsecond system uptime (GetTickCount64)', sig: 'time() → number' },
                { name: 'exec', desc: 'Fork a Windows sub-process', sig: 'exec(cmd: string, hidden?: bool) → number' },
                { name: 'readText', desc: 'Blocking file read (UTF-8)', sig: 'readText(path: string) → string' },
                { name: 'writeText', desc: 'Blocking file write', sig: 'writeText(path: string, data: string)' },
                { name: 'loadScript', desc: 'Hot-load a .dom file', sig: 'loadScript(path: string, asNewProcess?: bool)' },
                { name: 'dllCall', desc: 'FFI call returning number', sig: 'dllCall(dll, func, args[]) → number' },
                { name: 'dllCallString', desc: 'FFI call returning string', sig: 'dllCallString(dll, func, args[]) → string' },
                { name: 'loadExtension', desc: 'Load a C++ NativeDOM extension DLL', sig: 'loadExtension(dllPath: string) → bool' },
                { name: 'sendMessage', desc: 'Raw Win32 SendMessage', sig: 'sendMessage(hwnd, msg, wParam, lParam)' },
                { name: 'findWindow', desc: 'Find window by title', sig: 'findWindow(title: string) → number' },
                { name: 'getHwnd', desc: 'Get app HWND', sig: 'getHwnd() → number' },
                { name: 'screenshot', desc: 'Capture window to BMP', sig: 'screenshot(path?, x?, y?, w?, h?) → bool' },
                { name: 'getPixelColor', desc: 'Get pixel color at coords', sig: 'getPixelColor(x?, y?) → {r, g, b, hex, x, y}' },
                { name: 'setLogEnabled', desc: 'Toggle logging', sig: 'setLogEnabled(enable: bool)' },
                { name: 'window', desc: 'Window management APIs', sig: 'sys.window.*' },
                { name: 'screen', desc: 'Screen information APIs', sig: 'sys.screen.*' },
                { name: 'keyboard', desc: 'Keyboard hook APIs', sig: 'sys.keyboard.*' },
                { name: 'clipboard', desc: 'Clipboard APIs', sig: 'sys.clipboard.*' },
                { name: 'com', desc: 'COM/OLE Automation APIs', sig: 'sys.com.*' },
            ];
            for (const m of sysMembers) {
                const item = new vscode.CompletionItem(m.name, vscode.CompletionItemKind.Method);
                item.documentation = new vscode.MarkdownString(`**${m.sig}**\n\n${m.desc}`);
                item.detail = m.sig;
                items.push(item);
            }
            return items;
        }

        // sys.window.* completions
        if (textBefore.endsWith('sys.window.')) {
            const windowMembers = [
                { name: 'resize', sig: 'resize(w, h)' },
                { name: 'move', sig: 'move(x, y)' },
                { name: 'center', sig: 'center()' },
                { name: 'hide', sig: 'hide()' },
                { name: 'show', sig: 'show()' },
                { name: 'minimize', sig: 'minimize()' },
                { name: 'maximize', sig: 'maximize()' },
                { name: 'restore', sig: 'restore()' },
                { name: 'close', sig: 'close()' },
                { name: 'setFullscreen', sig: 'setFullscreen(enable: bool)' },
                { name: 'isMaximized', sig: 'isMaximized() → bool' },
                { name: 'setTitle', sig: 'setTitle(title: string)' },
                { name: 'setOpacity', sig: 'setOpacity(alpha: 0.0-1.0)' },
                { name: 'setAlwaysOnTop', sig: 'setAlwaysOnTop(enable: bool)' },
                { name: 'setIcon', sig: 'setIcon(filepath: string)' },
                { name: 'getSize', sig: 'getSize() → {width, height}' },
                { name: 'getPosition', sig: 'getPosition() → {x, y}' },
                { name: 'getActiveProcessName', sig: 'getActiveProcessName() → string' },
                { name: 'registerHotkey', sig: 'registerHotkey(id, keyStr)' },
                { name: 'unregisterHotkey', sig: 'unregisterHotkey(id)' },
            ];
            for (const m of windowMembers) {
                const item = new vscode.CompletionItem(m.name, vscode.CompletionItemKind.Method);
                item.detail = m.sig;
                items.push(item);
            }
            return items;
        }

        // sys.screen.* completions
        if (textBefore.endsWith('sys.screen.')) {
            for (const m of [
                { name: 'getInfo', sig: 'getInfo() → {width, height, monitors, workArea}' },
                { name: 'getDPI', sig: 'getDPI() → number' },
                { name: 'getMousePosition', sig: 'getMousePosition() → {x, y}' },
                { name: 'getMonitorAt', sig: 'getMonitorAt(x, y) → {name, x, y, width, height, isPrimary}' },
            ]) {
                const item = new vscode.CompletionItem(m.name, vscode.CompletionItemKind.Method);
                item.detail = m.sig;
                items.push(item);
            }
            return items;
        }

        // sys.keyboard.* completions
        if (textBefore.endsWith('sys.keyboard.')) {
            for (const m of [
                { name: 'hook', sig: 'hook(callback: Function)' },
                { name: 'unhook', sig: 'unhook()' },
            ]) {
                const item = new vscode.CompletionItem(m.name, vscode.CompletionItemKind.Method);
                item.detail = m.sig;
                items.push(item);
            }
            return items;
        }

        // sys.clipboard.* completions
        if (textBefore.endsWith('sys.clipboard.')) {
            for (const m of [
                { name: 'getText', sig: 'getText() → string' },
                { name: 'setText', sig: 'setText(payload: string)' },
                { name: 'clear', sig: 'clear()' },
                { name: 'getFormat', sig: 'getFormat(id) → number[]' },
                { name: 'setFormat', sig: 'setFormat(id, buffer)' },
            ]) {
                const item = new vscode.CompletionItem(m.name, vscode.CompletionItemKind.Method);
                item.detail = m.sig;
                items.push(item);
            }
            return items;
        }

        // sys.com.* completions
        if (textBefore.endsWith('sys.com.')) {
            for (const m of [
                { name: 'create', sig: 'create(progId) → object' },
                { name: 'getActive', sig: 'getActive(progId) → object' },
                { name: 'releaseAll', sig: 'releaseAll()' },
            ]) {
                const item = new vscode.CompletionItem(m.name, vscode.CompletionItemKind.Method);
                item.detail = m.sig;
                items.push(item);
            }
            return items;
        }

        // document.* completions
        if (textBefore.endsWith('document.')) {
            for (const m of [
                { name: 'querySelector', sig: 'querySelector(selector) → Element?' },
                { name: 'querySelectorAll', sig: 'querySelectorAll(selector) → Element[]' },
                { name: 'getElementById', sig: 'getElementById(id) → Element?' },
                { name: 'createElement', sig: 'createElement(tag) → Element' },
                { name: 'addEventListener', sig: 'addEventListener(type, callback)' },
                { name: 'removeEventListener', sig: 'removeEventListener(type)' },
                { name: 'dispatchEvent', sig: 'dispatchEvent(event)' },
                { name: 'shadowDomSelector', sig: 'shadowDomSelector(selector) → Element?' },
                { name: 'shadowDomSelectorAll', sig: 'shadowDomSelectorAll(selector) → Element[]' },
            ]) {
                const item = new vscode.CompletionItem(m.name, vscode.CompletionItemKind.Method);
                item.detail = m.sig;
                items.push(item);
            }
            return items;
        }

        // addEventListener event types
        const eventMatch = /addEventListener\(\s*["']$/.exec(textBefore);
        if (eventMatch) {
            for (const evt of EVENT_TYPES) {
                const item = new vscode.CompletionItem(evt, vscode.CompletionItemKind.Event);
                items.push(item);
            }
            return items;
        }

        // Element method completions after .
        if (textBefore.endsWith('.')) {
            const elementMethods = [
                'querySelector', 'querySelectorAll', 'shadowDomSelector', 'shadowDomSelectorAll',
                'getAttribute', 'setAttribute', 'classList', 'appendChild',
                'getBoundingRect', 'addEventListener', 'removeEventListener', 'dispatchEvent',
                'show', 'hide', 'focus', 'blur', 'parentElement', 'children',
                'textContent', 'innerText',
            ];
            for (const m of elementMethods) {
                const item = new vscode.CompletionItem(m, vscode.CompletionItemKind.Method);
                items.push(item);
            }
        }

        // General JS keywords and builtins
        const jsKeywords = [
            'var', 'let', 'const', 'function', 'if', 'else', 'for', 'while', 'do',
            'switch', 'case', 'break', 'continue', 'return', 'new', 'typeof', 'instanceof',
            'true', 'false', 'null', 'undefined',
            'setTimeout', 'setInterval', 'clearTimeout', 'clearInterval',
            'fetch', 'JSON', 'Math', 'Object', 'Array', 'String', 'Number',
            'parseInt', 'parseFloat', 'isNaN', 'isFinite',
            'Promise', 'CustomEvent',
            'sys', 'document', 'console', 'window',
        ];
        if (!textBefore.endsWith('.')) {
            for (const kw of jsKeywords) {
                const item = new vscode.CompletionItem(kw, vscode.CompletionItemKind.Keyword);
                items.push(item);
            }
        }

        return items;
    }

    private getImportedComponents(fullText: string): string[] {
        const components: string[] = [];
        const importRegex = /<import\s+[^>]*as="(\w+)"[^>]*\/?\s*>/g;
        let match;
        while ((match = importRegex.exec(fullText)) !== null) {
            components.push(match[1]);
        }
        return components;
    }
}
