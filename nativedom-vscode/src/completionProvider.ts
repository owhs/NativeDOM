import * as vscode from 'vscode';
import { extractMethodsFromDocument, inferVariableType, extractCssVariables } from './parserUtil';
import { 
    sysMembers, windowMembers, screenMembers, keyboardMembers, 
    clipboardMembers, comMembers, trayMembers, mathMembers, jsonMembers, 
    elementInstanceMethods, comInstanceMethods 
} from './apiDefinitions';

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

        // Robust tag counting ignoring attributes
        const styleOpens = (text.match(/<style[^>]*>/gi) || []).length;
        const styleCloses = (text.match(/<\/style>/gi) || []).length;
        if (styleOpens > styleCloses) return 'css';

        const scriptOpens = (text.match(/<script[^>]*>/gi) || []).length;
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

            // CSS variable declarations dynamically scraped
            const cssVars = extractCssVariables(document.getText());
            for (const v of cssVars) {
                const item = new vscode.CompletionItem(v, vscode.CompletionItemKind.Variable);
                items.push(item);
            }

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
        try {
            const items: vscode.CompletionItem[] = [];

        // Property access completion (e.g. sys.window., host., shell., e.target.)
        const propMatch = /(?:([a-zA-Z0-9_\.]+)\.)([a-zA-Z0-9_]*)$/.exec(textBefore);
        if (propMatch) {
            const objPrefix = propMatch[1]; // e.g. "sys", "sys.window", "document", "host"

            if (objPrefix === 'sys') {
                for (const m of sysMembers) {
                    const item = new vscode.CompletionItem(m.name, vscode.CompletionItemKind.Method);
                    item.documentation = new vscode.MarkdownString(`**${m.sig}**\n\n${m.desc}`);
                    item.detail = m.sig;
                    items.push(item);
                }
                return items;
            }

            if (objPrefix === 'Math') {
                for (const m of mathMembers) items.push(new vscode.CompletionItem(m.name, vscode.CompletionItemKind.Method));
                return items;
            }

            if (objPrefix === 'JSON') {
                for (const m of jsonMembers) items.push(new vscode.CompletionItem(m.name, vscode.CompletionItemKind.Method));
                return items;
            }

            if (objPrefix === 'sys.window') {
                for (const m of windowMembers) items.push(new vscode.CompletionItem(m.name, vscode.CompletionItemKind.Method));
                return items;
            }

            if (objPrefix === 'sys.screen') {
                for (const m of screenMembers) items.push(new vscode.CompletionItem(m.name, vscode.CompletionItemKind.Method));
                return items;
            }
            if (objPrefix === 'sys.keyboard') {
                for (const m of keyboardMembers) items.push(new vscode.CompletionItem(m.name, vscode.CompletionItemKind.Method));
                return items;
            }
            if (objPrefix === 'sys.clipboard') {
                for (const m of clipboardMembers) items.push(new vscode.CompletionItem(m.name, vscode.CompletionItemKind.Method));
                return items;
            }
            if (objPrefix === 'sys.com') {
                for (const m of comMembers) items.push(new vscode.CompletionItem(m.name, vscode.CompletionItemKind.Method));
                return items;
            }
            if (objPrefix === 'sys.tray') {
                for (const m of trayMembers) items.push(new vscode.CompletionItem(m.name, vscode.CompletionItemKind.Method));
                return items;
            }
            if (objPrefix === 'document') {
                for (const m of ['querySelector', 'querySelectorAll', 'getElementById', 'createElement', 'addEventListener', 'removeEventListener', 'dispatchEvent', 'shadowDomSelector', 'shadowDomSelectorAll']) items.push(new vscode.CompletionItem(m, vscode.CompletionItemKind.Method));
                return items;
            }
            if (objPrefix === 'self') {
                const methods = extractMethodsFromDocument(document.getText(), document.uri);
                for (const m of methods) {
                    const item = new vscode.CompletionItem(m.name, vscode.CompletionItemKind.Method);
                    let doc = `**${m.name}(${m.args.map(a => a.name).join(', ')})**\n`;
                    for (const arg of m.args) if (arg.comment) doc += `\n* \`${arg.name}\`: ${arg.comment}`;
                    item.documentation = new vscode.MarkdownString(doc);
                    item.detail = `${m.name}(${m.args.map(a => a.name).join(', ')})`;
                    item.insertText = new vscode.SnippetString(`${m.name}(${m.args.map((a, idx) => `\${${idx + 1}:${a.name}}`).join(', ')})`);
                    items.push(item);
                }
                return items;
            }

            // Fallback object property completion
            const typeInfo = inferVariableType(objPrefix, document.getText(), document.uri);
            const { type, componentPath } = typeInfo;

            // If the element was dynamically traced to an imported component, extract its custom JS API!
            if (componentPath) {
                try {
                    const fs = require('fs');
                    const compDocText = fs.readFileSync(componentPath, 'utf-8');
                    const compMethods = extractMethodsFromDocument(compDocText, vscode.Uri.file(componentPath));
                    for (const m of compMethods) {
                        const item = new vscode.CompletionItem(m.name, vscode.CompletionItemKind.Method);
                        let doc = `**${m.name}(${m.args.map(a => a.name).join(', ')})**\n\n*(Exported by <${objPrefix}> component)*`;
                        for (const arg of m.args) if (arg.comment) doc += `\n* \`${arg.name}\`: ${arg.comment}`;
                        
                        item.documentation = new vscode.MarkdownString(doc);
                        item.detail = `${m.name}(${m.args.map(a => a.name).join(', ')})`;
                        item.insertText = new vscode.SnippetString(`${m.name}(${m.args.map((a, idx) => `\${${idx + 1}:${a.name}}`).join(', ')})`);
                        item.sortText = '0000_' + m.name; // Force to the top of standard DOM functions!
                        items.push(item);
                    }
                } catch (e) {
                    // Ignore fs errors silently
                }
            }

            if (type === 'Element' || (type === 'Unknown' && !objPrefix.includes('sys'))) {
                for (const m of elementInstanceMethods) items.push(new vscode.CompletionItem(m.name, vscode.CompletionItemKind.Method));
            }

            if (type === 'COM' || (type === 'Unknown' && !objPrefix.includes('sys'))) {
                for (const m of comInstanceMethods) items.push(new vscode.CompletionItem(m.name, vscode.CompletionItemKind.Method));
            }

            return items;
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
            'sys', 'document', 'console', 'window', 'self',
        ];

        // Also inject ANY function defined in the active document via inline scrape
        const methods = extractMethodsFromDocument(document.getText(), document.uri);
            for (const m of methods) {
                const item = new vscode.CompletionItem(m.name, vscode.CompletionItemKind.Function);
                let doc = `**${m.name}(${m.args.map(a => a.name).join(', ')})**\n`;
                for (const arg of m.args) {
                    if (arg.comment) doc += `\n* \`${arg.name}\`: ${arg.comment}`;
                }
                item.documentation = new vscode.MarkdownString(doc);
                item.detail = `${m.name}(${m.args.map(a => a.name).join(', ')}) [Local Function]`;
                item.insertText = new vscode.SnippetString(`${m.name}(${m.args.map((a, idx) => `\${${idx + 1}:${a.name}}`).join(', ')})`);
                items.push(item);
            }
        
            for (const kw of jsKeywords) {
                const item = new vscode.CompletionItem(kw, vscode.CompletionItemKind.Keyword);
                items.push(item);
            }

        return items;
        } catch (e) {
            return [new vscode.CompletionItem('Error: ' + String(e), vscode.CompletionItemKind.Issue)];
        }
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
