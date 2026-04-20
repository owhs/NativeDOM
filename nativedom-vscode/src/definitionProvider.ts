import * as vscode from 'vscode';
import * as path from 'path';
import * as fs from 'fs';
import { inferVariableType } from './parserUtil';


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

        // Go to Definition for Object Method Calls (e.g. browser.navigateToString)
        const wordRange = document.getWordRangeAtPosition(position, /[\w]+\.[\w]+/);
        if (wordRange) {
            const word = document.getText(wordRange); // e.g. "browser.navigateToString"
            const parts = word.split('.');
            if (parts.length === 2) {
                const prefix = parts[0];
                const method = parts[1];

                if (prefix === 'self') {
                    // Search current document
                    const fullText = document.getText();
                    const rx = new RegExp(`self\\.${method}\\s*=`, 'g');
                    const match = rx.exec(fullText);
                    if (match) {
                        return new vscode.Location(document.uri, document.positionAt(match.index));
                    }
                } else {
                    // Try to infer cross-component
                    const typeInfo = inferVariableType(prefix, document.getText(), document.uri);
                    if (typeInfo.componentPath) {
                        try {
                            const compDocText = fs.readFileSync(typeInfo.componentPath, 'utf-8');
                            const rx = new RegExp(`(?:self\\.${method}\\s*=\\s*(?:async\\s*)?function|function\\s+${method}\\s*\\()`, 'g');
                            const match = rx.exec(compDocText);
                            if (match) {
                                // We need the Line and Character for the target file
                                const lines = compDocText.substring(0, match.index).split('\n');
                                const lineNum = lines.length - 1;
                                const charNum = lines[lines.length - 1].length;
                                return new vscode.Location(
                                    vscode.Uri.file(typeInfo.componentPath),
                                    new vscode.Position(lineNum, charNum)
                                );
                            }
                        } catch(e) {}
                    }
                }
            }
        }

        // Go to Definition for plain function calls (e.g. startTimer())
        const plainWordRange = document.getWordRangeAtPosition(position, /[a-zA-Z0-9_]+/);
        if (plainWordRange) {
            const word = document.getText(plainWordRange);
            // Search current document for 'function word(' or 'self.word ='
            const fullText = document.getText();
            const rx = new RegExp(`(?:self\\.${word}\\s*=\\s*(?:async\\s*)?function|function\\s+${word}\\s*\\()`, 'g');
            const match = rx.exec(fullText);
            if (match) {
                return new vscode.Location(document.uri, document.positionAt(match.index));
            }
        }

        return null;
    }
}
