#!/usr/bin/env bash
# Non-interactive tests: pipes commands into the shell and checks its output.
# Interactive features (TAB, arrow keys, Ctrl-C/Z/D) need a real terminal and
# are checked by hand.
set -u
export LC_ALL=C

BIN="$(cd "$(dirname "$0")/.." && pwd)/main"
WORK="$(mktemp -d)"
WORK="$(cd "$WORK" && pwd -P)"
export HOME="$WORK/userhome"
mkdir -p "$HOME"
cd "$WORK" || exit 1

mkdir -p dir1/sub dir2
touch alpha.txt alnum.txt dir1/sub/needle.txt dir2/.hidden
printf 'line3\nline1\nline2\n' > in.txt
ln -s alpha.txt link.txt

fail=0
pass() { echo "PASS: $1"; }
bad()  { echo "FAIL: $1"; echo "  expected: $(printf %q "$2")"; echo "  actual:   $(printf %q "$3")"; fail=1; }
check()    { if [ "$2" == "$3" ]; then pass "$1"; else bad "$1" "$2" "$3"; fi; }
contains() { if [[ "$3" == *"$2"* ]]; then pass "$1"; else bad "$1" "*$2*" "$3"; fi; }
run() { printf '%s\n' "$@" | "$BIN" 2>&1; }

# cd / pwd / echo
check    "echo collapses spaces"   "hello world"           "$(run 'echo   hello 	   world')"
check    "pwd"                     "$WORK"                 "$(run 'pwd')"
check    "cd .."                   "$(dirname "$WORK")"    "$(run 'cd ..' 'pwd')"
check    "cd ~ returns to start"   "$WORK"                 "$(run 'cd dir1' 'cd ~' 'pwd')"
check    "cd with no args"         "$WORK"                 "$(run 'cd dir1/sub' 'cd' 'pwd')"
check    "cd -"                    "$WORK"                 "$(run 'cd dir1' 'cd -')"
check    "cd too many args"        "Invalid arguments"     "$(run 'cd a b')"
contains "cd missing dir"          "No such file"          "$(run 'cd nowhere')"
check    "semicolons"              $'a\nb\nc'              "$(run 'echo a ; echo b;echo c')"

# ls (compared against the system ls)
check    "ls"                      "$(ls)"                 "$(run 'ls')"
check    "ls -a"                   "$(ls -a)"              "$(run 'ls -a')"
check    "ls -l"                   "$(ls -l)"              "$(run 'ls -l')"
check    "ls -la dir"              "$(ls -la dir1)"        "$(run 'ls -la dir1')"
check    "ls -l -a dir"            "$(ls -l -a dir2)"      "$(run 'ls -l dir2 -a')"
check    "ls ~"                    "$(ls)"                 "$(run 'cd dir1' 'ls ~')"
check    "ls two dirs"             "$(ls dir1 dir2)"       "$(run 'ls dir1 dir2')"
check    "ls file"                 "$(ls -l alpha.txt)"    "$(run 'ls -l alpha.txt')"
contains "ls missing"              "cannot access"         "$(run 'ls nope')"

# search
check    "search nested file"      "True"                  "$(run 'search needle.txt')"
check    "search folder"           "True"                  "$(run 'search sub')"
check    "search missing"          "False"                 "$(run 'search nothere.txt')"

# redirection
run 'echo hi > out.txt' >/dev/null
check    "> creates file"          "hi"                    "$(cat out.txt)"
check    "> uses mode 0644"        "644"                   "$(stat -c %a out.txt)"
run 'echo there >> out.txt' >/dev/null
check    ">> appends"              $'hi\nthere'            "$(cat out.txt)"
run 'echo new > out.txt' >/dev/null
check    "> truncates"             "new"                   "$(cat out.txt)"
check    "< input"                 $'line1\nline2\nline3'  "$(run 'sort < in.txt')"
run 'sort < in.txt > sorted.txt' >/dev/null
check    "< and > together"        $'line1\nline2\nline3'  "$(cat sorted.txt)"
run 'echo x>nospace.txt' >/dev/null
check    "> without spaces"        "x"                     "$(cat nospace.txt)"
contains "missing input file"      "does not exist"        "$(run 'cat < missing.txt')"

# pipes
check    "2-stage pipe"            "3"                     "$(run 'cat in.txt | wc -l' | tr -d ' ')"
check    "3-stage pipe"            $'line1\nline2'         "$(run 'cat in.txt | sort | head -2')"
check    "built-in in pipe"        "2"                     "$(run 'ls | grep al | wc -l' | tr -d ' ')"
check    "echo into pipe"          "HELLO"                 "$(run 'echo hello | tr a-z A-Z')"
run 'ls | grep ".txt" > txts.txt; cat < in.txt | wc -l > lines.txt' >/dev/null
contains "pipe + redirect (spec)"  "alpha.txt"             "$(cat txts.txt)"
check    "pipe + redirect (spec)"  "3"                     "$(tr -d ' ' < lines.txt)"

# system commands, background
check    "external command"        "external"              "$(run 'printf external')"
contains "unknown command"         "command not found"     "$(run 'nosuchcmd123')"
out="$(run 'sleep 0.1 &')"
if [[ "$out" =~ ^\[1\]\ [0-9]+$ ]]; then pass "background prints pid"; else bad "background prints pid" "[1] <pid>" "$out"; fi
contains "background job reaped"   "exited normally"       "$(run 'sleep 0.1 &' 'sleep 0.3' 'echo done')"

# pinfo
contains "pinfo self"              "Process Status -- "    "$(run 'pinfo')"
contains "pinfo pid"               "Executable Path -- /usr/bin/sleep" "$(sleep 2 & p=$!; run "pinfo $p"; kill $p)"
contains "pinfo bad pid"           "does not exist"        "$(run 'pinfo 999999')"

# history (persists across sessions, max 20, consecutive duplicates skipped)
rm -f "$HOME/.my_shell_history"
check    "history"                 $'pwd\ncd\nhistory'    "$(run 'pwd' 'cd' 'cd' 'history' | tail -n 3)"
check    "history across sessions" $'history\nexit\nhistory 3' "$(run 'exit' >/dev/null; run 'history 3')"
seq 1 30 | sed 's/^/echo /' | "$BIN" >/dev/null 2>&1
check    "history keeps 20"        "20"                    "$(wc -l < "$HOME/.my_shell_history" | tr -d ' ')"
check    "history default 10"      "10"                    "$(run 'history' | wc -l | tr -d ' ')"

# exit
check    "exit stops the shell"    "a"                     "$(run 'echo a' 'exit' 'echo b')"

echo
if [ $fail -eq 0 ]; then echo "All tests passed"; else echo "Some tests FAILED"; fi
rm -rf "$WORK"
exit $fail
