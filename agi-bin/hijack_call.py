#!/usr/bin/env python3
import re
import subprocess
import sys


SAFE_EXTENSION = re.compile(r'^\d{2,10}$')
SAFE_CHANNEL = re.compile(r'^[A-Za-z0-9_@/;:.\-]+$')


def agi_cmd(command):
    sys.stdout.write(command + '\n')
    sys.stdout.flush()
    return sys.stdin.readline()


def parse_concise_channels(output):
    channels = []
    for line in output.splitlines():
        parts = line.split('!')
        if len(parts) < 13:
            continue
        channels.append({
            'channel': parts[0].strip(),
            'state': parts[4].strip(),
            'caller_id': parts[7].strip(),
            'bridged_channel': parts[12].strip(),
        })
    return channels


def resolve_hijack_channels(output, target_extension, supervisor_channel):
    channels = parse_concise_channels(output)
    known_channels = {item['channel'] for item in channels}
    technology_prefixes = (
        'SIP/{}-'.format(target_extension),
        'PJSIP/{}-'.format(target_extension),
        'IAX2/{}-'.format(target_extension),
    )

    candidates = []
    for item in channels:
        channel = item['channel']
        if channel == supervisor_channel:
            continue
        exact_device = channel.startswith(technology_prefixes)
        caller_match = item['caller_id'] == target_extension
        if not exact_device and not caller_match:
            continue
        peer = item['bridged_channel']
        has_live_peer = peer in known_channels and peer not in (channel, supervisor_channel)
        candidates.append((
            int(has_live_peer),
            int(item['state'].lower() == 'up'),
            int(exact_device),
            item,
        ))

    if not candidates:
        return '', ''

    candidates.sort(key=lambda candidate: candidate[:3], reverse=True)
    employee = candidates[0][3]
    peer_channel = employee['bridged_channel']
    if peer_channel not in known_channels or peer_channel in (employee['channel'], supervisor_channel):
        peer_channel = ''

    return employee['channel'], peer_channel


def read_agi_environment():
    environment = {}
    while True:
        line = sys.stdin.readline().strip()
        if not line:
            return environment
        if ':' in line:
            key, value = line.split(':', 1)
            environment[key.strip()] = value.strip()


def main():
    agi_environment = read_agi_environment()
    target_extension = sys.argv[1].strip() if len(sys.argv) > 1 else ''
    supervisor_channel = agi_environment.get('agi_channel', '')

    if not SAFE_EXTENSION.fullmatch(target_extension):
        agi_cmd('VERBOSE "Hijack rejected: invalid target extension" 2')
        return

    try:
        output = subprocess.check_output(
            ['/usr/sbin/asterisk', '-rx', 'core show channels concise'],
            stderr=subprocess.DEVNULL,
        ).decode('utf-8', errors='ignore')
    except (OSError, subprocess.CalledProcessError):
        agi_cmd('VERBOSE "Hijack failed: unable to inspect active channels" 2')
        return

    employee_channel, peer_channel = resolve_hijack_channels(
        output,
        target_extension,
        supervisor_channel,
    )
    if not employee_channel:
        agi_cmd('VERBOSE "Hijack failed: target extension has no active channel" 2')
        return
    if not peer_channel:
        agi_cmd('VERBOSE "Hijack failed: target channel has no bridged peer" 2')
        return
    if not SAFE_CHANNEL.fullmatch(employee_channel) or not SAFE_CHANNEL.fullmatch(peer_channel):
        agi_cmd('VERBOSE "Hijack failed: unsafe channel name returned by Asterisk" 2')
        return

    agi_cmd('VERBOSE "Hijacking {} from {} to {}" 2'.format(
        peer_channel,
        employee_channel,
        supervisor_channel,
    ))
    agi_cmd('EXEC Bridge "{},p"'.format(peer_channel))
    agi_cmd('EXEC SoftHangup "{}"'.format(employee_channel))


if __name__ == '__main__':
    main()
