#!/bin/bash

shopt -s lastpipe
set -o pipefail

function Usage {
    cat <<'EOF'
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
EOF

    exit 0
}

function Fail {
    echo "$1" >& 2
    exit 2
}

function Main {
    local mode= sep= opt_h=-H opt_i= opt_q= fregex= xregex=
    local -a paths=() grep_opts=(-I)
    local -i matches_exit_code=0 no_matches_exit_code=1

    local regex
    local short_options=
    while true
    do
        if (( ${#short_options} > 0 ))
        then
            local opt="-${short_options:0:1}"
            short_options="${short_options:1}"
        elif (( $# == 0 ))
        then
            test "$mode" == list || Fail "Missing REGEX, see -h for usage"
            regex=. # Unfortunately, no grep regex will match an empty file
            break
        else
            local opt="$1"
            shift || Fail "Missing REGEX, see -h for usage"
        fi

        case "$opt" in
            --) break ;;
            -d|--diff) mode=diff ;;
            -e|--editor) mode=editor ;;
            -x|--exclude)
                xregex="$1"
                shift || Fail "Missing argument to $opt"
                ;;
            -X|--exclude-name)
                grep_opts+=(--exclude "$1" --exclude-dir "$1")
                shift || Fail "Missing argument to $opt"
                ;;
            -f|--file)
                fregex="$1"
                shift || Fail "Missing argument to $opt"
                ;;
            -F|--filename)
                grep_opts+=(--include "$1")
                shift || Fail "Missing argument to $opt"
                ;;
            -h|--help) Usage ;;
            -i|--ignore-case) opt_i=-i ;;
            -l|--list) mode=list ;;
            -p|--path)
                paths+=("$1") # Add canonical path
                shift || Fail "Missing argument to $opt"
                ;;
            -q|--quiet) opt_q=-q ;;
            -s|--separator)
                # Undocumented option to use when no sep is found automatically.
                sep="$1"
                (( ${#sep} == 1 )) || Fail "Invalid separator: '$sep'"
                shift || Fail "Missing argument to $opt"
                ;;
            -u|--update) mode=update ;;
            -w|--without-filename) opt_h=-h ;; # aka --no-filename
            -?|--*) Fail "Unknown option: '$opt'" ;;
            -*) short_options="${opt:1}" ;;
            *)
                regex="$opt"
                break
                ;;
        esac
    done

    type grep &> /dev/null || Fail "Command not found: 'grep'"

    # Resolve paths
    if test -n "$fregex" -o -n "$xregex"
    then
        # Some paths may start with "-", which find(1) cannot handle.
        local -a normpaths=()
        local path
        for path in "${paths[@]}"
        do
            if test "$path"       == .   || \
               test "${path:0:1}" == /   || \
               test "${path:0:2}" == ./  || \
               test "${path:0:3}" == ../
            then
                normpaths+=("$path")
            else
                normpaths+=(./"$path")
            fi
        done
        (( ${#normpaths[@]} > 0 )) || normpaths=(.)

        paths=()
        find "${normpaths[@]}" -type f | while read -r
        do
            # TODO: Match against normpath, or stripped of leading "./"?
            # TODO: Match against path equal to that output by -l.
            local path="${REPLY#./}"
            test -z "$fregex" || [[ "$path" =~ $fregex ]] || continue
            test -z "$xregex" || ! [[ "$path" =~ $xregex ]] || continue
            paths+=("$path") # Or $REPLY?
        done

        if (( ${#paths[@]} == 0 ))
        then
            test -n "$opt_q" || echo "No paths matched" >&2
            return $no_matches_exit_code
        fi
    fi

    if test "$mode" == list
    then
        if grep --color=auto -rEl $opt_h $opt_i $opt_q "${grep_opts[@]}" -e "$regex" "${paths[@]}" | while read -r
            do
                printf "%s\n" "${REPLY#./}"
            done
        then
            return $matches_exit_code
        elif (( $? == 1 ))
        then
            return $no_matches_exit_code
        else
            return 2
        fi
    fi

    if (( $# == 0 ))
    then
        test "$mode" == "" || Fail "Missing REPL"
        if grep --color=auto -rE $opt_h $opt_i $opt_q "${grep_opts[@]}" -e "$regex" "${paths[@]}"
        then
            return $matches_exit_code
        elif (( $? == 1 ))
        then
            return $no_matches_exit_code
        else
            return 2
        fi
    fi

    test "$mode" != "" || mode=repl
    local repl="$1"
    shift
    (( $# == 0 )) || Fail "Too many arguments"

    if test "$sep" == ""
    then
        for sep in / , : % = + ^ '|' ';' '#' '&' '{' '}' '~' '<' '>' '*' $'\0'
        do
            [[ "$regex" == *"$sep"* ]] && continue
            [[ "$repl" == *"$sep"* ]] && continue
            break
        done
        test "$sep" != $'\0' || \
            Fail "No valid sed(1) separator found: Use -s SEP to override"
    else
        [[ "$regex" != *"$sep"* ]] || Fail "REGEX contains separator '$sep'"
        [[ "$repl" != *"$sep"* ]] || Fail "REPL contains separator '$sep'"
    fi

    # sed -E is documented in sed(1) to use the same regular expressions as grep
    # -E.
    local -a grep_cmd=(grep -rEl $opt_i "${grep_opts[@]}" -e "$regex" "${paths[@]}")
    if "${grep_cmd[@]}" | mapfile -t
    then
        :
    elif (( $? == 2 ))
    then
        return 2 # Invalid REGEX: grep should already have written to stderr
    fi

    local -i nfiles="${#MAPFILE[@]}"
    if (( nfiles == 0 ))
    then
        test -n "$opt_q" || echo "No files matched" >&2
        return $no_matches_exit_code
    elif test -n "$opt_q" && [[ "$mode" =~ diff|repl ]] # or editor, update
    then
        return $matches_exit_code # Optimization
    fi
    local -a files=("${MAPFILE[@]}")

    if test "$mode" == editor
    then
        test -x "$EDITOR" || Fail "EDITOR is not set"
        type patch &> /dev/null || Fail "Command not found: patch"
        declare -g PATCHFILE
        PATCHFILE=$(mktemp re.XXXXXX.diff) || Fail "Failed to create temporary patch file"
        trap 'rm "$PATCHFILE"' EXIT
    fi

    type sed &> /dev/null || Fail "Command not found: 'sed'"

    local with_filename
    case "$opt_h" in
        -H) with_filename=true ;;
        -h) with_filename=false ;;
        *)
            if (( ${#paths[@]} == 1 )) && test -f "${paths[0]}"
            then
                with_filename=false
            else
                with_filename=true
            fi
            ;;
    esac

    local file
    for file in "${files[@]}"
    do
        local expr="s$sep$regex$sep$repl$sep${opt_i:+i}"g
        case "$mode" in
            diff) diff -u --label "old/$file" --label "new/$file" "$file" <(sed -E "$expr" "$file") ;;
            editor) diff -u --label "old/$file" --label "new/$file" "$file" <(sed -E "$expr" "$file") >> "$PATCHFILE" ;;
            repl)
                if $with_filename
                then
                    grep -Eh $opt_i "$regex" "$file" | sed -E "$expr" | while read -r
                    do
                        printf "%s:%s\n" "$file" "$REPLY"
                    done
                else
                    grep -Eh $opt_i "$regex" "$file" | sed -E "$expr"
                fi
                ;;
            update) sed -E -i "$expr" "$file" ;;
        esac
    done

    case "$mode" in
        editor)
            if "$EDITOR" "$PATCHFILE" && test -s "$PATCHFILE"
            then
                patch -p1 < "$PATCHFILE"
            elif test -z "$opt_q"
            then
                echo "Patch aborted"
            fi
            ;;
        update)
            if test -z "$opt_q"
            then
                if (( nfiles == 1 ))
                then
                    echo "Updated 1 file"
                else
                    echo "Updated $nfiles files"
                fi
            fi
            ;;
    esac

    return $matches_exit_code
}

Main "$@"
