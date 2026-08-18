const test = require('node:test');
const assert = require('node:assert/strict');
const { cleanPhoneString, extractPhoneFromClid, isValidPhoneNumber, getPhoneVariants } = require('../lib/phone-normalization');

test('cleanPhoneString removes non-digits except leading plus', () => {
    assert.equal(cleanPhoneString('+20 (101) 234-5678'), '+201012345678');
    assert.equal(cleanPhoneString('010-1234-5678'), '01012345678');
    assert.equal(cleanPhoneString(''), '');
});

test('extractPhoneFromClid extracts from angle brackets', () => {
    assert.equal(extractPhoneFromClid('"John Doe" <01012345678>'), '01012345678');
    assert.equal(extractPhoneFromClid('<+201012345678>'), '+201012345678');
    assert.equal(extractPhoneFromClid('01012345678'), '01012345678');
});

test('getPhoneVariants generates all Egyptian number formats', () => {
    const variants = getPhoneVariants('01012345678', '20');
    assert.ok(variants.includes('01012345678'));
    assert.ok(variants.includes('1012345678'));
    assert.ok(variants.includes('201012345678'));
    assert.ok(variants.includes('00201012345678'));
    assert.ok(variants.includes('+201012345678'));
});
