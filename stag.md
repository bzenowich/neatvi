# stag

`stag` is a tag indexer (like ctags) for neatvi. It scans source files and
writes a ctags-format `tags` file that neatvi reads to jump to symbol
definitions.

## How It Works

Each output line has three tab-separated fields:

```
tag<TAB>path<TAB>location
```

- **tag** — the token (symbol name); this is what you search for.
- **path** — the file that contains it.
- **location** — an ex search pattern (`/^...$/`) that jumps to the
  definition, falling back to a line number.

The patterns are hardcoded in the `tags[]` table in `stag.c`, keyed by file
extension:

- `.c` / `.h` — `#define`, `struct X {`, and function definitions
- `.go` — func / var / const / type
- `.sh` — functions
- `.py` — def / class
- `.ex` — def / defp / defmodule

Files whose extension matches nothing are skipped.

## Indexing a Project

Run `stag` over the files and redirect stdout to `tags`:

```sh
./stag *.c *.h >tags
```

Recursively over a whole project:

```sh
find . -name '*.[ch]' -o -name '*.go' -o -name '*.py' | xargs ./stag >tags
```

Progress (each filename) is printed to stderr; the tags go to stdout. Put
`stag` on your `$PATH` to drop the `./`.

## Searching by Token

neatvi reads the `tags` file. The default path is `tags` in the current
directory; override it with the `TAGPATH` environment variable.

In neatvi:

- `:ta <token>` — jump to a tag
- `^]` (normal mode) — jump to the tag under the cursor
- `:tn` / `:tp` — next / previous matching tag
- `^t` or `:po` — pop the tag stack (jump back)

## Listing Mode (`-a`)

```sh
./stag -a *.c >ls
```

With `-a`, stag emits `path:NNNN: line` instead of tags — a flat symbol
listing. neatvi highlights `*ls` files as directory listings, so you can use
`gl` on a line to jump straight to it, giving a quick symbol browser.
