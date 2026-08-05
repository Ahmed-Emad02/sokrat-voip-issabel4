# SOKRAT VOIP (Issabel 4 / Asterisk 11 Edition)

A lightweight, high-performance VoIP management dashboard specifically tailored for **Issabel 4 installations running CentOS 7 and Asterisk 11**.

---

## 🎯 Target Platform & Engine

- **Operating System:** CentOS 7
- **PBX Distribution:** Issabel 4
- **Telephony Engine:** Asterisk 11 (`chan_sip` signaling driver)
- **Node.js Runtime:** Node.js 16 LTS (CentOS 7 `glibc 2.17` symbol compatibility)
- **Database System:** MariaDB 5.5 / 10.1 (`asterisk` & `asteriskcdrdb`)

---

## ✂️ Stripped / Excluded Features (Issabel 4 Optimizations)

To ensure maximum performance and stability on CentOS 7 / Asterisk 11, the following legacy modern modules have been removed:
- ❌ **Autodialer Engine:** Stripped campaign management, lead lists, pacing engine, and associated DB tables.
- ❌ **WebRTC Softphone:** Stripped in-browser audio streaming, WebSocket listeners, SIP.js/JsSIP libraries, and browser phone UI components.

---

## 🚀 Quick Install

Run as root on a fresh Issabel 4 server:

```bash
curl -fsSL https://raw.githubusercontent.com/Ahmed-Emad02/sokrat-voip-issabel4/main/install.sh -o /tmp/sokrat-install.sh
bash /tmp/sokrat-install.sh
```

For non-interactive GSM dongle count selection:

```bash
NUM_DONGLES=20 bash /tmp/sokrat-install.sh
```

---

## ✨ Features Included

- **Executive Dashboard** — Real-time KPI cards, inbound/outbound distribution charts, date-range filtering.
- **CDR Analytics & Reports** — Search call detail records by date, extension, status, source, destination with seekable audio playback.
- **Extension Statistics & Metrics** — Individual extension performance metrics, call duration analytics, and roster overview.
- **Live Operator Board / Switchboard** — Real-time extension state tracking (`chan_sip`), active channel monitoring, Listen/Whisper/Barge controls via ChanSpy.
- **GSM Dongle Management** — 1-second polling of Huawei GSM dongles via `chan_dongle`, SMS reception, USSD console, SIM number mapping, and AT command controls.
- **System Configuration & Management** — Extensions (Generic SIP), Ring Groups, Queues, System Recordings, Trunks, Inbound/Outbound Routes, IVR menus, Time Groups/Conditions.
- **Role-Based Access Control** — Multi-user management, group permissions, email-based password resets, root superadmin support.
- **Light & Dark Theme + RTL / Arabic** — Dual English & Arabic interfaces with automatic RTL support.

---

## 🛠️ Technical Details & Dialplan Setup

The installer automatically configures `/etc/asterisk/extensions_custom.conf` with required contexts:

```asterisk
[from-internal-custom]
exten => _222X.,1,NoOp(Spying on extension ${EXTEN:3} in Listen-only mode)
exten => _222X.,n,Answer()
exten => _222X.,n,Set(spyee_dial=${DB(DEVICE/${EXTEN:3}/dial)})
exten => _222X.,n,GotoIf($["${spyee_dial}" = ""]?fallback)
exten => _222X.,n,ChanSpy(${spyee_dial},q)
exten => _222X.,n,Hangup()
exten => _222X.,n(fallback),ChanSpy(SIP/${EXTEN:3},q)
exten => _222X.,n,Hangup()

exten => _223X.,1,NoOp(Spying on extension ${EXTEN:3} in Whisper mode)
exten => _223X.,n,Answer()
exten => _223X.,n,Set(spyee_dial=${DB(DEVICE/${EXTEN:3}/dial)})
exten => _223X.,n,GotoIf($["${spyee_dial}" = ""]?fallback)
exten => _223X.,n,ChanSpy(${spyee_dial},qw)
exten => _223X.,n,Hangup()
exten => _223X.,n(fallback),ChanSpy(SIP/${EXTEN:3},qw)
exten => _223X.,n,Hangup()

exten => _224X.,1,NoOp(Spying on extension ${EXTEN:3} in Barge mode)
exten => _224X.,n,Answer()
exten => _224X.,n,Set(spyee_dial=${DB(DEVICE/${EXTEN:3}/dial)})
exten => _224X.,n,GotoIf($["${spyee_dial}" = ""]?fallback)
exten => _224X.,n,ChanSpy(${spyee_dial},qB)
exten => _224X.,n,Hangup()
exten => _224X.,n(fallback),ChanSpy(SIP/${EXTEN:3},qB)
exten => _224X.,n,Hangup()

exten => _225X.,1,NoOp(--- Instant AGI Hijack Call for Extension ${EXTEN:3} ---)
same => n,Answer()
same => n,AGI(hijack_call.py,${EXTEN:3})
same => n,Hangup()
```

---

## 🔒 Security & Boundaries

This repository (`sokrat-voip-issabel4`) is dedicated strictly to Issabel 4 / Asterisk 11 setups. The source repository `sokrat-voip-dev` remains untouched as a read-only reference for Issabel 5 / Asterisk 18 installations.
