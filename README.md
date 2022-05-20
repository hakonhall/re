# re
Recursive egrep and replace tool

```
$ re -h
Usage: re [OPTION...] [--] REGEX [REPL]
Run recursive egrep, replacing matches with REPL (if specified).

Matches are replaced according to 'sed -E s/REGEX/REPL/g'.  The separator / may
be overridden with -s.  REPL may contain the special character & to refer to the
matched text, and the special escapes \1 through \9 to refer to the
corresponding matching sub-expressions.

Options:
  -d,--diff               View modifications in unified diff(1) format.
  -e,--editor             Open EDITOR to edit patch before it is applied.
  -x,--exclude GLOB       Same as grep(1) --exclude.
  -X,--exclude-dir GLOB   Same as grep(1) --exclude-dir.
  -i,--include GLOB       Same as grep(1) --include.
  -l,--list               List the matched files only.
  -p,--path PATH...       Run on PATH instead of '.'.  Repeatable.
  -s,--separator SEP      Use SEP instead of / in sed(1) s/REGEX/REPL/g.
  -u,--update             Update the files in-place.
```

## Recursive egrep

There are two main use cases of `re`: Recursive egrep described here, and replacement described in the next section.

`re` is just a shorthand for `grep -rE` - the following are equivalent:

```
$ re REGEX
$ grep -rE REGEX
```

## Replace

But the primary utility of `re` is to replace matched text with a replacement string using `--editor`.

```
$ re -e REGEX REPL
```

This will

1. make a unified diff(1) file that, if applied with patch(1), will change all matches of REGEX with REPL in all files below the current working directory.
2. Open an editor with the patch file, using the EDITOR environment variable.
3. You can now inspect and modify the patch file as you see fit.
4. Once the editor exits, the patch is applied.

If you figure out in (3) that you'd like to abort the patching, you can clear the file before exiting.
