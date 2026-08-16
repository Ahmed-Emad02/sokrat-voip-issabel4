#!/bin/bash
# Issabel Dashboard — Automated Uninstaller
# Removes dashboard application files and restores Issabel to default Apache configuration.
# DOES NOT touch the database.

set -euo pipefail

INSTALL_DIR=/opt/sokrat-voip

echo "============================================"
echo " Issabel Dashboard Uninstaller"
echo "============================================"

# 1. Stop and disable dashboard systemd services
echo "[1/6] Stopping and removing systemd service units..."
for svc in sokrat-voip issabel-dashboard; do
    if systemctl is-active "$svc" &>/dev/null || systemctl is-enabled "$svc" &>/dev/null; then
        systemctl stop "$svc" 2>/dev/null || true
        systemctl disable "$svc" 2>/dev/null || true
        echo "  Service $svc stopped and disabled."
    fi
    if [ -f "/etc/systemd/system/${svc}.service" ]; then
        rm -f "/etc/systemd/system/${svc}.service"
        echo "  Removed /etc/systemd/system/${svc}.service"
    fi
done
systemctl daemon-reload
echo "  Systemd daemon reloaded."

# 2. Restore Apache configuration to default Issabel behavior
echo "[2/6] Restoring Apache web server configuration..."

# Remove dashboard reverse proxy config for port 80
if [ -f /etc/httpd/conf.d/dashboard.conf ]; then
    rm -f /etc/httpd/conf.d/dashboard.conf
    echo "  Removed /etc/httpd/conf.d/dashboard.conf"
fi

# Remove proxy directives inserted into ssl.conf
if [ -f /etc/httpd/conf.d/ssl.conf ]; then
    sed -i '/ProxyPreserveHost On/d' /etc/httpd/conf.d/ssl.conf 2>/dev/null || true
    sed -i '/RewriteEngine On/d' /etc/httpd/conf.d/ssl.conf 2>/dev/null || true
    sed -i '/RewriteCond %{HTTP:Upgrade}/d' /etc/httpd/conf.d/ssl.conf 2>/dev/null || true
    sed -i '/RewriteCond %{REQUEST_URI}/d' /etc/httpd/conf.d/ssl.conf 2>/dev/null || true
    sed -i '/RewriteRule.*ws:\/\/127\.0\.0\.1:8080/d' /etc/httpd/conf.d/ssl.conf 2>/dev/null || true
    sed -i '/ProxyPass.*8080/d' /etc/httpd/conf.d/ssl.conf 2>/dev/null || true
    sed -i '/ProxyPassReverse.*8080/d' /etc/httpd/conf.d/ssl.conf 2>/dev/null || true
    echo "  Cleaned proxy settings from /etc/httpd/conf.d/ssl.conf"
fi

# Remove Listen 3000 from httpd.conf and ensure Listen 80 is present
if [ -f /etc/httpd/conf/httpd.conf ]; then
    sed -i '/^Listen 3000/d' /etc/httpd/conf/httpd.conf
    if ! grep -q '^Listen 80' /etc/httpd/conf/httpd.conf; then
        echo "Listen 80" >> /etc/httpd/conf/httpd.conf
    fi
    echo "  Restored default Listen port (80) in httpd.conf"
fi

# Restart Apache to apply changes
if systemctl is-active httpd &>/dev/null; then
    systemctl restart httpd
    echo "  Apache restarted."
fi

# 3. Clean custom Asterisk dialplan contexts
echo "[3/6] Restoring Asterisk dialplan configurations..."
if [ -f /etc/asterisk/extensions_custom.conf ]; then
    python3 -c "import re;f=open('/etc/asterisk/extensions_custom.conf').read();f=re.sub(r'\[from-internal-custom\].*?(?=\n\[|\Z)', '', f, flags=re.DOTALL);f=re.sub(r'\[from-intercom-autoanswer\].*?(?=\n\[|\Z)', '', f, flags=re.DOTALL);f=re.sub(r'\[intercom-predial-autoanswer\].*?(?=\n\[|\Z)', '', f, flags=re.DOTALL);f=re.sub(r'\[from-intercom-conf\].*?(?=\n\[|\Z)', '', f, flags=re.DOTALL);f=re.sub(r'\[from-dongle-custom\].*?(?=\n\[|\Z)', '', f, flags=re.DOTALL);f=re.sub(r'\[ext-moh\].*?(?=\n\[|\Z)', '', f, flags=re.DOTALL);f=re.sub(r'\[macro-dialout-trunk-predial-hook\].*?(?=\n\[|\Z)', '', f, flags=re.DOTALL);f=re.sub(r'\[dongle-hangup-cleanup\].*?(?=\n\[|\Z)', '', f, flags=re.DOTALL);f=re.sub(r'\[macro-dialout-one-predial-hook\].*?(?=\n\[|\Z)', '', f, flags=re.DOTALL);open('/etc/asterisk/extensions_custom.conf','w').write(f)" 2>/dev/null || true
    asterisk -rx "dialplan reload" 2>/dev/null || true
    echo "  Cleaned custom dialplan contexts and reloaded Asterisk."
fi

# 4. Clean up AGI scripts and credential artifacts
echo "[4/6] Removing AGI scripts and credential artifacts..."
rm -f /var/lib/asterisk/agi-bin/hijack_call.py
rm -f /etc/sokrat-root-credential.txt
rm -rf /tmp/dashboard-uploads /tmp/dashboard-staging

# 5. Preserving database notice
echo "[5/6] Database notice: Database tables were NOT dropped or altered."

# 6. Remove dashboard directories
echo "[6/6] Removing dashboard installation directories..."
cd /tmp
for d in /opt/sokrat-voip /opt/issabel-dashboard; do
    if [ -d "$d" ]; then
        rm -rf "$d"
        echo "  Removed $d"
    fi
done

echo "============================================"
echo " Uninstallation complete! Issabel default web GUI restored."
echo "============================================"
