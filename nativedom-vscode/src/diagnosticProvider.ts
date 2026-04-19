import * as vscode from 'vscode';

export class DomDiagnosticProvider {
    private diagnosticCollection: vscode.DiagnosticCollection;
    private debounceTimer: NodeJS.Timeout | undefined;

    constructor(diagnosticCollection: vscode.DiagnosticCollection) {
        this.diagnosticCollection = diagnosticCollection;
    }

    updateDiagnostics(document: vscode.TextDocument): void {
        const config = vscode.workspace.getConfiguration('nativedom');
        if (!config.get('enableDiagnostics', true)) {
            this.diagnosticCollection.delete(document.uri);
            return;
        }

        // Debounce
        if (this.debounceTimer) clearTimeout(this.debounceTimer);
        this.debounceTimer = setTimeout(() => {
            this._runDiagnostics(document);
        }, 300);
    }

    private _runDiagnostics(document: vscode.TextDocument): void {
        const text = document.getText();
        const diagnostics: vscode.Diagnostic[] = [];

        this.checkTagBalance(text, document, diagnostics);
        this.checkImports(text, document, diagnostics);
        this.checkStyleBlocks(text, document, diagnostics);
        this.checkScriptBlocks(text, document, diagnostics);
        this.checkUiRoot(text, document, diagnostics);
        this.checkClassNotSupported(text, document, diagnostics);
        this.checkCommonMistakes(text, document, diagnostics);

        this.diagnosticCollection.set(document.uri, diagnostics);
    }

    private checkTagBalance(text: string, document: vscode.TextDocument, diagnostics: vscode.Diagnostic[]): void {
        // Check paired block tags
        const blockTags = ['style', 'script', 'ui', 'component'];
        for (const tag of blockTags) {
            const openRegex = new RegExp(`<${tag}[\\s>]`, 'g');
            const closeRegex = new RegExp(`</${tag}>`, 'g');
            const opens = (text.match(openRegex) || []).length;
            const closes = (text.match(closeRegex) || []).length;

            if (opens > closes) {
                // Find the last unclosed opening tag
                let lastOpen = -1;
                let match;
                const searchRegex = new RegExp(`<${tag}[\\s>]`, 'g');
                while ((match = searchRegex.exec(text)) !== null) {
                    lastOpen = match.index;
                }
                if (lastOpen >= 0) {
                    const pos = document.positionAt(lastOpen);
                    diagnostics.push(new vscode.Diagnostic(
                        new vscode.Range(pos, pos.translate(0, tag.length + 2)),
                        `Unclosed <${tag}> tag — missing </${tag}>`,
                        vscode.DiagnosticSeverity.Error
                    ));
                }
            } else if (closes > opens && tag !== 'component') {
                const searchRegex = new RegExp(`</${tag}>`, 'g');
                let lastClose = -1;
                let match;
                while ((match = searchRegex.exec(text)) !== null) {
                    lastClose = match.index;
                }
                if (lastClose >= 0) {
                    const pos = document.positionAt(lastClose);
                    diagnostics.push(new vscode.Diagnostic(
                        new vscode.Range(pos, pos.translate(0, tag.length + 3)),
                        `Unexpected </${tag}> — no matching opening tag`,
                        vscode.DiagnosticSeverity.Error
                    ));
                }
            }
        }

        // Check HTML-like element tags (View, div)
        const containerTags = ['View', 'div'];
        for (const tag of containerTags) {
            const openRegex = new RegExp(`<${tag}\\b(?![^>]*/\\s*>)[^>]*>`, 'g');
            const closeRegex = new RegExp(`</${tag}>`, 'g');
            const opens = (text.match(openRegex) || []).length;
            const closes = (text.match(closeRegex) || []).length;

            if (opens > closes) {
                diagnostics.push(new vscode.Diagnostic(
                    new vscode.Range(new vscode.Position(0, 0), new vscode.Position(0, 1)),
                    `Potentially unclosed <${tag}> tag(s) — ${opens} open, ${closes} closed`,
                    vscode.DiagnosticSeverity.Warning
                ));
            }
        }
    }

