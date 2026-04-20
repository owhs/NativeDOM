export const sysMembers = [
    { name: 'log', desc: 'Print to console, OutputDebugString, and sys_log.txt', sig: 'log(msg: string)' },
    { name: 'time', desc: 'Microsecond system uptime (GetTickCount64)', sig: 'time() \u2192 number' },
    { name: 'exec', desc: 'Fork a Windows sub-process', sig: 'exec(cmd: string, hidden?: bool) \u2192 number' },
    { name: 'readText', desc: 'Blocking file read (UTF-8)', sig: 'readText(path: string) \u2192 string' },
    { name: 'writeText', desc: 'Blocking file write', sig: 'writeText(path: string, data: string)' },
    { name: 'loadScript', desc: 'Hot-load a .dom file', sig: 'loadScript(path: string, asNewProcess?: bool)' },
    { name: 'dllCall', desc: 'FFI call returning number', sig: 'dllCall(dll: string, func: string, args: any[]) \u2192 number' },
    { name: 'dllCallString', desc: 'FFI call returning string', sig: 'dllCallString(dll: string, func: string, args: any[]) \u2192 string' },
    { name: 'loadExtension', desc: 'Load a C++ NativeDOM extension DLL', sig: 'loadExtension(dllPath: string) \u2192 bool' },
    { name: 'sendMessage', desc: 'Raw Win32 SendMessage', sig: 'sendMessage(hwnd: number, msg: number, wParam: number, lParam: number)' },
    { name: 'findWindow', desc: 'Find window by title', sig: 'findWindow(title: string) \u2192 number' },
    { name: 'getHwnd', desc: 'Get app HWND', sig: 'getHwnd() \u2192 number' },
    { name: 'screenshot', desc: 'Capture window to BMP', sig: 'screenshot(path?: string, x?: number, y?: number, w?: number, h?: number) \u2192 bool' },
    { name: 'getPixelColor', desc: 'Get pixel color at coords', sig: 'getPixelColor(x?: number, y?: number) \u2192 {r, g, b, hex, x, y}' },
    { name: 'setLogEnabled', desc: 'Toggle logging', sig: 'setLogEnabled(enable: bool)' },
    { name: 'sleep', desc: 'Blocking Sleep (ms)', sig: 'sleep(ms: number)' },
    { name: 'window', desc: 'Window management APIs', sig: 'sys.window' },
    { name: 'screen', desc: 'Screen information APIs', sig: 'sys.screen' },
    { name: 'keyboard', desc: 'Keyboard hook APIs', sig: 'sys.keyboard' },
    { name: 'clipboard', desc: 'Clipboard APIs', sig: 'sys.clipboard' },
    { name: 'com', desc: 'COM/OLE Automation APIs', sig: 'sys.com' },
    { name: 'tray', desc: 'Tray Icon/Menu APIs', sig: 'sys.tray' }
];

export const windowMembers = [
    { name: 'resize', sig: 'resize(w: number, h: number)' },
    { name: 'move', sig: 'move(x: number, y: number)' },
    { name: 'center', sig: 'center()' },
    { name: 'hide', sig: 'hide()' },
    { name: 'show', sig: 'show()' },
    { name: 'minimize', sig: 'minimize()' },
    { name: 'maximize', sig: 'maximize()' },
    { name: 'restore', sig: 'restore()' },
    { name: 'close', sig: 'close()' },
    { name: 'setFullscreen', sig: 'setFullscreen(enable: bool)' },
    { name: 'isMaximized', sig: 'isMaximized() \u2192 bool' },
    { name: 'setTitle', sig: 'setTitle(title: string)' },
    { name: 'setOpacity', sig: 'setOpacity(alpha: number)' },
    { name: 'setAlwaysOnTop', sig: 'setAlwaysOnTop(enable: bool)' },
    { name: 'setIcon', sig: 'setIcon(filepath: string)' },
    { name: 'getSize', sig: 'getSize() \u2192 {width, height}' },
    { name: 'getPosition', sig: 'getPosition() \u2192 {x, y}' },
    { name: 'getActiveProcessName', sig: 'getActiveProcessName() \u2192 string' },
    { name: 'registerHotkey', sig: 'registerHotkey(id: number, keyStr: string)' },
    { name: 'unregisterHotkey', sig: 'unregisterHotkey(id: number)' }
];

export const screenMembers = [
    { name: 'getInfo', sig: 'getInfo() \u2192 {width, height}' },
    { name: 'getDPI', sig: 'getDPI() \u2192 number' },
    { name: 'getMousePosition', sig: 'getMousePosition() \u2192 {x, y}' },
    { name: 'getMonitorAt', sig: 'getMonitorAt(x: number, y: number) \u2192 ScreenInfo' }
];

export const keyboardMembers = [
    { name: 'hook', sig: 'hook(callback: Function)' },
    { name: 'unhook', sig: 'unhook()' },
    { name: 'sendText', sig: 'sendText(text: string)' }
];

export const clipboardMembers = [
    { name: 'getText', sig: 'getText() \u2192 string' },
    { name: 'setText', sig: 'setText(text: string)' },
    { name: 'clear', sig: 'clear()' },
    { name: 'getFormat', sig: 'getFormat() \u2192 number' },
    { name: 'setFormat', sig: 'setFormat(format: number, data: string)' }
];

export const comMembers = [
    { name: 'create', sig: 'create(progId: string) \u2192 COMObject | null' },
    { name: 'getActive', sig: 'getActive(progId: string) \u2192 COMObject | null' },
    { name: 'releaseAll', sig: 'releaseAll()' }
];

