import * as vscode from 'vscode';
import * as path from 'path';

export class DomDefinitionProvider implements vscode.DefinitionProvider {
    provideDefinition(
        document: vscode.TextDocument,
        position: vscode.Position,
        _token: vscode.CancellationToken
    ): vscode.Definition | null {
        const line = document.lineAt(position).text;

        // Go to Definition for import src paths
        const importMatch = /src="([^"]*)"/.exec(line);
        if (importMatch) {
            const srcPath = importMatch[1];
            const srcStart = line.indexOf(srcPath);
            const srcEnd = srcStart + srcPath.length;

            if (position.character >= srcStart && position.character <= srcEnd) {
                const currentDir = path.dirname(document.uri.fsPath);
                const fullPath = path.resolve(currentDir, srcPath);
                return new vscode.Location(
                    vscode.Uri.file(fullPath),
                    new vscode.Position(0, 0)
                );
            }
        }

        // Go to Definition for getElementById/querySelector string args
        const idMatch = /(?:getElementById|querySelector|shadowDomSelector)\(\s*["']#?([^"']+)["']\s*\)/.exec(line);
        if (idMatch) {
            const id = idMatch[1].replace(/^#/, '');
            const idStart = line.indexOf(id, line.indexOf(idMatch[0]));
            const idEnd = idStart + id.length;

            if (position.character >= idStart && position.character <= idEnd) {
                // Search the same document for id="..."
                const fullText = document.getText();
                const idAttrRegex = new RegExp(`id="${id}"`, 'g');
                const match = idAttrRegex.exec(fullText);
                if (match) {
                    const pos = document.positionAt(match.index);
                    return new vscode.Location(document.uri, pos);
                }
            }
        }

        // Go to Definition for sys.loadScript paths
        const loadScriptMatch = /loadScript\(\s*["']([^"']+)["']/.exec(line);
        if (loadScriptMatch) {
            const scriptPath = loadScriptMatch[1];
            const currentDir = path.dirname(document.uri.fsPath);
            const fullPath = path.resolve(currentDir, scriptPath);
            return new vscode.Location(
                vscode.Uri.file(fullPath),
                new vscode.Position(0, 0)
            );
        }

        // Go to Definition for component tag names → find their import
        const tagMatch = /<(\w+)\s/.exec(line);
        if (tagMatch) {
            const tagName = tagMatch[1];
            const tagStart = line.indexOf(tagName);
            const tagEnd = tagStart + tagName.length;

            if (position.character >= tagStart && position.character <= tagEnd) {
                // Find the import that defines this component
                const fullText = document.getText();
                const importRegex = new RegExp(`<import\\s+[^>]*as="${tagName}"[^>]*src="([^"]*)"`, 'g');
                const imp = importRegex.exec(fullText);
                if (imp) {
                    const srcPath = imp[1];
                    const currentDir = path.dirname(document.uri.fsPath);
                    const fullPath = path.resolve(currentDir, srcPath);
                    return new vscode.Location(
                        vscode.Uri.file(fullPath),
                        new vscode.Position(0, 0)
                    );
                }

                // Try reverse order (src first, then as)
                const importRegex2 = new RegExp(`<import\\s+[^>]*src="([^"]*)"[^>]*as="${tagName}"`, 'g');
                const imp2 = importRegex2.exec(fullText);
                if (imp2) {
                    const srcPath = imp2[1];
                    const currentDir = path.dirname(document.uri.fsPath);
                    const fullPath = path.resolve(currentDir, srcPath);
                    return new vscode.Location(
                        vscode.Uri.file(fullPath),
                        new vscode.Position(0, 0)
                    );
                }
            }
        }

        return null;
    }
}
