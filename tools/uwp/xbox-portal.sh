#!/usr/bin/env bash
# Talks to an Xbox in Developer Mode through its Windows Device Portal (HTTPS on port 11443), for
# xbox-run.sh, xbox-push-data.sh and xbox-lba2cc.py. Any script answering the same commands can
# stand in for it (XBOX_SH); docs/plan/XBOX_PLAN.md lists them.
#
#   xbox-portal.sh trust                     pin the portal's TLS key (first use, or after the console
#                                            makes a new certificate); shows the key before saving it
#   xbox-portal.sh apps                      installed packages: full name, a tab, the app id
#   xbox-portal.sh deploy DIR                copy an unpacked app (a folder with AppxManifest.xml) to
#                                            DevelopmentFiles\LooseApps\<folder name> and register it
#   xbox-portal.sh launch FULLNAME           start an installed package
#   xbox-portal.sh screenshot OUT            the TV picture as a PNG
#   xbox-portal.sh api METHOD PATH [curl args...]   any other portal request; the body to stdout
#
# XBOX_HOST              the console's address (Dev Home shows it)
# XBOX_PORTAL_USER       the portal login set in Dev Home's Remote Access settings, or a netrc file
# XBOX_PORTAL_PASSWORD   (XBOX_PORTAL_NETRC, default ~/.config/lba2cc/xbox-portal.netrc) with a line
#                        "machine <XBOX_HOST> login <user> password <password>"
# XBOX_PORTAL_PIN_FILE   where `trust` keeps the key (default ~/.config/lba2cc/xbox-portal.pin)
# XBOX_TIMEOUT           curl's --max-time in seconds (default 60)
#
# The portal's certificate comes from the console's own authority, which the computer does not
# trust, so its public key is pinned instead; a changed key fails closed. The login reaches curl
# through a config on stdin, so it never shows in `ps`.
set -euo pipefail

die() { echo "xbox-portal.sh: $*" >&2; exit 1; }

host="${XBOX_HOST:-}"
base="https://$host:11443"
pinfile="${XBOX_PORTAL_PIN_FILE:-$HOME/.config/lba2cc/xbox-portal.pin}"
netrc="${XBOX_PORTAL_NETRC:-$HOME/.config/lba2cc/xbox-portal.netrc}"
# macOS lets the system curl reach the local network; a Homebrew one may be blocked by its
# Local Network privacy setting.
curl=/usr/bin/curl
[ -x "$curl" ] || curl="$(command -v curl)" || die "no curl"

# A value for a double-quoted string in a curl config.
cfg_quote() { local v="${1//\\/\\\\}"; printf '"%s"' "${v//\"/\\\"}"; }
b64() { printf '%s' "$1" | base64 | tr -d '\n'; }
urlenc() { python3 -I -c 'import sys, urllib.parse; print(urllib.parse.quote(sys.argv[1], safe=""))' "$1"; }

need_host() { [ -n "$host" ] || die "set XBOX_HOST to the console's address"; }

pin() {
  [ -s "$pinfile" ] || die "no pinned key in $pinfile: run  xbox-portal.sh trust"
  cat "$pinfile"
}

login_config() {
  if [ -n "${XBOX_PORTAL_USER:-}" ] && [ -n "${XBOX_PORTAL_PASSWORD:-}" ]; then
    printf 'user = %s\n' "$(cfg_quote "$XBOX_PORTAL_USER:$XBOX_PORTAL_PASSWORD")"
  elif [ -s "$netrc" ]; then
    printf 'netrc-file = %s\n' "$(cfg_quote "$netrc")"
  else
    die "no portal login: set XBOX_PORTAL_USER and XBOX_PORTAL_PASSWORD, or write $netrc"
  fi
}

# One cookie jar for the run: it carries the portal's CSRF token.
jar="$(mktemp "${TMPDIR:-/tmp}/xbox-portal.XXXXXX")"
trap 'rm -f "$jar"' EXIT

# call METHOD PATH [curl args...]: one authenticated request. Writes need the portal's CSRF
# token: a first GET sets it as a cookie, and it goes back as the X-CSRF-Token header.
call() {
  need_host
  local method="$1" path="$2"; shift 2
  local p conf csrf
  p="$(pin)"
  conf="$(login_config)"
  if [ ! -s "$jar" ]; then
    printf '%s\n' "$conf" | "$curl" -sS -k --pinnedpubkey "sha256//$p" --max-time 20 -K - \
      -c "$jar" -o /dev/null "$base/api/os/machinename" \
      || die "cannot reach the portal at $base (curl exit $?; 90 means the key changed: see  xbox-portal.sh trust)"
  fi
  csrf="$(awk '$6 == "CSRF-Token" { print $7 }' "$jar" | tail -1)"
  printf '%s\nheader = %s\n' "$conf" "$(cfg_quote "X-CSRF-Token: $csrf")" \
    | "$curl" -sS -k --pinnedpubkey "sha256//$p" --max-time "${XBOX_TIMEOUT:-60}" -K - \
        -b "$jar" -c "$jar" --fail-with-body -X "$method" "$base/${path#/}" "$@"
}

