# re
Recursive egrep and replace tool

There are two main use cases of `re`:

* Recursive egrep, and
* replace the text matched by recursive egrep, in-place

```
Usage: re [OPTION...] [--] REGEX [REPL]
Run recursive egrep(1), optionally replacing matches with REPL.

With REPL, all matches of the extended regular expression REGEX in the text
files below the current working directory are replaced by REPL.  REPL may
contain the special character & to refer to the matched text, and the special
escapes \1 through \9 to refer to the corresponding matching sub-expressions.

Options:
  -d,--diff               View the replacement as a unified diff(1) patch.
  -e,--editor             Open EDITOR to edit the patch before it is applied.
  -x,--exclude XREGEX     Exclude paths matching XREGEX.
  -X,--exclude-name XGLOB Exclude filenames and directories matching XGLOB.
  -f,--file FREGEX        Only include paths matching FREGEX.
  -F,--filename FGLOB     Only include filenames matching FGLOB.
  -i,--ignore-case        Ignore case when matching REGEX.
  -l,--list               List the matched files only. REGEX optional.
  -p,--path PATH...       Run on PATH instead of '.'.
  -q,--quiet              Avoid printing to stdout.
  -u,--update             Update the files in-place.
  -w,--without-filename   Suppress prefixing each line with its path.
```

## Recursive egrep

`re` is just a shorthand for `grep --recursive --extended-regexp --binary-files=without-match`. The following are equivalent:

```
$ re REGEX
$ grep -rEI REGEX
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

## File selection

The `--path PATH`, `--file FREGEX`, `--filename FGLOB`, `--exclude XREGEX`, and `--exclude-name XGLOB` options select the files to operate on.  You can experiment using the `--list` options and by starting with:

```
$ re -l
```

Note that the file list are pruned for binary files, see grep(1) on `--binary-files=without-match` for details.

The `--path` specifies the roots of the file trees, but may refer to regular files too.  The option may be repeated to add additional roots.  If no `--path` is specified, it is as-if `--path .` was specified.

Both `--file` and `--exclude` are matched against the full path of the file, so prefixed with one of the PATH.  Any `./` prefix of such a path is removed before matching.

Both `--filename` and `--exclude-name` operate on directory entry names (filenames and directory names), and use globbing to match instead of regular expressions.  `--filename` specifies what the filename must match to be included, while `--exclude-name` excludes directory names and filenames.

The complete description of how the fileset is calculated is as follows:

1. The fileset is initialized to the set of `PATH` specified by the repeatable `--path` option.
2. If the fileset is empty, `.` is added.
3. Repeat the following until there are no more directories in the fileset: (a) remove a directory, (b) find its directory entries, (c) if the entry is a regular file, and the filename matches the glob pattern of `--filename` (if present), and the filename doesn't match the glob pattern of `--exclude-name` (if specified), then add its full path to the fileset. (d) if the entry is a directory, and the directory name doesn't match the glob pattern of `--exclude-name` (if specified), then add the directory entry to the fileset.
4. The fileset now contains paths to regular files that begin with one of the PATH, or begin with `./` if no PATH were specified.
5. Remove any `./` prefix from the paths in the fileset.
6. Remove any paths not matching the `--file` regex, if specified.
7. Remove any paths matching the `--exclude` regex, if specified.
8. Remove any paths whose content is determined to be binary by grep's `--binary-files=without-match` option.

## Short option contraction

Several short options can be combined into a single short-option argument.  For example the following two are equivalent:

```
re -f /v1/ -X v1 -x podsecu -l SELinuxOptions
re -fXxl /v1/ v1 podsecu SELinuxOptions
```

The order of `/v1/`, `v1`, and `podsecu` must match the order of the short options `f`, `X`, and `x` in the short-option argument, respectively.

## Exit status

The exit status is the same as grep(1):

* 0 if there were at least one match.
* 1 if there were no matches.
* 2 on error.
