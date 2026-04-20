import * as vscode from 'vscode';
import * as fs from 'fs';
import * as path from 'path';

export interface ParsedArg {
    name: string;
    comment: string;
}

export interface ParsedMethod {
    name: string;
    args: ParsedArg[];
}

// Extract methods optionally including from imported files recursively
export function extractMethodsFromDocument(text: string, documentUri?: vscode.Uri, depth: number = 0): ParsedMethod[] {
    if (depth > 5) return []; // Prevent infinite recursion

    const methods: ParsedMethod[] = [];
    const regex = /(?:self\.([\w]+)\s*=\s*(?:async\s*)?function|function\s+([\w]+))\s*\(([^)]*)\)/g;
    
    let match;
    while ((match = regex.exec(text)) !== null) {
        const name = match[1] || match[2];
        const argsStr = match[3];
        const args: ParsedArg[] = [];
        
        const argStrings = argsStr.split(/\s*,\s*/);
        for (const argStr of argStrings) {
            if (!argStr.trim()) continue;
            const argRegex = /([\w]+)\s*(?:\/\*\s*(.*?)\s*\*\/)?/;
            const argMatch = argRegex.exec(argStr.trim());
            if (argMatch) {
                args.push({
                    name: argMatch[1],
                    comment: argMatch[2] ? argMatch[2].replace(/\\n/g, '\n').trim() : ''
                });
            }
        }
        methods.push({ name, args });
    }

    // Process imports
    if (documentUri) {
        const importRegex = /<import\s+src=["']([^"']+)["']/g;
        const basePath = path.dirname(documentUri.fsPath);
        
        while ((match = importRegex.exec(text)) !== null) {
            const relPath = match[1];
            try {
                let p = path.resolve(basePath, relPath);
                if (!p.endsWith('.dom')) p += '.dom';
                
                if (fs.existsSync(p)) {
                    const content = fs.readFileSync(p, 'utf-8');
                    const importedMethods = extractMethodsFromDocument(content, vscode.Uri.file(p), depth + 1);
                    methods.push(...importedMethods);
                }
            } catch (e) {
                // Ignore read errors
            }
        }
    }
    
    // Deduplicate
    const unique = new Map<string, ParsedMethod>();
    for (const m of methods) unique.set(m.name, m);
    return Array.from(unique.values());
}

export function extractCssVariables(text: string): string[] {
    const vars = new Set<string>();
    const cssVarRegex = /--[\w-]+/g;
    let match;
    while ((match = cssVarRegex.exec(text)) !== null) {
        vars.add(match[0]);
    }
    return Array.from(vars);
}

export interface ExtendedTypeInfo {
    type: 'Element' | 'COM' | 'Unknown';
    componentPath?: string;
}

// Infers the "type" of a variable at cursor location, and attempts cross-file component resolution
export function inferVariableType(variableName: string, text: string, documentUri?: vscode.Uri): ExtendedTypeInfo {
    if (variableName === 'document' || variableName === 'self' || variableName.endsWith('target') || variableName === 'titlebar') return { type: 'Element' };
    if (variableName === 'sys') return { type: 'Unknown' };

    // 1. Check if it's assigned from an Element method
    // e.g. let host = titlebar.parentElement(), var host = document.getElementById("myBrowser")
    const elRegex = new RegExp(`(?:var|let|const)\\s+${variableName}\\s*=\\s*.*?(?:getElementById|querySelector|parentElement|children\\[|createElement|shadowDomSelector)\\s*\\(["']([^"']+)["']\\)?`, 'i');
    
    let isElement = false;
    let targetComponentPath: string | undefined = undefined;

    const elMatch = elRegex.exec(text);
    if (elMatch) {
         isElement = true;
         const selector = elMatch[1]; // e.g. "myBrowser"

         // If the selector looks like an ID, let's find the tag in the document
         const tagRegex = new RegExp(`<([a-zA-Z0-9_-]+)[^>]*\\bid=["']${selector}["']`, 'i');
         const tagMatch = tagRegex.exec(text);
         if (tagMatch && documentUri) {
             const tagName = tagMatch[1]; // e.g. "WebView"
             
             // Now check if there is an <import src="..." as="WebView" /> for this tag!
             const importRegex = new RegExp(`<import\\s+src=["']([^"']+)["'][^>]*\\bas=["']${tagName}["']`, 'i');
             const importMatch = importRegex.exec(text);
             if (importMatch) {
                 const relPath = importMatch[1];
                 const basePath = path.dirname(documentUri.fsPath);
                 try {
                     let p = path.resolve(basePath, relPath);
                     if (!p.endsWith('.dom')) p += '.dom';
                     if (fs.existsSync(p)) {
                         targetComponentPath = p;
                     }
                 } catch (e) {}
             }
         }
    }

    // Fallback naive search if it's an element but regex didn't capture string arg
    if (!isElement) {
        const strictElRegex = new RegExp(`(?:var|let|const)\\s+${variableName}\\s*=\\s*.*?(?:getElementById|querySelector|parentElement|children\\[|createElement|shadowDomSelector)`, 'i');
        if (strictElRegex.test(text)) isElement = true;
    }

    if (isElement) return { type: 'Element', componentPath: targetComponentPath };

    // 2. Check if it's a COM object
    const comRegex = new RegExp(`(?:var|let|const)\\s+${variableName}\\s*=\\s*sys\\.com\\.(?:create|getActive)`, 'i');
    if (comRegex.test(text)) return { type: 'COM' };

    return { type: 'Unknown' };
}
