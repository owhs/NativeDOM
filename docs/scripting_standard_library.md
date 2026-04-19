# Scripting & Standard Library

NativeDOM runs a fully custom V8-like AST interpreter natively in C++. It formally supports most structural ES6 fundamentals!

## Table of Contents
1. [Globals](#globals)
2. [Math Object](#math-object)
3. [String & Array Prototypes](#string--array-prototypes)
4. [Object & JSON](#object--json)
5. [Async Boundaries](#async-boundaries)

---

## Globals
*   `console.log(...)` (Equivalent to `sys.log()`)
*   `parseInt(string, [radix]) -> number` / `parseFloat(string) -> number`
*   `isNaN(value) -> bool` / `isFinite(value) -> bool`

## Math Object
Native wrappers wrapping standard C++ `<cmath>`.
*   `Math.PI` / `Math.E`
*   `Math.floor(n)` / `Math.ceil(n)` / `Math.round(n)` / `Math.trunc(n)`
*   `Math.abs(n)` / `Math.sqrt(n)` / `Math.pow(base, exp)`
*   `Math.min(...args)` / `Math.max(...args)`
*   `Math.random()` (Returns `0.0 - 1.0` utilizing native `rand()`)
*   `Math.sin(n)` / `Math.cos(n)` / `Math.tan(n)` / `Math.log(n)`

## String & Array Prototypes

### String.prototype
*   **Constructors**: `String.fromCharCode(...codes)`
*   **Search**: `.indexOf(q)`, `.lastIndexOf(q)`, `.includes(q)`, `.startsWith(q)`, `.endsWith(q)`, `.match(expr)`
*   **Format**: `.toUpperCase()`, `.toLowerCase()`, `.trim()`, `.padStart(len, [pad])`
*   **Mutation**: `.slice(start, [end])`, `.substring(start, [end])`, `.split([sep])`, `.replace(from, to)`, `.replaceAll(from, to)`, `.repeat(n)`
*   **Binary**: `.charAt(n)`, `.charCodeAt(n)`

### Array.prototype
*   **Constructors**: `Array.isArray(obj)`, `Array.from(obj)`
*   **Mutations**: `.push(v)`, `.pop()`, `.shift()`, `.unshift(v)`, `.splice(start, [deleteCount], [...items])`, `.reverse()`, `.sort([compareFn])`
*   **Iteration**: `.forEach(fn)`, `.map(fn)`, `.filter(fn)`, `.reduce(fn, [init])`, `.find(fn)`, `.findIndex(fn)`, `.some(fn)`, `.every(fn)`
*   **Inspection**: `.indexOf(v)`, `.includes(v)`, `.join([sep])`, `.slice(start, [end])`, `.concat(...arrays)`, `.flat()`

*Note: NativeDOM Arrays correctly cascade prototype links allowing fluid chaining (e.g. `arr.map().filter().join()`).*

## Object & JSON

*   `JSON.stringify(obj) -> string`
*   `JSON.parse(string) -> object`
*   `Object.keys(obj) -> Array<string>`
*   `Object.values(obj) -> Array<any>`
*   `Object.entries(obj) -> Array<Array>`
*   `Object.assign(target, ...sources)`
*   `obj.hasOwnProperty(key) -> bool`

### Number.prototype
*   `num.toFixed(digits)`
*   `num.toString([radix])` (Supports radix 16 for hexadecimal encoding!)

---

## Async Boundaries
*   `setTimeout(fn: Function, ms: number)`, `setInterval(fn: Function, ms: number)`
    Asynchronous event handlers queue execution exactly inside NativeDOM's generic message loop.
*   `clearTimeout(id: number)`, `clearInterval(id: number)`
    Silently unwraps pending execution arrays halting future calls natively.
*   `new Promise((resolve, reject) => ...)`, `.then()`, `.catch()`
    Standard ECMAScript implementation. NativeDOM resolves promise memory cleanly without blocking layout updates!
*   `fetch(url: string, { method, body, headers })`
    Fully async standard XHR networking bridge utilizing internal multi-threading over Windows HTTP layers.
    Returns a `Promise` resolving to:
    ```javascript
    {
        ok: boolean,
        status: number,
        body: string, // raw string output
        text: () => string, // callable payload text mapping 
        json: () => object // callable JSON object parser boundary
    }
    ```
