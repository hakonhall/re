#!/bin/bash

shopt -s lastpipe
set -o pipefail

function Usage {
    cat <<'EOF'
Usage: re [OPTION...] [--] REGEX [REPL]
Runs recursive egrep, optionally replacing matches with REPL.

With REPL, all matches of the extended regular expression REGEX in the text
files below the current working directory are replaced by REPL.  REPL may
contain the special character & to refer to the matched text, and the special
escapes \1 through \9 to refer to the corresponding matching sub-expressions.

Options:
  -d,--diff               View the replacement as a unified diff(1) patch.
  -e,--editor             Open EDITOR to edit the patch before it is applied.
  -x,--exclude GLOB       Same as grep(1) --exclude.
  -X,--exclude-dir GLOB   Same as grep(1) --exclude-dir.
  -i,--include GLOB       Same as grep(1) --include.
  -l,--list               List the matched files only.
  -p,--path PATH...       Run on PATH instead of '.'.  Repeatable.
  -u,--update             Update the files in-place.
EOF

    exit 0
}

function Fail {
    echo "$1" >& 2
    exit 1
}

function Main {
    local mode=""
    local sep=""
    local -a paths=()
    local -a grep_args=(-I)

    while (( $# > 0 ))
    do
        case "$1" in
            --)
                shift
                break
                ;;
            -d|--diff)
                mode=diff
                shift
                ;;
            -e|--editor)
                mode=editor
                shift
                ;;
            -x|--exclude)
                grep_args+=(--exclude "$2")
                shift 2 || Fail "Missing argument to $2"
                ;;
            -X|--exclude-dir)
                grep_args+=(--exclude-dir "$2")
                shift 2 || Fail "Missing argument to $2"
                ;;
            -h|--help) Usage ;;
            -i|--include)
                grep_args+=(--include "$2")
                shift 2 || Fail "Missing argument to $2"
                ;;
            -l|--list)
                mode=list
                shift
                ;;
            -p|--path)
                paths+=("$2")
                shift 2 || Fail "Missing argument to $2"
                ;;
            -s|--separator)
                # Undocumented option for use in emergencies.
                sep="$2"
                (( ${#sep} == 1 )) || Fail "Invalid separator: '$sep'"
                shift 2 || Fail "Missing argument to $2"
                ;;
            -u|--update)
                mode=update
                shift
                ;;
            -*) Fail "Unknown option: '$1'" ;;
            *) break ;;
        esac
    done

    (( $# >= 1 )) || Fail "Missing REGEX, see -h for help"
    local regex="$1"
    shift

    type grep &> /dev/null || Fail "Command not found: 'grep'"

    if test "$mode" == list
    then
        grep -rEl -e "$regex" "${grep_args[@]}" "${paths[@]}"
        return $?
    elif (( $# == 0 ))
    then
        test "$mode" == "" || Fail "Missing REPL"
        grep -rE -e "$regex" "${grep_args[@]}" "${paths[@]}"
        return $?
    fi

    local repl="$1"
    shift
    (( $# == 0 )) || Fail "Too many arguments"

    if test "$sep" == ""
    then
        local _sep
        for _sep in / , : % = + ^ '|' ';' '#' '&' '{' '}' '~' '<' '>' '*'
        do
            [[ "$regex" == *"$_sep"* ]] && continue
            [[ "$repl" == *"$_sep"* ]] && continue
            sep="$_sep"
            break
        done
        test "$sep" != "" || \
            Fail "No valid sed(1) separator found: Use -s SEP to override"
    else
        [[ "$regex" != *"$sep"* ]] || Fail "REGEX contains separator '$sep'"
        [[ "$repl" != *"$sep"* ]] || Fail "REPL contains separator '$sep'"
    fi

    # sed -E is documented in sed(1) to use the same regular expressions as grep
    # -E.
    local -a grep_cmd=(grep -rEl -e "$regex" "${grep_args[@]}" "${paths[@]}")
    if "${grep_cmd[@]}" | mapfile -t
    then
        :
    elif (( $? == 2 ))
    then
        return 2 # Invalid REGEX: grep should already have written to stderr
    fi

    if (( ${#MAPFILE[@]} == 0 ))
    then
        if test "$mode" == update
        then
            echo "0 files updated"
        fi
        return 0
    fi
    local -a files=("${MAPFILE[@]}")

    test "$mode" != "" || mode=repl
    if test "$mode" == editor
    then
        test -x "$EDITOR" || Fail "EDITOR is not set"
        type patch &> /dev/null || Fail "Command not found: patch"
        declare -g PATCHFILE
        PATCHFILE=$(mktemp re.XXXXXX.diff) || Fail "Failed to create temporary patch file"
        trap 'rm "$PATCHFILE"' EXIT
    fi

    type sed &> /dev/null || Fail "Command not found: 'sed'"

    local file
    for file in "${files[@]}"
    do
        local expr="s$sep$regex$sep$repl${sep}g"
        case "$mode" in
            diff) diff -u "$file" <(sed -E "$expr" "$file") ;;
            editor) diff -u "$file" <(sed -E "$expr" "$file") >> "$PATCHFILE" ;;
            repl)
                local content
                content=$(grep -Eh "$regex" "$file" | sed -E "$expr")
                printf "%s:%s\n" "$file" "$content"
                ;;
            update) sed -E -i "$expr" "$file"
        esac
    done

    case "$mode" in
        editor)
            if "$EDITOR" "$PATCHFILE" && test -s "$PATCHFILE"
            then
                patch -p0 < "$PATCHFILE"
            else
                echo "Patch aborted"
            fi
            ;;
        update)
            local -i nfiles="${#files[@]}"
            if (( nfiles == 1 ))
            then
                echo "1 file updated"
            else
                echo "$nfiles files updated"
            fi
            ;;
    esac
}

Main "$@"
