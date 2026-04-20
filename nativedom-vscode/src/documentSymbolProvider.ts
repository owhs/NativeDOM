import * as vscode from 'vscode';

export class DomDocumentSymbolProvider implements vscode.DocumentSymbolProvider {
    public provideDocumentSymbols(
        document: vscode.TextDocument,
        token: vscode.CancellationToken
    ): vscode.ProviderResult<vscode.SymbolInformation[] | vscode.DocumentSymbol[]> {
        const symbols: vscode.DocumentSymbol[] = [];
        const text = document.getText();
        const lines = text.split('\n');

        let currentSection: vscode.DocumentSymbol | null = null;
        let scriptSection: vscode.DocumentSymbol | null = null;
        let styleSection: vscode.DocumentSymbol | null = null;

        for (let i = 0; i < lines.length; i++) {
            const line = lines[i];
            const trimmed = line.trim();

            // Match UI elements with IDs (e.g. <Button id="btnCalc">)
            const idMatch = /<([a-zA-Z0-9]+)[^>]*\bid=["']([^"']+)["']/i.exec(line);
            if (idMatch) {
                const tag = idMatch[1];
                const id = idMatch[2];
                const range = new vscode.Range(i, Math.max(0, line.indexOf(idMatch[0])), i, line.length);
                const selectionRange = new vscode.Range(i, line.indexOf(id), i, line.indexOf(id) + id.length);
                const symbol = new vscode.DocumentSymbol(
                    `#${id}`,
                    `<${tag}>`,
                    vscode.SymbolKind.Object,
                    range,
                    selectionRange
                );
                symbols.push(symbol);
            }

            // Match Style Section
            if (/<style/i.test(line)) {
                const range = new vscode.Range(i, 0, i, line.length);
                styleSection = new vscode.DocumentSymbol(
                    'Styling',
                    '<style>',
                    vscode.SymbolKind.Namespace,
                    range,
                    range
                );
                symbols.push(styleSection);
                currentSection = styleSection;
            } else if (/<\/style>/i.test(line) && styleSection) {
                styleSection.range = new vscode.Range(styleSection.range.start, new vscode.Position(i, line.length));
                currentSection = null;
            } else if (currentSection === styleSection && styleSection && trimmed.endsWith('{')) {
                // CSS class or element styling block
                const selector = trimmed.substring(0, trimmed.length - 1).trim();
                if (selector) {
                    const range = new vscode.Range(i, line.indexOf(selector), i, line.length);
                    styleSection.children.push(new vscode.DocumentSymbol(
                        selector,
                        'CSS Matcher',
                        vscode.SymbolKind.Class,
                        range,
                        range
                    ));
                }
            }

            // Match Script Section
            if (/<script/i.test(line)) {
                const range = new vscode.Range(i, 0, i, line.length);
                scriptSection = new vscode.DocumentSymbol(
                    'Logic & Scripting',
                    '<script>',
                    vscode.SymbolKind.Namespace,
                    range,
                    range
                );
                symbols.push(scriptSection);
                currentSection = scriptSection;
            } else if (/<\/script>/i.test(line) && scriptSection) {
                scriptSection.range = new vscode.Range(scriptSection.range.start, new vscode.Position(i, line.length));
                currentSection = null;
            } else if (currentSection === scriptSection && scriptSection) {
                // Extract Javascript functions
                const funcMatch = /(?:function\s+([a-zA-Z0-9_]+)\s*\(|([a-zA-Z0-9_]+)\s*=\s*(?:function|\([^)]*\)\s*=>))/.exec(line);
                if (funcMatch) {
                    const funcName = funcMatch[1] || funcMatch[2];
                    if (funcName) {
                        const range = new vscode.Range(i, line.indexOf(funcName), i, line.length);
                        scriptSection.children.push(new vscode.DocumentSymbol(
                            funcName,
                            'Function',
                            vscode.SymbolKind.Function,
                            range,
                            range
                        ));
                    }
                }
                
                // Extract major global variables
                const varMatch = /^(?:let|var|const)\s+([a-zA-Z0-9_]+)\s*=/.exec(trimmed);
                if (varMatch) {
                    const varName = varMatch[1];
                    const range = new vscode.Range(i, line.indexOf(varName), i, line.length);
                    scriptSection.children.push(new vscode.DocumentSymbol(
                        varName,
                        'Variable',
                        vscode.SymbolKind.Variable,
                        range,
                        range
                    ));
                }
            }
        }

        return symbols;
    }
}
