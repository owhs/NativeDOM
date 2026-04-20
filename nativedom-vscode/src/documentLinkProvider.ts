import * as vscode from 'vscode';
import * as path from 'path';

export class DomDocumentLinkProvider implements vscode.DocumentLinkProvider {
    public provideDocumentLinks(
        document: vscode.TextDocument,
        token: vscode.CancellationToken
    ): vscode.ProviderResult<vscode.DocumentLink[]> {
        const links: vscode.DocumentLink[] = [];
        const text = document.getText();
        
        // Match import statements: <import src="components/Button.dom" />
        // and image/audio src attributes: <Image src="assets/banner.png" />
        const regex = /(?:<import|<Image|<audio)\s+[^>]*?src=["']([^"']+)["']/gi;
        
        const docDir = path.dirname(document.uri.fsPath);
        
        let match;
        while ((match = regex.exec(text)) !== null) {
            const rawPath = match[1];
            
            // Skip system pseudo-paths that aren't real files
            if (rawPath.startsWith('system:')) continue;
            
            // Calculate position metrics for VS Code link rendering
            const startPos = document.positionAt(match.index + match[0].indexOf(rawPath));
            const endPos = document.positionAt(match.index + match[0].indexOf(rawPath) + rawPath.length);
            const range = new vscode.Range(startPos, endPos);
            
            try {
                // Resolve path relative to the current active document
                let targetPath = rawPath;
                if (!path.isAbsolute(rawPath)) {
                    targetPath = path.resolve(docDir, rawPath);
                }
                
                const uri = vscode.Uri.file(targetPath);
                links.push(new vscode.DocumentLink(range, uri));
            } catch (e) {
                // Ignore path parsing errors
            }
        }
        
        return links;
    }
}