    private checkImports(text: string, document: vscode.TextDocument, diagnostics: vscode.Diagnostic[]): void {
        const importRegex = /<import\s+[^>]*src="([^"]*)"[^>]*as="([^"]*)"[^>]*\/?>/g;
        const imports: { src: string; as: string; pos: number }[] = [];
        let match;

        while ((match = importRegex.exec(text)) !== null) {
            imports.push({ src: match[1], as: match[2], pos: match.index });
        }

        // Check for missing 'as' attribute
        const badImportRegex = /<import\s+[^>]*src="([^"]*)"(?![^>]*as=)[^>]*\/?>/g;
        while ((match = badImportRegex.exec(text)) !== null) {
            const pos = document.positionAt(match.index);
            diagnostics.push(new vscode.Diagnostic(
                new vscode.Range(pos, pos.translate(0, match[0].length)),
                `Import missing 'as' attribute — component name required`,
                vscode.DiagnosticSeverity.Error
            ));
        }

        // Check for missing 'src' attribute
        const noSrcRegex = /<import\s+(?![^>]*src=)[^>]*as="([^"]*)"[^>]*\/?>/g;
        while ((match = noSrcRegex.exec(text)) !== null) {
            const pos = document.positionAt(match.index);
            diagnostics.push(new vscode.Diagnostic(
                new vscode.Range(pos, pos.translate(0, match[0].length)),
                `Import missing 'src' attribute — file path required`,
                vscode.DiagnosticSeverity.Error
            ));
        }

        // Check for duplicate import names
        const importNames = new Map<string, number>();
        for (const imp of imports) {
            if (importNames.has(imp.as)) {
                const pos = document.positionAt(imp.pos);
                diagnostics.push(new vscode.Diagnostic(
                    new vscode.Range(pos, pos.translate(0, 20)),
                    `Duplicate import name '${imp.as}' — already imported`,
                    vscode.DiagnosticSeverity.Warning
                ));
            }
            importNames.set(imp.as, imp.pos);
        }
    }

    private checkStyleBlocks(text: string, document: vscode.TextDocument, diagnostics: vscode.Diagnostic[]): void {
        // Check for unclosed braces inside <style> blocks
        const styleRegex = /<style>([\s\S]*?)<\/style>/g;
        let match;
        while ((match = styleRegex.exec(text)) !== null) {
            const styleContent = match[1];
            const styleOffset = match.index + 7; // <style> length

            let braceCount = 0;
            for (let i = 0; i < styleContent.length; i++) {
                if (styleContent[i] === '{') braceCount++;
                if (styleContent[i] === '}') braceCount--;
            }

            if (braceCount > 0) {
                const pos = document.positionAt(styleOffset);
                diagnostics.push(new vscode.Diagnostic(
                    new vscode.Range(pos, pos.translate(0, 1)),
                    `Unclosed '{' in <style> block — ${braceCount} unclosed brace(s)`,
                    vscode.DiagnosticSeverity.Error
                ));
            } else if (braceCount < 0) {
                const pos = document.positionAt(styleOffset);
                diagnostics.push(new vscode.Diagnostic(
                    new vscode.Range(pos, pos.translate(0, 1)),
                    `Extra '}' in <style> block — ${Math.abs(braceCount)} unmatched closing brace(s)`,
                    vscode.DiagnosticSeverity.Error
                ));
            }

            // Check for missing semicolons in property declarations
            const lines = styleContent.split('\n');
            let inBlock = false;
            for (let i = 0; i < lines.length; i++) {
                const line = lines[i].trim();
                if (line.includes('{')) inBlock = true;
                if (line.includes('}')) inBlock = false;

                if (inBlock && line.includes(':') && !line.includes('{') && !line.includes('}')) {
                    if (!line.endsWith(';') && !line.startsWith('//') && !line.startsWith('/*') && line.length > 0) {
                        // Find position in document
                        const lineOffset = styleContent.split('\n').slice(0, i).join('\n').length + 1;
                        const pos = document.positionAt(styleOffset + lineOffset);
                        diagnostics.push(new vscode.Diagnostic(
                            new vscode.Range(pos, pos.translate(0, line.length)),
                            `CSS declaration may be missing semicolon`,
                            vscode.DiagnosticSeverity.Hint
                        ));
                    }
                }
            }
        }
    }

    private checkScriptBlocks(text: string, document: vscode.TextDocument, diagnostics: vscode.Diagnostic[]): void {
        const scriptRegex = /<script>([\s\S]*?)<\/script>/g;
        let match;
        while ((match = scriptRegex.exec(text)) !== null) {
            const scriptContent = match[1];
            const scriptOffset = match.index + 8;

            // Check for balanced braces
            let braceCount = 0;
            let inString = false;
            let stringChar = '';
            for (let i = 0; i < scriptContent.length; i++) {
                const ch = scriptContent[i];
                if (inString) {
                    if (ch === stringChar && scriptContent[i - 1] !== '\\') inString = false;
                    continue;
                }
                if (ch === '"' || ch === '\'' || ch === '`') {
                    inString = true;
                    stringChar = ch;
                    continue;
                }
                if (ch === '/' && i + 1 < scriptContent.length) {
                    if (scriptContent[i + 1] === '/') {
                        // Skip line comment
                        while (i < scriptContent.length && scriptContent[i] !== '\n') i++;
                        continue;
                    }
                    if (scriptContent[i + 1] === '*') {
                        // Skip block comment
                        i += 2;
                        while (i + 1 < scriptContent.length && !(scriptContent[i] === '*' && scriptContent[i + 1] === '/')) i++;
                        i++;
                        continue;
                    }
                }
                if (ch === '{') braceCount++;
                if (ch === '}') braceCount--;
            }

            if (braceCount !== 0) {
                const pos = document.positionAt(scriptOffset);
                diagnostics.push(new vscode.Diagnostic(
                    new vscode.Range(pos, pos.translate(0, 1)),
                    `Unbalanced braces in <script> block (${braceCount > 0 ? braceCount + ' unclosed' : Math.abs(braceCount) + ' extra closing'})`,
                    vscode.DiagnosticSeverity.Error
                ));
            }

            // Same for parentheses
            let parenCount = 0;
            inString = false;
            for (let i = 0; i < scriptContent.length; i++) {
                const ch = scriptContent[i];
                if (inString) {
                    if (ch === stringChar && scriptContent[i - 1] !== '\\') inString = false;
                    continue;
                }
                if (ch === '"' || ch === '\'' || ch === '`') {
                    inString = true;
                    stringChar = ch;
                    continue;
                }
                if (ch === '(') parenCount++;
                if (ch === ')') parenCount--;
            }

            if (parenCount !== 0) {
                const pos = document.positionAt(scriptOffset);
                diagnostics.push(new vscode.Diagnostic(
                    new vscode.Range(pos, pos.translate(0, 1)),
                    `Unbalanced parentheses in <script> block (${parenCount > 0 ? parenCount + ' unclosed' : Math.abs(parenCount) + ' extra closing'})`,
                    vscode.DiagnosticSeverity.Warning
                ));
            }
        }
    }

    private checkUiRoot(text: string, document: vscode.TextDocument, diagnostics: vscode.Diagnostic[]): void {
        // Must have at least one <ui> tag (unless this is a component file)
        const hasUi = /<ui[\s>]/.test(text);
        const hasUiClose = /<\/ui>/.test(text);

        if (!hasUi && text.trim().length > 0) {
            // Could be a pure component file — only warn if there's significant content
            const hasScript = /<script>/.test(text);
            const hasStyle = /<style>/.test(text);
            if (hasScript || hasStyle) {
                // Component files (like Button.dom) have <ui> inside them
                // Only warn if there's no <ui> at all
                diagnostics.push(new vscode.Diagnostic(
                    new vscode.Range(new vscode.Position(0, 0), new vscode.Position(0, 1)),
                    `No <ui> root element found — every .dom file needs a <ui> root node`,
                    vscode.DiagnosticSeverity.Warning
                ));
            }
        }
    }

    private checkClassNotSupported(text: string, document: vscode.TextDocument, diagnostics: vscode.Diagnostic[]): void {
        // Warn about ES6 class syntax which NativeDOM doesn't support
        const scriptRegex = /<script>([\s\S]*?)<\/script>/g;
        let match;
        while ((match = scriptRegex.exec(text)) !== null) {
            const scriptContent = match[1];
            const scriptOffset = match.index + 8;

            const classRegex = /\bclass\s+\w+/g;
            let classMatch;
            while ((classMatch = classRegex.exec(scriptContent)) !== null) {
                const pos = document.positionAt(scriptOffset + classMatch.index);
                diagnostics.push(new vscode.Diagnostic(
                    new vscode.Range(pos, pos.translate(0, classMatch[0].length)),
                    `ES6 'class' syntax is NOT supported by NativeDOM. Use IIFE + self.method = function() pattern instead.`,
                    vscode.DiagnosticSeverity.Error
                ));
            }

            // Warn about arrow function class methods
            const asyncMatch = /\basync\s+function\b/.exec(scriptContent);
            if (asyncMatch) {
                const pos = document.positionAt(scriptOffset + asyncMatch.index);
                diagnostics.push(new vscode.Diagnostic(
                    new vscode.Range(pos, pos.translate(0, asyncMatch[0].length)),
                    `'async' functions are not natively supported. Use Promise chains or setTimeout patterns.`,
                    vscode.DiagnosticSeverity.Warning
                ));
            }
        }
    }

    private checkCommonMistakes(text: string, document: vscode.TextDocument, diagnostics: vscode.Diagnostic[]): void {
        // Check for :focus instead of :focused
        const focusRegex = /:focus\b(?!ed)/g;
        let match;
        while ((match = focusRegex.exec(text)) !== null) {
            // Check we're in a style block
            const before = text.substring(0, match.index);
            const styleOpens = (before.match(/<style>/g) || []).length;
            const styleCloses = (before.match(/<\/style>/g) || []).length;
            if (styleOpens > styleCloses) {
                const pos = document.positionAt(match.index);
                diagnostics.push(new vscode.Diagnostic(
                    new vscode.Range(pos, pos.translate(0, 6)),
                    `NativeDOM uses ':focused' not ':focus' — did you mean :focused?`,
                    vscode.DiagnosticSeverity.Warning
                ));
            }
        }

        // Check for background-color instead of bg
        const bgColorRegex = /\bbackground-color\s*:/g;
        while ((match = bgColorRegex.exec(text)) !== null) {
            const pos = document.positionAt(match.index);
            diagnostics.push(new vscode.Diagnostic(
                new vscode.Range(pos, pos.translate(0, 16)),
                `NativeDOM uses 'bg' instead of 'background-color'`,
                vscode.DiagnosticSeverity.Information
            ));
        }

        // Check for background instead of bg
        const bgRegex = /\bbackground\s*:/g;
        while ((match = bgRegex.exec(text)) !== null) {
            // Make sure it's not background-color (already handled)
            if (text[match.index + 10] !== '-') {
                const pos = document.positionAt(match.index);
                diagnostics.push(new vscode.Diagnostic(
                    new vscode.Range(pos, pos.translate(0, 10)),
                    `NativeDOM uses 'bg' instead of 'background'`,
                    vscode.DiagnosticSeverity.Information
                ));
            }
        }
    }
}
