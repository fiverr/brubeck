#include <string.h>

#include "sput.h"
#include "brubeck.h"

static void must_parse(const char *msg_text, double value, double sample, uint8_t modifiers)
{
	struct brubeck_statsd_msg msg;
	char buffer[128];
	size_t len = strlen(msg_text);
	memcpy(buffer, msg_text, len);

	sput_fail_unless(brubeck_statsd_msg_parse(&msg, buffer, buffer + len) == 0, msg_text);
	sput_fail_unless(value == msg.value, "msg.value == expected");
	sput_fail_unless(sample == msg.sample_freq, "msg.sample_rate == expected");
	sput_fail_unless(modifiers == msg.modifiers, "msg.modifiers == expected");
}

static void must_sanitize_key(const char *msg_text, const char *expected_key)
{
	struct brubeck_statsd_msg msg;
	char buffer[128];
	size_t len = strlen(msg_text);
	memcpy(buffer, msg_text, len);

	sput_fail_unless(brubeck_statsd_msg_parse(&msg, buffer, buffer + len) == 0, msg_text);
	sput_fail_unless(strcmp(msg.key, expected_key) == 0, expected_key);
	//sput_fail_unless(strcmp(msg.key, expected_key) == 0, "msg.key == expected");
}

static void must_not_parse(const char *msg_text)
{
	struct brubeck_statsd_msg msg;
	char buffer[128];
	size_t len = strlen(msg_text);
	memcpy(buffer, msg_text, len);

	sput_fail_unless(brubeck_statsd_msg_parse(&msg, buffer, buffer + len) < 0, msg_text);
}

void test_statsd_msg__parse_strings(void)
{
	must_parse("github.auth.fingerprint.sha1:1|c", 1, 1.0, 0);
	must_parse("github.auth.fingerprint.sha1:1|c|@0.1", 1, 10.0, 0);
	must_parse("github.auth.fingerprint.sha1:1|g", 1, 1.0, 0);
	must_parse("lol:1|ms", 1, 1.0, 0);
	must_parse("this.is.sparta:23.23|g", 23.23, 1.0, 0);
	must_parse("this.is.sparta:0.232030|g", 0.23203, 1.0, 0);
	must_parse("this.are.some.floats:1234567.89|g", 1234567.89, 1.0, 0);
	must_parse("this.are.some.floats:1234567.89|g|@0.025", 1234567.89, 40.0, 0);
	must_parse("this.are.some.floats:1234567.89|g|@0.25", 1234567.89, 4.0, 0);
	must_parse("this.are.some.floats:1234567.89|g|@0.01", 1234567.89, 100.0, 0);
	must_parse("this.are.some.floats:1234567.89|g|@000.0100", 1234567.89, 100.0, 0);
	must_parse("this.are.some.floats:1234567.89|g|@1.0", 1234567.89, 1.0, 0);
	must_parse("this.are.some.floats:1234567.89|g|@1", 1234567.89, 1.0, 0);
	must_parse("this.are.some.floats:1234567.89|g|@1.", 1234567.89, 1.0, 0);
	must_parse("this.are.some.floats:|g", 0.0, 1.0, 0);
	must_parse("this.are.some.floats:1234567.89|g", 1234567.89, 1.0, 0);
	must_parse("gauge.increment:+1|g", 1, 1.0, BRUBECK_MOD_RELATIVE_VALUE);
	must_parse("gauge.decrement:-1|g", -1, 1.0, BRUBECK_MOD_RELATIVE_VALUE);

	must_not_parse("this.are.some.floats:12.89.23|g");
	must_not_parse("this.are.some.floats:12.89|a");
	must_not_parse("this.are.some.floats:12.89|msdos");
	must_not_parse("this.are.some.floats:12.89g|g");
	must_not_parse("this.are.some.floats:12.89|");
	must_not_parse("this.are.some.floats:12.89");
	must_not_parse("this.are.some.floats:12.89 |g");
	must_not_parse("this.are.some.floats|g");
	must_not_parse("this.are.some.floats:1.0|g|1.0");
	must_not_parse("this.are.some.floats:1.0|g|0.1");
	must_not_parse("this.are.some.floats:1.0|g|@0.1.1");
	must_not_parse("this.are.some.floats:1.0|g|@0.1@");
	must_not_parse("this.are.some.floats:1.0|g|@0.1125.2");
	must_not_parse("this.are.some.floats:1.0|g|@0.1125.2");
	must_not_parse("this.are.some.floats:1.0|g|@1.23");
	must_not_parse("this.are.some.floats:1.0|g|@3.0");
	must_not_parse("this.are.some.floats:1.0|g|@-3.0");
	must_not_parse("this.are.some.floats:1.0|g|@-1.0");
	must_not_parse("this.are.some.floats:1.0|g|@-0.23");
	must_not_parse("this.are.some.floats:1.0|g|@0.0");
	must_not_parse("this.are.some.floats:1.0|g|@0");

	must_sanitize_key("replace.space.in.key name:1|c","replace.space.in.key_name");
	must_sanitize_key("replace.slash.in.key/name:1|c","replace.slash.in.key-name");
	must_sanitize_key("remove.non.alphanumeric.chars.from.key*#name:1|c","remove.non.alphanumeric.chars.from.keyname");
}

static int prefixed(const char *prefix, uint8_t type, int key_len)
{
	struct key_prefix kp;
	struct brubeck_statsd_msg msg;
	char key[300];
	int rc;

	strcpy(kp.str, prefix);
	kp.str_len = strlen(prefix);
	memset(key, 'a', key_len);
	key[key_len] = '\0';
	msg.key = key;
	msg.key_len = key_len;
	msg.type = type;

	rc = add_prefix(&msg, &kp);
	sput_fail_unless(kp.str_len == strlen(prefix), "prefix length intact");
	return rc;
}

void test_statsd_msg__add_prefix(void)
{
	/* "<prefix><type>.<key>" must stay under 256 chars */
	sput_fail_unless(prefixed("tech.", BRUBECK_MT_COUNTER, 241) == 0, "tech counter, key 241: kept");
	sput_fail_unless(prefixed("tech.", BRUBECK_MT_COUNTER, 242) == -10, "tech counter, key 242: dropped");
	sput_fail_unless(prefixed("tech.", BRUBECK_MT_TIMER, 243) == 0, "tech timer, key 243: kept");
	sput_fail_unless(prefixed("tech.", BRUBECK_MT_TIMER, 244) == -10, "tech timer, key 244: dropped");
	sput_fail_unless(prefixed("biz.", BRUBECK_MT_GAUGE, 244) == 0, "biz gauge, key 244: kept");
	sput_fail_unless(prefixed("biz.", BRUBECK_MT_GAUGE, 255) == -10, "biz gauge, key 255: dropped");
}
