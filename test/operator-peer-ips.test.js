const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('fs');
const path = require('path');
const ejs = require('ejs');

const operatorEjsPath = path.join(__dirname, '../views/operator.ejs');

test('server-side peer IP normalization strips ports and protocols cleanly', () => {
    function sanitizePeerIp(ip) {
        const cleanIp = String(ip || '').trim().replace(/^sip:/i, '').split(':')[0].replace(/[^0-9.]/g, '');
        return /^\d+\.\d+\.\d+\.\d+$/.test(cleanIp) ? cleanIp : null;
    }

    assert.equal(sanitizePeerIp('192.168.1.105:5060'), '192.168.1.105');
    assert.equal(sanitizePeerIp('sip:10.0.0.42:5060'), '10.0.0.42');
    assert.equal(sanitizePeerIp('172.16.20.1'), '172.16.20.1');
    assert.equal(sanitizePeerIp('dynamic'), null);
    assert.equal(sanitizePeerIp('-none-'), null);
    assert.equal(sanitizePeerIp(''), null);
    assert.equal(sanitizePeerIp(null), null);
});

test('views/operator.ejs renders visible IP badges on extension cards when IP is present', async () => {
    const html = await ejs.renderFile(operatorEjsPath, {
        currentLang: 'en',
        isRtl: false,
        currentPage: '/operator',
        isSuperAdmin: true,
        user: { username: 'admin' },
        activeCalls: {},
        roster: [
            { extension: '101', name: 'Ahmed Emad', title: 'Tech Lead', emp_group: 'Engineering', online: true, ip: '192.168.1.101', photo: null },
            { extension: '102', name: 'Sara Mohamed', title: 'Support Agent', emp_group: 'Support', online: false, ip: null, photo: null }
        ],
        employeeGroups: ['Engineering', 'Support']
    });

    // Verify presence of employee-ip-badge class and element IDs
    assert.ok(html.includes('id="ipText-101"'), 'Should render ipText-101 element');
    assert.ok(html.includes('id="ipText-102"'), 'Should render ipText-102 element');
    assert.ok(html.includes('192.168.1.101'), 'Should display IP address for extension 101');

    // Extension 101 (has IP) should NOT have hidden class initially
    const hasHidden101 = /id="ipText-101"[^>]*class="[^"]*\bhidden\b[^"]*"/.test(html);
    assert.equal(hasHidden101, false, 'ipText-101 with valid IP should not have hidden class');

    // Extension 102 (no IP) should have hidden class initially
    const hasHidden102 = /id="ipText-102"[^>]*class="[^"]*\bhidden\b[^"]*"/.test(html);
    assert.ok(hasHidden102, 'ipText-102 without IP should have hidden class');
});

test('views/operator.ejs includes Socket.IO peerIPs handler updating IP text and visibility', () => {
    const content = fs.readFileSync(operatorEjsPath, 'utf8');

    assert.ok(content.includes("socket.on('peerIPs'"), 'Should contain Socket.io peerIPs event listener');
    assert.ok(content.includes("ipText.classList.remove('hidden')"), 'Should unhide IP element when valid IP is received via socket');
    assert.ok(content.includes("ipText.classList.add('hidden')"), 'Should hide IP element when IP is empty via socket');
});
