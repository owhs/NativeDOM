# DOM API

This document details the methods and properties available for manipulating the Document Object Model within NativeDOM. Native DOM manipulation mimics standard browser conventions but natively strips away asynchronous race conditions.

## Table of Contents
1. [DOM Element Methods](#dom-element-methods)
2. [Layout & Positioning](#layout--positioning)
3. [HTML Properties & Inline States](#html-properties--inline-states)
4. [Event System](#event-system)

---

## DOM Element Methods

*   `document.querySelector(selector: string) -> Element?`
    Searches the light DOM globally and returns the first element matching the CSS selector. Returns `null` if no match is found.
*   `document.querySelectorAll(selector: string) -> Array<Element>`
    Searches the light DOM globally and returns an array of all matching elements.
*   `document.shadowDomSelector(selector: string) -> Element?`
    Bypasses standard encapsulation boundaries to query elements inside component shadow trees.
*   `document.shadowDomSelectorAll(selector: string) -> Array<Element>`
    Returns all matching shadow elements internally isolated inside components.
*   `document.getElementById(id: string) -> Element?`
    Fast ID lookup globally mapping to the document.
*   `document.createElement(tag: string) -> Element`
    Programmatically creates a detached node memory object. Use `.appendChild` to inject it into the active tree.
*   `el.querySelector`, `el.querySelectorAll`, `el.shadowDomSelector`
    Performs exactly the same query operations, but restricts the scope entirely to the children of `el`.
*   `el.parentElement -> Element?`
    Returns the immediate parent wrapper of this element. Returns `null` if at the root.
*   `el.children -> Array<Element>`
    Returns an array of all direct light child node descriptors.
*   `el.appendChild(child: Element)`
    Injects a detached or existing node onto the end of the element's direct children. Native DOM immediately signals a GUI repaint.
*   `el.getAttribute(key: string) -> string`
    Returns the raw string property value for `key` stored in the element's state memory.
*   `el.setAttribute(key: string, value: string)`
    Sets or updates an element's property. If the property affects layout or visuals, the engine immediately flags the GUI for redrawing.
*   `el.classList.add(cls: string)`, `.remove(cls: string)`, `.toggle(cls: string)`, `.contains(cls: string) -> bool`
    Efficient management for the element's CSS class mappings. All mutating calls automatically queue a repaint.
*   `el.textContent` / `el.innerText` (property strings)
    Gets or overrides the inner text value natively rendered inside the node.
*   `el.getBoundingRect() -> {x, y, width, height}`
    Triggers an immediate layout calculation if dirty and returns the absolute screen coordinate bounds of the element.
*   `el.show()`, `el.hide()`
    Toggles the internal visibility bitmask of the element. Invisible elements still consume layout space unless styled as `display: none`.
*   `el.focus()`, `el.blur()`
    Grants or removes UI interaction focus. Focus applies `:focused` CSS state globally.

---

## Layout & Positioning

NativeDOM implements a flex-like auto-layout engine for structuring applications intelligently without raw coordinate math.

*   `layout="absolute"` *(Default)*
    All children are positioned explicitly utilizing exact `x` and `y` bounds mapping from their parent's zero-coordinate origin. Overlapping occurs freely.
*   `layout="row"`
    Forces all children into a horizontal layout slice.
*   `layout="col"`
    Forces all children into a vertical stacking sequence.

### Layout Modifiers 
**(Active in `row` or `col` only):**
*   `gap="<number>"`
    Injects exactly `<number>` logical pixels of empty padding evenly spaced between each instantiated child block.
*   `justify="<string>"` 
    Controls **Main-Axis** alignment (Horizontal in `row`, Vertical in `col`).
    Values: `"start"`, `"center"`, `"end"` (`"right"`/`"bottom"`), `"space-between"`.
*   `align="<string>"`
    Controls **Cross-Axis** alignment (Vertical in `row`, Horizontal in `col`).
    Values: `"start"`, `"center"`, `"end"` (`"right"`/`"bottom"`).

*Note: You can still assign manual `x` and `y` coordinates to elements nested freely inside `row`/`col` layouts. The layout engine simply modifies their relative mathematical offsets!*

---

## HTML Properties & Inline States

All CSS properties can be set inline via direct HTML attributes.

```xml
<Button border-radius="12" />
```

You can selectively map inline pseudo-state styling without writing a CSS class by prefixing the property with the target state identifier. 

**Example**: 
```xml
<Button hover:bg="#ff0000" focused:border="#00ff00" active:opacity="0.5" />
```

---

## Event System

The framework binds robustly to both generic OS hooks and specific visual interactions!

*   **Document Level Listeners**: `document.addEventListener(type: string, cb)`, `document.removeEventListener(type)`
    Captures bubbling triggers traversing directly to the core document root.
*   **Element Listeners**: `el.addEventListener(type: string, cb)`, `el.removeEventListener(type)`
    Maps specifically onto encapsulated coordinate hits or specialized logic.
*   **Dispatchers**: `document.dispatchEvent(event)`, `el.dispatchEvent(event)`
    Manually queues execution trees firing all listening logic.
*   **Event Controls**: `e.preventDefault()`, `e.stopPropagation()`, `e.stopImmediatePropagation()`
    Halts bubbling/capturing propagation completely inline simulating exact DOM Level 3 definitions.
*   **Global Event: Native** `hotkey`
    Provided via `sys.window.registerHotkey`. Triggers properties: `e.id`, `e.modifiers`, `e.vkCode`.
*   **Global Event: Input** `keydown`, `keypress`, `keyup`
    Triggers properties: `e.keyCode`, `e.key`.
*   **Global Event: Mouse** `click`, `dblclick`, `mousedown`, `mouseup`, `mousemove`, `mouseenter`, `mouseleave`, `contextmenu`, `wheel`
    Injected with positional boundaries `e.clientX`, `e.clientY`, and the bounding object `e.target`.
*   **Global Event: Control** `focus`, `blur`
    Maps activation routines across specific tree nodes.
*   **Global Event: Status** `load`, `resize`, `close`
    Standard workflow hooks allowing robust bootstrapping or shutdown arrays.
    **Note on `close`**: The `close` event is **synchronous**. If you call `e.preventDefault()`, the Windows shutdown interrupt is halted and the app remains open!
*   **Global Event: OS Integrations** `drop`
    Injected upon OS Drag-and-Drop operations tracking target filepaths inside `e.file`.
*   **Custom Dispatch Hooks**: `new CustomEvent("name", { detail: object })`
    Synthesizes brand new pseudo-logic wrappers for custom component APIs.
