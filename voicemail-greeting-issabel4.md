# Voicemail Greeting Upload — Issabel 4 (Asterisk 11)

Replace the default "unavailable" voicemail greeting with your own custom recording on Issabel 4 servers. Only your audio plays — no default prompts.

## Requirements

- Server access (SSH root or sudo)
- `sox` installed (`yum install -y sox`)
- A `.wav` file (any sample rate, stereo/mono — will be auto-converted)

## Commands

### From Windows — upload your WAV

```powershell
scp my-greeting.wav root@<server-ip>:/tmp/
```

### On the server — install sox (skip if already installed)

```bash
yum install -y sox
```

### On the server — convert and deploy

```bash
# 1. Convert your WAV to 8kHz mono GSM (Asterisk 11 primary format)
sox /tmp/my-greeting.wav -r 8000 -c 1 /tmp/custom.gsm

# 2. Backup any existing mailbox greeting
cp /var/spool/asterisk/voicemail/default/120/unavail.gsm \
   /var/spool/asterisk/voicemail/default/120/unavail.gsm.bak 2>/dev/null

# 3. Place the custom greeting in the mailbox directory
cp /tmp/custom.gsm /var/spool/asterisk/voicemail/default/120/unavail.gsm
chown asterisk:asterisk /var/spool/asterisk/voicemail/default/120/unavail.gsm

# 4. Silence vm-leavemsg (plays after greeting, before beep)
sox -n -r 8000 -c 1 /tmp/silent.gsm trim 0 1
cp /tmp/silent.gsm /var/lib/asterisk/sounds/en/vm-leavemsg.gsm
chown asterisk:asterisk /var/lib/asterisk/sounds/en/vm-leavemsg.gsm

# 5. Backup and restore system unavailable (not used by leave-msg flow,
#    but kept for safety)
cp /var/lib/asterisk/sounds/en/unavailable.gsm \
   /var/lib/asterisk/sounds/en/unavailable.gsm.bak 2>/dev/null

# 6. Reload Asterisk
asterisk -rx "core reload"
```

## Per-extension vs Universal

| Mode | File to replace |
|------|----------------|
| **Single extension** | `/var/spool/asterisk/voicemail/default/<ext>/unavail.gsm` |
| **All extensions** (universal) | Replace the file for each extension's mailbox directory |

For universal, loop through all mailboxes:

```bash
for ext in $(ls /var/spool/asterisk/voicemail/default/); do
  cp /tmp/custom.gsm "/var/spool/asterisk/voicemail/default/$ext/unavail.gsm"
  chown asterisk:asterisk "/var/spool/asterisk/voicemail/default/$ext/unavail.gsm"
done
```

## Verify

Test by calling the extension and letting it go to voicemail. To see what's playing, check the Asterisk log:

```bash
grep 'Playing' /var/log/asterisk/full | tail -5
```

## Restore defaults

```bash
# Restore mailbox greeting
cp /var/spool/asterisk/voicemail/default/120/unavail.gsm.bak \
   /var/spool/asterisk/voicemail/default/120/unavail.gsm 2>/dev/null || \
   rm -f /var/spool/asterisk/voicemail/default/120/unavail.gsm

# Restore vm-leavemsg
cp /var/lib/asterisk/sounds/en/vm-leavemsg.gsm.bak \
   /var/lib/asterisk/sounds/en/vm-leavemsg.gsm

# Restore system unavailable
cp /var/lib/asterisk/sounds/en/unavailable.gsm.bak \
   /var/lib/asterisk/sounds/en/unavailable.gsm

# Reload
asterisk -rx "core reload"
```

## How it works (Asterisk 11 vs 18)

| Aspect | Asterisk 11 | Asterisk 18 |
|--------|-------------|-------------|
| Greeting location | Mailbox directory only | Mailbox + system fallback |
| Mailbox file | `unavail.gsm` | `unavail.wav` (or any format) |
| Instruction skip | No `s` flag available; silence `vm-leavemsg` | `s` flag skips `vm-intro` |
| Prompts after greeting | `vm-leavemsg` only | `vm-intro` + `vm-leavemsg` |
