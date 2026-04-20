import * as vscode from 'vscode';
import { extractMethodsFromDocument, inferVariableType } from './parserUtil';
import * as fs from 'fs';
import { getSignatureFor } from './apiDefinitions';


export class DomSignatureHelpProvider implements vscode.SignatureHelpProvider {
    provideSignatureHelp(
        document: vscode.TextDocument,
        position: vscode.Position,
        token: vscode.CancellationToken,
        context: vscode.SignatureHelpContext
    ): vscode.ProviderResult<vscode.SignatureHelp> {
        
        const lineText = document.lineAt(position).text;
        const textBefore = lineText.substring(0, position.character);
        
        // Very basic naive check: find the closest function call before cursor
        // e.g. "self.Message(123, "
        // We'll iterate backwards to find the unclosed '('
        let openParenIdx = -1;
        let pCount = 0;
        for (let i = textBefore.length - 1; i >= 0; i--) {
            if (textBefore[i] === ')') pCount++;
            else if (textBefore[i] === '(') {
                if (pCount === 0) {
                    openParenIdx = i;
                    break;
                }
                pCount--;
            }
        }
        
        if (openParenIdx === -1) return undefined;
        
        // Find commas between '(' and cursor to know the active parameter
        const argsStr = textBefore.substring(openParenIdx + 1);
        const activeParameter = argsStr.split(',').length - 1;
        
        // Find the function name string right before the '('
        const beforeParen = textBefore.substring(0, openParenIdx).trim();
        const matchName = /([\w\.]+)$/.exec(beforeParen);
        if (!matchName) return undefined;
        
        const funcCallInfo = matchName[1]; // e.g. "self.Message" or "Message"
        
        const isSelf = funcCallInfo.startsWith('self.');
        const isSys = funcCallInfo.startsWith('sys.');
        const justName = funcCallInfo.includes('.') ? funcCallInfo.split('.').pop() : funcCallInfo;
        
        if (!justName) return undefined;

        const signatureHelp = new vscode.SignatureHelp();
        signatureHelp.signatures = [];
        signatureHelp.activeSignature = 0;
        signatureHelp.activeParameter = activeParameter;

        if (isSelf) {
            // Find custom defined method
            const methods = extractMethodsFromDocument(document.getText(), document.uri);
            const target = methods.find(m => m.name === justName);
            if (target) {
                const sigParams = target.args.map(a => `${a.name}`);
                const label = `${justName}(${sigParams.join(', ')})`;
                const sigInfo = new vscode.SignatureInformation(label);
                
                sigInfo.parameters = target.args.map(a => {
                    const pi = new vscode.ParameterInformation(a.name);
                    if (a.comment) pi.documentation = new vscode.MarkdownString(a.comment);
                    return pi;
                });
                
                signatureHelp.signatures.push(sigInfo);
                return signatureHelp;
            }
        } else {
            const objPrefix = funcCallInfo.substring(0, funcCallInfo.lastIndexOf('.'));

            // 1. First try to dynamically resolve it if it's a cross-file component (e.g. browser -> WebView.dom)
            if (objPrefix) {
                const typeInfo = inferVariableType(objPrefix, document.getText(), document.uri);
                if (typeInfo.componentPath) {
                    try {
                        const compDocText = fs.readFileSync(typeInfo.componentPath, 'utf-8');
                        const compMethods = extractMethodsFromDocument(compDocText, vscode.Uri.file(typeInfo.componentPath));
                        const target = compMethods.find(m => m.name === justName);
                        if (target) {
                            const sigParams = target.args.map(a => `${a.name}`);
                            const label = `${justName}(${sigParams.join(', ')})`;
                            const sigInfo = new vscode.SignatureInformation(label);
                            
                            sigInfo.parameters = target.args.map(a => {
                                const pi = new vscode.ParameterInformation(a.name);
                                if (a.comment) pi.documentation = new vscode.MarkdownString(a.comment);
                                return pi;
                            });
                            
                            signatureHelp.signatures.push(sigInfo);
                            return signatureHelp;
                        }
                    } catch (e) {}
                }
            }

            // 2. Resolve internally baked method signatures
            const sig = getSignatureFor(objPrefix, justName);
            
            if (sig) {
                // E.g. "log(msg: string) → void"
                const sigInfo = new vscode.SignatureInformation(sig);
                
                // Extract arguments via regex parsing for highlighting
                const argMatch = /\((.*?)\)/.exec(sig);
                if (argMatch && argMatch[1]) {
                    const args = argMatch[1].split(',').map(a => a.trim());
                    sigInfo.parameters = args.map(a => new vscode.ParameterInformation(a));
                }
                
                signatureHelp.signatures.push(sigInfo);
                return signatureHelp;
            }
        }
        
        return undefined;
    }
}
