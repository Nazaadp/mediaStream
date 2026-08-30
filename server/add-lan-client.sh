#!/usr/bin/env bash
# add-lan-client.sh — authorise a LAN IP to reach the mediaStream API.
#
# `allowed_lan_clients` is the only LAN-bound setting in the entire stack. That
# one set gates all three chains:
#     forward accept  |  prerouting DNAT 8443 -> 10.66.0.10:443  |  postrouting masquerade
# Nothing else needs touching when a device is added or the LAN is re-IP'd.
#
# Applies the change live AND persists it — both matter. The live add takes
# effect instantly; the file edit survives reboots, because the libvirt VM-start
# hook reloads /etc/nftables.conf and would otherwise revert the set.
#
# Usage:
#     sudo ./add-lan-client.sh 192.168.100.7
#     sudo ./add-lan-client.sh              # prompts for the IP
#
# Removing a client is the mirror image:
#     sudo nft delete element inet sandbox allowed_lan_clients { <IP> }
#     then delete it from the elements line in /etc/nftables.d/sandbox.nft

set -euo pipefail

NFT_FILE=/etc/nftables.d/sandbox.nft
SET_NAME=allowed_lan_clients
TABLE=(inet sandbox)
LAN_IFACE=enp4s0

die()  { printf '\033[31merror:\033[0m %s\n' "$1" >&2; exit 1; }
warn() { printf '\033[33mwarn:\033[0m  %s\n' "$1" >&2; }
note() { printf '\033[36m==>\033[0m %s\n' "$1"; }
ok()   { printf '\033[32m ok\033[0m  %s\n' "$1"; }

[[ $EUID -eq 0 ]] || die "must run as root:  sudo $0 ${1:-<IP>}"
[[ -f $NFT_FILE ]] || die "$NFT_FILE not found"
command -v nft >/dev/null || die "nft not installed"

# ---------------------------------------------------------------- 1. get IP --
IP="${1:-}"
if [[ -z $IP ]]; then
    read -rp "IP to authorise (e.g. 192.168.100.7): " IP
fi
IP="${IP//[[:space:]]/}"
[[ -n $IP ]] || die "no IP given"

[[ $IP =~ ^([0-9]{1,3}\.){3}[0-9]{1,3}$ ]] || die "'$IP' is not an IPv4 address"
IFS=. read -r o1 o2 o3 o4 <<<"$IP"
for o in "$o1" "$o2" "$o3" "$o4"; do
    ((10#$o >= 0 && 10#$o <= 255)) || die "'$IP' has an octet out of range"
done

# A typo'd subnet fails silently later (traffic just never matches the DNAT),
# so catch it here while it is still cheap to notice.
host_prefix=$(ip -4 -o addr show "$LAN_IFACE" 2>/dev/null |
              awk 'NR==1 {print $4}' | cut -d/ -f1 | cut -d. -f1-3) || true
if [[ -n ${host_prefix:-} && "$o1.$o2.$o3" != "$host_prefix" ]]; then
    warn "$IP is outside the host LAN ($host_prefix.0/24) — it will never match the DNAT rule."
    read -rp "      Add it anyway? [y/N] " reply
    [[ ${reply,,} == y ]] || die "aborted"
fi

in_file() { sed -n "/set $SET_NAME/,/}/p" "$NFT_FILE" | grep -qF "$IP"; }

# --------------------------------------------------------------- 2. persist --
grep -q "set $SET_NAME" "$NFT_FILE" || die "set '$SET_NAME' not found in $NFT_FILE"

if in_file; then
    ok "$IP already persisted in $NFT_FILE"
else
    backup="$NFT_FILE.bak-$(date +%Y%m%d-%H%M%S)"
    cp -a "$NFT_FILE" "$backup"
    note "backed up -> $backup"

    restore() { cp -a "$backup" "$NFT_FILE"; }

    # Range-scoped to this set only, and appends to whatever is already listed,
    # so it stays correct as the list grows.
    sed -i "/set $SET_NAME/,/}/ s/elements = { \(.*\) }/elements = { \1, $IP }/" "$NFT_FILE"

    # An empty or reformatted set would make the sed a silent no-op.
    in_file || { restore; die "could not edit the elements line — $NFT_FILE restored, add $IP by hand"; }

    # A malformed file here takes the whole ruleset down at next boot, so prove
    # it parses before we walk away from it.
    if ! nft -c -f /etc/nftables.conf; then
        restore
        die "ruleset failed syntax check — $NFT_FILE restored from $backup"
    fi
    ok "persisted to $NFT_FILE"
fi

# ------------------------------------------------------------- 3. apply live --
# Deliberately NOT `nft -f /etc/nftables.conf`: that file opens with
# `flush ruleset`, which drops the egress allowlist and libvirt's chains and
# kills VM internet until the 04:00 cron re-runs. A single element add is surgical.
if nft list set "${TABLE[@]}" "$SET_NAME" | grep -qF "$IP"; then
    ok "$IP already in the live ruleset"
else
    nft add element "${TABLE[@]}" "$SET_NAME" "{ $IP }"
    ok "$IP added to the live ruleset"
fi

# ----------------------------------------------------------------- 4. report --
note "live set is now:"
nft list set "${TABLE[@]}" "$SET_NAME" | sed 's/^/     /'

cat <<EOF

Next steps for $IP:
  • Give it a DHCP reservation on the router, or the lease will move and it will
    silently stop working.
  • Test from that device:
        curl -k https://$(ip -4 -o addr show "$LAN_IFACE" | awk 'NR==1 {print $4}' | cut -d/ -f1):8443/api/v1/discover/movies
  • Note: with mTLS currently disabled, this list is the only thing gating the
    API. Any address here has full unauthenticated access.
EOF
