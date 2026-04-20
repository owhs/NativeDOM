import * as vscode from 'vscode';

export class DomDocumentFormattingProvider implements vscode.DocumentFormattingEditProvider {
    public provideDocumentFormattingEdits(
        document: vscode.TextDocument,
        options: vscode.FormattingOptions,
        token: vscode.CancellationToken
    ): vscode.ProviderResult<vscode.TextEdit[]> {
        const text = document.getText();
        const lines = text.split('\n');
        
        let indentLevel = 0;
        const indentString = options.insertSpaces ? ' '.repeat(options.tabSize) : '\t';
        
        const edits: vscode.TextEdit[] = [];
        
        for (let i = 0; i < lines.length; i++) {
            let line = lines[i].trim();
            // skip empty lines but preserve them without trailing whitespace
            if (!line) {
                // If it wasn't empty initially, wipe the whitespace
                const origLength = lines[i].replace('\r', '').length;
                if (origLength > 0) {
                    edits.push(vscode.TextEdit.replace(new vscode.Range(i, 0, i, lines[i].length), ''));
                }
                continue;
            }

            // Calculation of indent reduction for CURRENT line
            let decreaseIndent = 0;
            if (line.startsWith('</') || line.startsWith('}') || line.startsWith(']')) {
                decreaseIndent = 1;
            }

            // Construct preferred indent
            const currentIndent = Math.max(0, indentLevel - decreaseIndent);
            const preferredPrefix = indentString.repeat(currentIndent);
            
            const originalLineWithoutR = lines[i].endsWith('\r') ? lines[i].substring(0, lines[i].length - 1) : lines[i];
            const originalPrefixLength = originalLineWithoutR.length - originalLineWithoutR.trimStart().length;
            const originalPrefix = originalLineWithoutR.substring(0, originalPrefixLength);
            
            if (originalPrefix !== preferredPrefix) {
                edits.push(vscode.TextEdit.replace(
                    new vscode.Range(i, 0, i, originalPrefixLength),
                    preferredPrefix
                ));
            }

            // Calculation of indent shift for NEXT line
            // Only capture tags that are strictly <text...> but absolutely DO NOT end with />
            // Since lookbehind JS isn't globally universally perfect, let's use positive matches
            const openTags = (line.match(/<[a-zA-Z0-9_-]+(?:[^>]*?[^\/])?>/g) || []).length;
            const selfCloseTags = (line.match(/<[a-zA-Z0-9_-]+[^>]*?\/>/g) || []).length;
            const closeTags = (line.match(/<\/[a-zA-Z0-9_-]+(?:[^>]*)?>/g) || []).length;
            
            const openBraces = (line.match(/\{/g) || []).length;
            const closeBraces = (line.match(/\}/g) || []).length;
            
            const openBrackets = (line.match(/\[/g) || []).length;
            const closeBrackets = (line.match(/\]/g) || []).length;

            // Wait, an open tag match matches self-closing tags if we aren't strict.
            // <Button id="btn" />
            // The `>` matches the end. `[^\/]` ensures it doesn't match if it ends in `/` before `>`.
            // Wait, `<script>` doesn't end with `/`, it's an open block. 
            // `(?:[^>]*?[^\/])?>` means it can have chars, but MUST NOT END in `/`.
            // Let's refine strict tag opening.
            // Simplified logic: If it explicitly matches `</tag>`, it's close.
            // If it matches `/>`, it's self closing. 
            // Anything else that is `<tag...>` is open!
            let lineTagsOpenCount = 0;
            let lineTagsCloseCount = 0;
            
            const tagRegex = /<(\/)?([a-zA-Z0-9_-]+)([^>]*?)(\/?)>/g;
            let match;
            while ((match = tagRegex.exec(line)) !== null) {
                const isClosingBlock = match[1] === '/';
                const isSelfClosing = match[4] === '/';
                
                if (isClosingBlock) {
                    lineTagsCloseCount++;
                } else if (!isSelfClosing) {
                    lineTagsOpenCount++;
                }
            }

            indentLevel += (lineTagsOpenCount - lineTagsCloseCount) + (openBraces - closeBraces) + (openBrackets - closeBrackets);
            
            // Safety bound
            if (indentLevel < 0) indentLevel = 0;
        }

        return edits;
    }
}
