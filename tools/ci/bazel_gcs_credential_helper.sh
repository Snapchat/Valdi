#!/usr/bin/env bash
# Bazel credential helper for the GCS remote cache used by CI.
#
# Bazel invokes this on every remote-cache request (subject to its `expires`
# caching), so a single long-running `bazel build` can rotate auth tokens
# mid-build instead of being stuck with the bearer set at startup.
#
# Spec: https://github.com/EngFlow/credential-helper-spec
#   Invocation: `<helper> get` with `{"uri": "https://..."}` on stdin
#   Response:   `{"headers": {"Authorization": ["Bearer ..."]}, "expires": "..."}`
#
# Implements the GitHub-OIDC → Google-STS → optional SA-impersonation chain
# directly with curl + jq, so we don't depend on `gcloud` being installed on
# the runner (notably, macos-latest GitHub-hosted runners no longer ship the
# Google Cloud SDK preinstalled, and `google-github-actions/auth@v2` only
# writes the WIF credential file — it doesn't install gcloud).
#
# Auth source: `google-github-actions/auth@v2` writes the external_account
# credential file to $GOOGLE_APPLICATION_CREDENTIALS. GitHub's OIDC token
# endpoint is reachable via ACTIONS_ID_TOKEN_REQUEST_{URL,TOKEN} (set by the
# runner when `id-token: write` permission is granted).

set -euo pipefail

LOG_FILE="${BAZEL_GCS_CRED_HELPER_LOG:-/tmp/bazel-cred-helper.log}"
log() {
  printf '[%s] %s\n' "$(date -u +'%Y-%m-%dT%H:%M:%SZ')" "$*" >> "$LOG_FILE" 2>/dev/null || true
}

emit_empty() {
  printf '{"headers": {}}\n'
}

if [ "${1:-}" != "get" ]; then
  exit 0
fi

# Drain stdin — we only ever handle storage.googleapis.com so the URI is
# informational, but Bazel will hang if we don't consume it.
cat >/dev/null

CREDS_FILE="${GOOGLE_APPLICATION_CREDENTIALS:-}"
if [ -z "$CREDS_FILE" ] || [ ! -f "$CREDS_FILE" ]; then
  # Fork-PR / no-creds path: emit empty headers so Bazel falls back to
  # building without remote cache, same as the pre-auth behaviour.
  log "no GOOGLE_APPLICATION_CREDENTIALS — emitting empty headers"
  emit_empty
  exit 0
fi

if ! command -v jq >/dev/null 2>&1; then
  log "FATAL: jq not on PATH (PATH=$PATH)"
  exit 1
fi
if ! command -v curl >/dev/null 2>&1; then
  log "FATAL: curl not on PATH (PATH=$PATH)"
  exit 1
fi

AUDIENCE="$(jq -r '.audience // empty' "$CREDS_FILE")"
TOKEN_URL="$(jq -r '.token_url // empty' "$CREDS_FILE")"
SUBJECT_TOKEN_TYPE="$(jq -r '.subject_token_type // empty' "$CREDS_FILE")"
IMPERSONATION_URL="$(jq -r '.service_account_impersonation_url // empty' "$CREDS_FILE")"

if [ -z "$AUDIENCE" ] || [ -z "$TOKEN_URL" ] || [ -z "$SUBJECT_TOKEN_TYPE" ]; then
  log "FATAL: credentials file at $CREDS_FILE missing required external_account fields"
  exit 1
fi

OIDC_TOKEN="${ACTIONS_ID_TOKEN_REQUEST_TOKEN:-}"
OIDC_URL="${ACTIONS_ID_TOKEN_REQUEST_URL:-}"
if [ -z "$OIDC_TOKEN" ] || [ -z "$OIDC_URL" ]; then
  log "FATAL: ACTIONS_ID_TOKEN_REQUEST_{TOKEN,URL} unset; cannot mint OIDC subject token"
  exit 1
fi

ENCODED_AUDIENCE="$(printf '%s' "$AUDIENCE" | jq -sRr @uri)"
# ACTIONS_ID_TOKEN_REQUEST_URL already contains a query string
# (e.g. ?api-version=...), so append with &.
OIDC_RESPONSE="$(curl -sS \
  -H "Authorization: Bearer $OIDC_TOKEN" \
  -H "Accept: application/json; api-version=2.0" \
  "${OIDC_URL}&audience=${ENCODED_AUDIENCE}" 2>>"$LOG_FILE")"

SUBJECT_TOKEN="$(printf '%s' "$OIDC_RESPONSE" | jq -r '.value // empty')"
if [ -z "$SUBJECT_TOKEN" ]; then
  log "FATAL: GitHub OIDC token fetch returned no .value; response: $OIDC_RESPONSE"
  exit 1
fi
log "got GitHub OIDC subject token (len=${#SUBJECT_TOKEN})"

STS_RESPONSE="$(curl -sS -X POST "$TOKEN_URL" \
  -H "Content-Type: application/x-www-form-urlencoded" \
  --data-urlencode "audience=$AUDIENCE" \
  --data-urlencode "grant_type=urn:ietf:params:oauth:grant-type:token-exchange" \
  --data-urlencode "requested_token_type=urn:ietf:params:oauth:token-type:access_token" \
  --data-urlencode "scope=https://www.googleapis.com/auth/cloud-platform" \
  --data-urlencode "subject_token_type=$SUBJECT_TOKEN_TYPE" \
  --data-urlencode "subject_token=$SUBJECT_TOKEN" 2>>"$LOG_FILE")"

FEDERATED_TOKEN="$(printf '%s' "$STS_RESPONSE" | jq -r '.access_token // empty')"
if [ -z "$FEDERATED_TOKEN" ]; then
  log "FATAL: Google STS exchange returned no access_token; response: $STS_RESPONSE"
  exit 1
fi

if [ -n "$IMPERSONATION_URL" ]; then
  IMP_RESPONSE="$(curl -sS -X POST "$IMPERSONATION_URL" \
    -H "Authorization: Bearer $FEDERATED_TOKEN" \
    -H "Content-Type: application/json" \
    --data '{"scope":["https://www.googleapis.com/auth/cloud-platform"]}' 2>>"$LOG_FILE")"

  ACCESS_TOKEN="$(printf '%s' "$IMP_RESPONSE" | jq -r '.accessToken // empty')"
  EXPIRES="$(printf '%s' "$IMP_RESPONSE" | jq -r '.expireTime // empty')"
  if [ -z "$ACCESS_TOKEN" ] || [ -z "$EXPIRES" ]; then
    log "FATAL: SA impersonation returned no accessToken/expireTime; response: $IMP_RESPONSE"
    exit 1
  fi
  log "minted impersonated access token, expires=$EXPIRES"
else
  ACCESS_TOKEN="$FEDERATED_TOKEN"
  EXPIRES_IN="$(printf '%s' "$STS_RESPONSE" | jq -r '.expires_in // 3600')"
  EXPIRES="$(date -u -v+"${EXPIRES_IN}"S +'%Y-%m-%dT%H:%M:%SZ' 2>/dev/null \
    || date -u -d "+${EXPIRES_IN} seconds" +'%Y-%m-%dT%H:%M:%SZ')"
  log "minted federated access token (no SA impersonation), expires=$EXPIRES"
fi

jq -nc \
  --arg token "$ACCESS_TOKEN" \
  --arg expires "$EXPIRES" \
  '{headers: {Authorization: ["Bearer " + $token]}, expires: $expires}'
