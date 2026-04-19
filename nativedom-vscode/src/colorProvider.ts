import * as vscode from 'vscode';

export class DomColorProvider implements vscode.DocumentColorProvider {
    provideDocumentColors(
        document: vscode.TextDocument,
        _token: vscode.CancellationToken
    ): vscode.ColorInformation[] {
        const config = vscode.workspace.getConfiguration('nativedom');
        if (!config.get('enableColorDecorations', true)) return [];

        const colors: vscode.ColorInformation[] = [];
        const text = document.getText();

        // Match hex colors: #RGB, #RRGGBB, #RRGGBBAA
        const hexRegex = /#(?:[0-9a-fA-F]{8}|[0-9a-fA-F]{6}|[0-9a-fA-F]{3})\b/g;
        let match;

        while ((match = hexRegex.exec(text)) !== null) {
            const hex = match[0];
            const color = this.parseHexColor(hex);
            if (color) {
                const startPos = document.positionAt(match.index);
                const endPos = document.positionAt(match.index + hex.length);
                colors.push(new vscode.ColorInformation(
                    new vscode.Range(startPos, endPos),
                    color
                ));
            }
        }

        return colors;
    }

    provideColorPresentations(
        color: vscode.Color,
        context: { document: vscode.TextDocument; range: vscode.Range },
        _token: vscode.CancellationToken
    ): vscode.ColorPresentation[] {
        const r = Math.round(color.red * 255);
        const g = Math.round(color.green * 255);
        const b = Math.round(color.blue * 255);
        const a = Math.round(color.alpha * 255);

        const presentations: vscode.ColorPresentation[] = [];

        // #RRGGBB format
        if (a === 255) {
            const hex6 = `#${this.toHex(r)}${this.toHex(g)}${this.toHex(b)}`;
            presentations.push(new vscode.ColorPresentation(hex6));
        }

        // #RRGGBBAA format
        const hex8 = `#${this.toHex(r)}${this.toHex(g)}${this.toHex(b)}${this.toHex(a)}`;
        presentations.push(new vscode.ColorPresentation(hex8));

        return presentations;
    }

    private parseHexColor(hex: string): vscode.Color | null {
        if (!hex.startsWith('#')) return null;

        let r = 0, g = 0, b = 0, a = 255;

        if (hex.length === 7) {
            r = parseInt(hex.substring(1, 3), 16);
            g = parseInt(hex.substring(3, 5), 16);
            b = parseInt(hex.substring(5, 7), 16);
        } else if (hex.length === 4) {
            r = parseInt(hex[1] + hex[1], 16);
            g = parseInt(hex[2] + hex[2], 16);
            b = parseInt(hex[3] + hex[3], 16);
        } else if (hex.length === 9) {
            r = parseInt(hex.substring(1, 3), 16);
            g = parseInt(hex.substring(3, 5), 16);
            b = parseInt(hex.substring(5, 7), 16);
            a = parseInt(hex.substring(7, 9), 16);
        } else {
            return null;
        }

        if (isNaN(r) || isNaN(g) || isNaN(b) || isNaN(a)) return null;

        return new vscode.Color(r / 255, g / 255, b / 255, a / 255);
    }

    private toHex(n: number): string {
        const hex = n.toString(16).toUpperCase();
        return hex.length === 1 ? '0' + hex : hex;
    }
}