export const trayMembers = [
    { name: 'setIcon', sig: 'setIcon(filepath: string)' },
    { name: 'setMenu', sig: 'setMenu(menuItems: any[])' },
    { name: 'remove', sig: 'remove()' }
];

export const mathMembers = [
    { name: 'abs', sig: 'abs(x: number) \u2192 number' }, { name: 'acos', sig: 'acos(x: number) \u2192 number' },
    { name: 'asin', sig: 'asin(x: number) \u2192 number' }, { name: 'atan', sig: 'atan(x: number) \u2192 number' },
    { name: 'ceil', sig: 'ceil(x: number) \u2192 number' }, { name: 'cos', sig: 'cos(x: number) \u2192 number' },
    { name: 'exp', sig: 'exp(x: number) \u2192 number' }, { name: 'floor', sig: 'floor(x: number) \u2192 number' },
    { name: 'log', sig: 'log(x: number) \u2192 number' }, { name: 'max', sig: 'max(...values: number[]) \u2192 number' },
    { name: 'min', sig: 'min(...values: number[]) \u2192 number' }, { name: 'pow', sig: 'pow(x: number, y: number) \u2192 number' },
    { name: 'random', sig: 'random() \u2192 number' }, { name: 'round', sig: 'round(x: number) \u2192 number' },
    { name: 'sin', sig: 'sin(x: number) \u2192 number' }, { name: 'sqrt', sig: 'sqrt(x: number) \u2192 number' },
    { name: 'tan', sig: 'tan(x: number) \u2192 number' }, { name: 'trunc', sig: 'trunc(x: number) \u2192 number' },
    { name: 'PI', sig: 'PI: number' }, { name: 'E', sig: 'E: number' }
];

export const jsonMembers = [
    { name: 'parse', sig: 'parse(text: string) \u2192 any' },
    { name: 'stringify', sig: 'stringify(value: any, replacer?: any, space?: number|string) \u2192 string' }
];

export const comInstanceMethods = [
    { name: 'call', sig: 'call(methodName: string, ...args) \u2192 any' },
    { name: 'get', sig: 'get(propertyName: string) \u2192 any' },
    { name: 'set', sig: 'set(propertyName: string, value: any)' },
    { name: 'release', sig: 'release()' },
    { name: 'forEach', sig: 'forEach(callback: (item: any) => void)' },
    { name: 'toArray', sig: 'toArray() \u2192 any[]' }
];

export const elementInstanceMethods = [
    { name: 'querySelector', sig: 'querySelector(selector: string) \u2192 Element | null' },
    { name: 'querySelectorAll', sig: 'querySelectorAll(selector: string) \u2192 Element[]' },
    { name: 'shadowDomSelector', sig: 'shadowDomSelector(selector: string) \u2192 Element | null' },
    { name: 'shadowDomSelectorAll', sig: 'shadowDomSelectorAll(selector: string) \u2192 Element[]' },
    { name: 'getAttribute', sig: 'getAttribute(name: string) \u2192 string | null' },
    { name: 'setAttribute', sig: 'setAttribute(name: string, value: string)' },
    { name: 'classList', sig: 'classList' },
    { name: 'appendChild', sig: 'appendChild(element: Element)' },
    { name: 'getBoundingRect', sig: 'getBoundingRect() \u2192 {left, top, right, bottom, width, height}' },
    { name: 'addEventListener', sig: 'addEventListener(type: string, listener: Function, options?: any)' },
    { name: 'removeEventListener', sig: 'removeEventListener(type: string, listener: Function)' },
    { name: 'dispatchEvent', sig: 'dispatchEvent(event: any)' },
    { name: 'show', sig: 'show()' },
    { name: 'hide', sig: 'hide()' },
    { name: 'focus', sig: 'focus()' },
    { name: 'blur', sig: 'blur()' },
    { name: 'parentElement', sig: 'parentElement \u2192 Element | null' },
    { name: 'children', sig: 'children \u2192 Element[]' },
    { name: 'textContent', sig: 'textContent: string' },
    { name: 'innerText', sig: 'innerText: string' },
    { name: 'innerHTML', sig: 'innerHTML: string' },
    { name: 'id', sig: 'id: string' },
    { name: 'value', sig: 'value: string' },
    { name: 'checked', sig: 'checked: boolean' }
];

export function getSignatureFor(objPrefix: string, method: string): string | undefined {
    // Determine which array to search
    let srcArray: {name: string, sig: string}[] | undefined;
    
    if (objPrefix === 'sys') srcArray = sysMembers;
    else if (objPrefix === 'sys.window') srcArray = windowMembers;
    else if (objPrefix === 'sys.screen') srcArray = screenMembers;
    else if (objPrefix === 'sys.keyboard') srcArray = keyboardMembers;
    else if (objPrefix === 'sys.clipboard') srcArray = clipboardMembers;
    else if (objPrefix === 'sys.com') srcArray = comMembers;
    else if (objPrefix === 'sys.tray') srcArray = trayMembers;
    else if (objPrefix === 'Math') srcArray = mathMembers;
    else if (objPrefix === 'JSON') srcArray = jsonMembers;
    else if (objPrefix === 'document' || objPrefix.endsWith('target') || objPrefix === 'parentElement') srcArray = elementInstanceMethods;
    
    if (srcArray) {
        const item = srcArray.find(m => m.name === method);
        if (item) return item.sig;
    }
    
    // Check fallback arrays if unknown prefix
    const asCom = comInstanceMethods.find(m => m.name === method);
    if (asCom) return asCom.sig;
    
    const asEl = elementInstanceMethods.find(m => m.name === method);
    if (asEl) return asEl.sig;
    
    return undefined;
}
