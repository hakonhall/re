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
  -f,--filename           Prefix each line with filename. Default with >1 file.
  -F,--no-filename        Suppress prefixing each line with filename.
  -i,--include GLOB       Same as grep(1) --include.
  -l,--list               List the matched files only.
  -p,--path PATH...       Run on PATH instead of '.'.  Repeatable.
  -q,--quiet              Avoid printing to stdout.
  -u,--update             Update the files in-place.
EOF

    exit 0
}

function Fail {
    echo "$1" >& 2
    exit 1
}

function Main {
    local mode="" sep="" fn_arg="" q_arg=""
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
            -f|--filename)
                fn_arg=-H # --with-filename
                shift
                ;;
            -F|--no-filename)
                fn_arg=-h # --no-filename
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
            -q|--quiet)
                q_arg=-q
                shift
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
        grep --color=auto -rEl -e "$regex" $fn_arg $q_arg "${grep_args[@]}" "${paths[@]}"
        return $?
    elif (( $# == 0 ))
    then
        test "$mode" == "" || Fail "Missing REPL"
        grep --color=auto -rE -e "$regex" $fn_arg $q_arg "${grep_args[@]}" "${paths[@]}"
        return $?
    fi

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
        if test "$mode" == update && test -z "$q_arg"
        then
            echo "0 files updated"
        fi
        return 1 # no matches => exit code 1 with grep, so too with replace?
    fi
    local -a files=("${MAPFILE[@]}")

    test "$mode" != "" || mode=repl

    if test "$q_arg" == -q
    then
        case "$mode" in
            repl|diff) return 0 ;;
        esac
    fi

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
    case "$fn_arg" in
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
        local expr="s$sep$regex$sep$repl${sep}g"
        case "$mode" in
            diff) diff -u "$file" <(sed -E "$expr" "$file") ;;
            editor) diff -u "$file" <(sed -E "$expr" "$file") >> "$PATCHFILE" ;;
            repl)
                if $with_filename
                then
                    grep -Eh "$regex" "$file" | sed -E "$expr" | while read -r
                    do
                        printf "%s:%s\n" "$file" "$REPLY"
                    done
                else
                    grep -Eh "$regex" "$file" | sed -E "$expr"
                fi
                ;;
            update) sed -E -i "$expr" "$file" ;;
        esac
    done

    case "$mode" in
        editor)
            if "$EDITOR" "$PATCHFILE" && test -s "$PATCHFILE"
            then
                patch -p0 < "$PATCHFILE"
            elif test -z "$q_arg"
            then
                echo "Patch aborted"
            fi
            ;;
        update)
            if test -z "$q_arg"
            then
                local -i nfiles="${#files[@]}"
                if (( nfiles == 1 ))
                then
                    echo "1 file updated"
                else
                    echo "$nfiles files updated"
                fi
            fi
            ;;
    esac

    return 0
}

Main "$@"
