import * as vscode from 'vscode';
import { DomCompletionProvider } from './completionProvider';
import { DomHoverProvider } from './hoverProvider';
import { DomDiagnosticProvider } from './diagnosticProvider';
import { DomColorProvider } from './colorProvider';
import { DomDefinitionProvider } from './definitionProvider';
import { DomSignatureHelpProvider } from './signatureProvider';
import { DomDocumentSymbolProvider } from './documentSymbolProvider';
import { DomDocumentFormattingProvider } from './documentFormattingProvider';
import { DomDocumentLinkProvider } from './documentLinkProvider';


export function activate(context: vscode.ExtensionContext) {
    const selector: vscode.DocumentSelector = { language: 'dom', scheme: 'file' };

    // ---- Completion (IntelliSense) ----
    const completionProvider = new DomCompletionProvider();
    context.subscriptions.push(
        vscode.languages.registerCompletionItemProvider(selector, completionProvider,
            '<', ' ', ':', '.', '#', '"', '-', '(')
    );

    // ---- Hover Documentation ----
    const hoverProvider = new DomHoverProvider();
    context.subscriptions.push(
        vscode.languages.registerHoverProvider(selector, hoverProvider)
    );

    // ---- Signature Help ----
    const signatureProvider = new DomSignatureHelpProvider();
    context.subscriptions.push(
        vscode.languages.registerSignatureHelpProvider(selector, signatureProvider, '(', ',')
    );

    // ---- Diagnostics ----
    const diagnosticCollection = vscode.languages.createDiagnosticCollection('nativedom');
    context.subscriptions.push(diagnosticCollection);
    const diagnosticProvider = new DomDiagnosticProvider(diagnosticCollection);
    
    // Run diagnostics on open and change
    if (vscode.window.activeTextEditor?.document.languageId === 'dom') {
        diagnosticProvider.updateDiagnostics(vscode.window.activeTextEditor.document);
    }
    context.subscriptions.push(
        vscode.workspace.onDidChangeTextDocument(e => {
            if (e.document.languageId === 'dom') {
                diagnosticProvider.updateDiagnostics(e.document);
            }
        }),
        vscode.workspace.onDidOpenTextDocument(doc => {
            if (doc.languageId === 'dom') {
                diagnosticProvider.updateDiagnostics(doc);
            }
        }),
        vscode.workspace.onDidCloseTextDocument(doc => {
            diagnosticCollection.delete(doc.uri);
        })
    );

    // ---- Color Provider ----
    const colorProvider = new DomColorProvider();
    context.subscriptions.push(
        vscode.languages.registerColorProvider(selector, colorProvider)
    );

    // ---- Definition Provider (Go to Definition for imports) ----
    const definitionProvider = new DomDefinitionProvider();
    context.subscriptions.push(
        vscode.languages.registerDefinitionProvider(selector, definitionProvider)
    );

    // ---- Document Symbol Provider (Outline View) ----
    const symbolProvider = new DomDocumentSymbolProvider();
    context.subscriptions.push(
        vscode.languages.registerDocumentSymbolProvider(selector, symbolProvider)
    );

    // ---- Document Formatting Edit Provider (Beautifier / Formatter) ----
    const formatProvider = new DomDocumentFormattingProvider();
    context.subscriptions.push(
        vscode.languages.registerDocumentFormattingEditProvider(selector, formatProvider)
    );

    // ---- Document Link Provider (CTRL+Click File Jumping) ----
    const linkProvider = new DomDocumentLinkProvider();
    context.subscriptions.push(
        vscode.languages.registerDocumentLinkProvider(selector, linkProvider)
    );



    // ---- Commands ----
    context.subscriptions.push(
        vscode.commands.registerCommand('nativedom.runFile', async () => {
            const editor = vscode.window.activeTextEditor;
            if (!editor || editor.document.languageId !== 'dom') {
                vscode.window.showWarningMessage('No .dom file is currently active.');
                return;
            }

            await editor.document.save();
            const filePath = editor.document.uri.fsPath;

            // Look for dom.exe in workspace
            const workspaceFolders = vscode.workspace.workspaceFolders;
            let domExePath = 'dom.exe';
            if (workspaceFolders) {
                const rootPath = workspaceFolders[0].uri.fsPath;
                const possiblePaths = [
                    `${rootPath}\\dom.exe`,
                    `${rootPath}\\..\\dom.exe`,
                ];
                for (const p of possiblePaths) {
                    try {
                        await vscode.workspace.fs.stat(vscode.Uri.file(p));
                        domExePath = p;
                        break;
                    } catch { /* not found, try next */ }
                }
            }

            const terminal = vscode.window.createTerminal({
                name: 'NativeDOM',
                cwd: filePath.substring(0, filePath.lastIndexOf('\\')),
            });
            terminal.show();
            // Call operator & is REQUIRED in powershell when path has spaces or quotes
            terminal.sendText(`& "${domExePath}" "${filePath}"`);
        }),

        vscode.commands.registerCommand('nativedom.buildBundle', async () => {
            const editor = vscode.window.activeTextEditor;
            if (!editor || editor.document.languageId !== 'dom') {
                vscode.window.showWarningMessage('No .dom file is currently active.');
                return;
            }

            const workspaceFolders = vscode.workspace.workspaceFolders;
            if (!workspaceFolders) {
                vscode.window.showErrorMessage('No workspace folder open.');
                return;
            }
            const rootPath = workspaceFolders[0].uri.fsPath;
            const filePath = editor.document.uri.fsPath;

            const terminal = vscode.window.createTerminal({
                name: 'NativeDOM Build',
                cwd: rootPath,
            });
            terminal.show();
            terminal.sendText(`build.bat`);
        }),

        vscode.commands.registerCommand('nativedom.openDocs', () => {
            const docsUri = vscode.Uri.file(
                vscode.workspace.workspaceFolders?.[0]?.uri.fsPath + '\\docs\\getting_started.md'
            );
            vscode.commands.executeCommand('markdown.showPreview', docsUri);
        })
    );

    // ---- Status Bar ----
    const statusBar = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Left, 100);
    statusBar.text = '$(zap) NativeDOM';
    statusBar.tooltip = 'NativeDOM Extension Active';
    statusBar.command = 'nativedom.runFile';
    context.subscriptions.push(statusBar);

    const updateStatusBar = () => {
        if (vscode.window.activeTextEditor?.document.languageId === 'dom') {
            statusBar.show();
        } else {
            statusBar.hide();
        }
    };
    updateStatusBar();
    context.subscriptions.push(vscode.window.onDidChangeActiveTextEditor(updateStatusBar));

    console.log('NativeDOM extension activated');
}

export function deactivate() {}
