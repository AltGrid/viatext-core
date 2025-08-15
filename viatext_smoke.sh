#!/usr/bin/env bash
set -u

NODE="${1:-N3}"
CLI="./viatext-cli"
CLI_OPTS=(--baud 115200 --timeout 2500 --boot-delay 600)  # longer I/O cushions

GREEN='\033[0;32m'; RED='\033[0;31m'; YEL='\033[0;33m'; NC='\033[0m'
pass(){ echo -e "${GREEN}PASS${NC}  $*"; }
fail(){ echo -e "${RED}FAIL${NC}  $*"; }

# run <label> [--post-sleep-ms N] -- <cmd...> --expect <substr...>
run() {
  local label="$1"; shift
  local post_ms=200
  if [[ "${1:-}" == "--post-sleep-ms" ]]; then post_ms="$2"; shift 2; fi
  [[ "$1" == "--" ]] && shift
  local cmd=()
  while [[ $# -gt 0 && "$1" != "--expect" ]]; do cmd+=("$1"); shift; done
  shift # --expect
  local expect=()
  while [[ $# -gt 0 ]]; do expect+=("$1"); shift; done

  # one retry if expectations aren’t met
  local attempt out rc ok=0
  for attempt in 1 2; do
    out=$("${cmd[@]}" 2>&1); rc=$?
    if [[ $rc -eq 0 ]]; then
      ok=1
      for s in "${expect[@]}"; do
        if ! grep -Fq "$s" <<<"$out"; then ok=0; break; fi
      done
    fi
    if [[ $ok -eq 1 ]]; then break; fi
    sleep 0.5  # small backoff before retry
  done

  if [[ $ok -eq 1 ]]; then
    pass "$label"
    echo "$out" | sed 's/^/    /'
  else
    fail "$label"
    echo "$out"
  fi

  # settle between commands
  sleep "$(awk "BEGIN{printf \"%.3f\", $post_ms/1000}")"
}

echo -e "${YEL}=== Discovery / Targeting ===${NC}"
run "scan" -- ${CLI} --scan                                 --expect "id=" "dev="
run "ping (legacy)" -- ${CLI} "${CLI_OPTS[@]}" --ping --node "$NODE"  --expect "status=ok"
run "get-id (legacy)" -- ${CLI} "${CLI_OPTS[@]}" --get-id --node "$NODE" --expect "status=ok" "id="

echo -e "${YEL}=== ID / Alias ===${NC}"
run "get id" -- ${CLI} "${CLI_OPTS[@]}" --get id --node "$NODE"       --expect "id="
run "set alias=TestNode" --post-sleep-ms 600 -- \
    ${CLI} "${CLI_OPTS[@]}" --set alias TestNode --node "$NODE"       --expect "alias=TestNode"
run "get alias" -- ${CLI} "${CLI_OPTS[@]}" --get alias --node "$NODE" --expect "alias=TestNode"

echo -e "${YEL}=== Radio Config ===${NC}"
run "set freq=920000000" --post-sleep-ms 600 -- \
    ${CLI} "${CLI_OPTS[@]}" --set freq 920000000 --node "$NODE"       --expect "freq_hz=920000000"
run "get freq" -- ${CLI} "${CLI_OPTS[@]}" --get freq --node "$NODE"   --expect "freq_hz=920000000"

run "set sf=9" --post-sleep-ms 500 -- \
    ${CLI} "${CLI_OPTS[@]}" --set sf 9 --node "$NODE"                 --expect "sf=9"
run "get sf" -- ${CLI} "${CLI_OPTS[@]}" --get sf --node "$NODE"       --expect "sf=9"

run "set bw=125000" --post-sleep-ms 500 -- \
    ${CLI} "${CLI_OPTS[@]}" --set bw 125000 --node "$NODE"            --expect "bw_hz=125000"
run "get bw" -- ${CLI} "${CLI_OPTS[@]}" --get bw --node "$NODE"       --expect "bw_hz=125000"

run "set cr=5 (4/5)" --post-sleep-ms 500 -- \
    ${CLI} "${CLI_OPTS[@]}" --set cr 5 --node "$NODE"                 --expect "cr=4/5"
run "get cr" -- ${CLI} "${CLI_OPTS[@]}" --get cr --node "$NODE"       --expect "cr=4/5"

run "set tx_pwr=17" --post-sleep-ms 500 -- \
    ${CLI} "${CLI_OPTS[@]}" --set tx_pwr 17 --node "$NODE"            --expect "tx_pwr_dbm=17"
run "get tx_pwr" -- ${CLI} "${CLI_OPTS[@]}" --get tx_pwr --node "$NODE" --expect "tx_pwr_dbm=17"

run "set chan=1" --post-sleep-ms 500 -- \
    ${CLI} "${CLI_OPTS[@]}" --set chan 1 --node "$NODE"               --expect "chan=1"
run "get chan" -- ${CLI} "${CLI_OPTS[@]}" --get chan --node "$NODE"   --expect "chan=1"

echo -e "${YEL}=== Behavior ===${NC}"
run "set mode=0" --post-sleep-ms 500 -- \
    ${CLI} "${CLI_OPTS[@]}" --set mode 0 --node "$NODE"               --expect "mode=0"
run "get mode" -- ${CLI} "${CLI_OPTS[@]}" --get mode --node "$NODE"   --expect "mode=0"

run "set hops=2" --post-sleep-ms 500 -- \
    ${CLI} "${CLI_OPTS[@]}" --set hops 2 --node "$NODE"               --expect "hops=2"
run "get hops" -- ${CLI} "${CLI_OPTS[@]}" --get hops --node "$NODE"   --expect "hops=2"

run "set beacon_s=30" --post-sleep-ms 600 -- \
    ${CLI} "${CLI_OPTS[@]}" --set beacon 30 --node "$NODE"            --expect "beacon_s=30"
run "get beacon_s" -- ${CLI} "${CLI_OPTS[@]}" --get beacon --node "$NODE" --expect "beacon_s=30"

run "set buf_size=64" --post-sleep-ms 600 -- \
    ${CLI} "${CLI_OPTS[@]}" --set buf_size 64 --node "$NODE"          --expect "buf_size=64"
run "get buf_size" -- ${CLI} "${CLI_OPTS[@]}" --get buf_size --node "$NODE" --expect "buf_size=64"

run "set ack=1" --post-sleep-ms 500 -- \
    ${CLI} "${CLI_OPTS[@]}" --set ack 1 --node "$NODE"                --expect "ack=1"
run "get ack" -- ${CLI} "${CLI_OPTS[@]}" --get ack --node "$NODE"     --expect "ack=1"

echo -e "${YEL}=== Diagnostics (RO) ===${NC}"
run "get rssi"       -- ${CLI} "${CLI_OPTS[@]}" --get rssi --node "$NODE"       --expect "rssi_dbm="
run "get snr"        -- ${CLI} "${CLI_OPTS[@]}" --get snr  --node "$NODE"       --expect "snr_db="
run "get vbat"       -- ${CLI} "${CLI_OPTS[@]}" --get vbat --node "$NODE"       --expect "vbat_mv="
run "get temp"       -- ${CLI} "${CLI_OPTS[@]}" --get temp --node "$NODE"       --expect "temp_c="
run "get free_mem"   -- ${CLI} "${CLI_OPTS[@]}" --get free_mem --node "$NODE"   --expect "free_mem="
run "get free_flash" -- ${CLI} "${CLI_OPTS[@]}" --get free_flash --node "$NODE" --expect "free_flash="
run "get log_count"  -- ${CLI} "${CLI_OPTS[@]}" --get log_count --node "$NODE"  --expect "log_count="

echo -e "${YEL}=== Bulk Snapshot ===${NC}"
run "get all" -- ${CLI} "${CLI_OPTS[@]}" --get all --node "$NODE" \
  --expect "id=" "alias=" "freq_hz=" "sf=" "bw_hz=" "cr=" "tx_pwr_dbm=" "chan=" "mode=" "hops=" "beacon_s=" "buf_size=" "ack="

echo -e "${YEL}=== Done ===${NC}"

