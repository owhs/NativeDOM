# CSS Features & Selectors

NativeDOM parses CSS-like styling strictly top-to-bottom internally. The following capabilities define the custom matching engine:

## Selectors

- **Elements**: `Text`, `Button`
- **Identifiers**: `#my-id`
- **Classes**: `.my-class`
- **Attributes**: `[attr]`, `[attr="val"]`, `[attr*="val"]`, `[attr^="val"]`, `[attr$="val"]`
- **Pseudo-States**: `:hover`, `:focused`, `:active`, `:first-child`, `:last-child`, `:nth-child(n)`, `:not(sel)`, `:has(sel)`

## Combinators

- **Descendant**: `parent child`
- **Direct Child**: `parent > child`
- **Adjacent Sibling**: `+` (Matches the immediate next sibling node)
- **General Sibling**: `~` (Matches any subsequent sibling node)

## Grouping & Scope

- **Commas/Multiple**: `header, main > .container`
- **Wildcards**: `*` or `parent > *`
- **Host Injection (Experimental Component Spec)**: `:host([cond]) selector` allows components to internally detect properties passed to their encapsulating host shadow boundaries without rewriting code.

## Advanced Values

- **Variables**: `--var-name: val;` inside elements can be referenced via `var(--var-name, default)`. 
- **Attribute Interpolation**: Native `attr(custom-property, default)` binds HTML attribute text values instantly into static styles.
- **Live Math**: Native `calc(100% - 20)` mathematical interpolation (supports inline `+, -, *, /` calculations).
- **Color Blends**: Native `color-mix(#color1, #color2, percentage)` smooth interpolation mixing.