cmd_trust() {
  need_host
  local pem cert key a
  pem="$("$curl" -sk --max-time 10 -o /dev/null -w '%{certs}' "$base/")" || die "cannot reach $base"
  cert="$(printf '%s\n' "$pem" | awk '/BEGIN CERT/,/END CERT/ { print } /END CERT/ { exit }')"
  [ -n "$cert" ] || die "the portal sent no certificate"
  key="$(printf '%s\n' "$cert" | openssl x509 -pubkey -noout | openssl pkey -pubin -outform der 2>/dev/null \
    | openssl dgst -sha256 -binary | base64)" || key=""
  [ -n "$key" ] || die "could not read the certificate's key"
  printf '%s\n' "$cert" | openssl x509 -noout -subject -issuer -dates
  echo "key sha256: $key"
  if [ -s "$pinfile" ] && [ "$(cat "$pinfile")" = "$key" ]; then echo "already pinned"; return; fi
  [ -s "$pinfile" ] && echo "was:        $(cat "$pinfile")"
  read -r -p "Pin this key for $host? [y/N] " a
  [ "$a" = y ] || [ "$a" = Y ] || die "not pinned"
  mkdir -p "$(dirname "$pinfile")"
  printf '%s\n' "$key" > "$pinfile"
  echo "pinned in $pinfile"
}

cmd_apps() {
  call GET /api/app/packagemanager/packages | python3 -I -c '
import json, sys
for p in sorted(json.load(sys.stdin).get("InstalledPackages", []), key=lambda p: p.get("Name", "")):
    print(p.get("PackageFullName", "") + "\t" + p.get("PackageRelativeId", ""))'
}

# The portal answers 204 while a deployment runs, then 200 with the outcome, or an error.
wait_deploy() {
  local out code
  for _ in $(seq 1 300); do
    out="$(call GET /api/app/packagemanager/state -w '\n%{http_code}' 2>/dev/null || true)"
    code="${out##*$'\n'}"
    if [ "$code" = 200 ]; then echo registered; return; fi
    if [ "$code" != 204 ]; then printf '%s\n' "${out%$'\n'*}" >&2; die "deployment failed (HTTP $code)"; fi
    sleep 2
  done
  die "no result after 10 minutes; check the console"
}

# Files are overwritten, never deleted: for a clean copy, uninstall and remove the folder first.
cmd_deploy() {
  local dir="${1:?usage: xbox-portal.sh deploy DIR}"
  [ -f "$dir/AppxManifest.xml" ] || die "no AppxManifest.xml in $dir"
  dir="$(cd "$dir" && pwd)"
  local name sub rel parent n=0 files f
  name="$(basename "$dir")"
  local devq="knownfolderid=DevelopmentFiles"
  # Folders first, parents before children. Creating one that exists is harmless.
  call POST "/api/filesystem/apps/folder?$devq&path=$(urlenc "\\")&newfoldername=LooseApps" -d '' -o /dev/null 2>/dev/null || true
  call POST "/api/filesystem/apps/folder?$devq&path=$(urlenc '\LooseApps')&newfoldername=$(urlenc "$name")" -d '' -o /dev/null 2>/dev/null || true
  while IFS= read -r sub; do
    rel="${sub#"$dir"/}"; parent="$(dirname "$rel")"; [ "$parent" = . ] && parent="" || parent="\\${parent//\//\\}"
    call POST "/api/filesystem/apps/folder?$devq&path=$(urlenc "\\LooseApps\\$name$parent")&newfoldername=$(urlenc "$(basename "$rel")")" -d '' -o /dev/null
  done < <(find "$dir" -mindepth 1 -type d ! -name '.*' | sort)
  # Then each folder's files, one upload per folder.
  while IFS= read -r sub; do
    files=()
    while IFS= read -r f; do files+=(-F "f$((n += 1))=@$f"); done < <(find "$sub" -maxdepth 1 -type f ! -name '.*' | sort)
    [ "${#files[@]}" -gt 0 ] || continue
    rel="${sub#"$dir"}"; rel="${rel//\//\\}"
    XBOX_TIMEOUT=900 call POST "/api/filesystem/apps/file?$devq&path=$(urlenc "\\LooseApps\\$name$rel")" "${files[@]}" -o /dev/null
  done < <(find "$dir" -type d ! -name '.*' | sort)
  echo "copied $n files to DevelopmentFiles\\LooseApps\\$name"
  call POST "/api/app/packagemanager/register?folder=$(urlenc "$(b64 "$name")")" -d '' -o /dev/null
  wait_deploy
}

cmd_launch() {
  local full="${1:?usage: xbox-portal.sh launch FULLNAME}" praid
  praid="$(cmd_apps | awk -F'\t' -v f="$full" '$1 == f { print $2 }')"
  [ -n "$praid" ] || die "no installed package $full (see  xbox-portal.sh apps)"
  # A POST with no body must still say so (Content-Length: 0), or the portal answers 411.
  call POST "/api/taskmanager/app?appid=$(urlenc "$(b64 "$praid")")&package=$(urlenc "$(b64 "$full")")" -d '' -o /dev/null
  echo "started $praid"
}

cmd_screenshot() {
  local out="${1:?usage: xbox-portal.sh screenshot OUT}"
  call GET /ext/screenshot -o "$out"
  echo "$out"
}

case "${1:-}" in
  trust)      cmd_trust ;;
  apps)       cmd_apps ;;
  deploy)     shift; cmd_deploy "$@" ;;
  launch)     shift; cmd_launch "$@" ;;
  screenshot) shift; cmd_screenshot "$@" ;;
  api)        shift; m="${1:?usage: xbox-portal.sh api METHOD PATH}"; p="${2:?usage: xbox-portal.sh api METHOD PATH}"; shift 2; call "$m" "$p" "$@"; echo ;;
  *)          sed -n '2,/^set -euo/p' "$0" | sed '$d; s/^# \{0,1\}//'; exit 2 ;;
esac
