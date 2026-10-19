#include "sput.h"
#include "brubeck.h"

static struct brubeck_metric *new_metric(const char *name)
{
	size_t name_len = strlen(name);
	struct brubeck_metric *metric = malloc(sizeof(struct brubeck_metric) + name_len + 1);

	memcpy(metric->key, name, name_len);
	metric->key[name_len] = 0;
	metric->key_len = name_len;

	return metric;
}

void test_mstore__save(void)
{
	static const int nmetrics = 15000;
	brubeck_hashtable_t *store;
	int i;

	store = brubeck_hashtable_new(4096);

	for (i = 0; i < nmetrics; ++i) {
		char buffer[64];
		struct brubeck_metric *metric;

		sprintf(buffer, "github.test.metric.%d", i);
		metric = new_metric(buffer);

		if (!brubeck_hashtable_insert(store, metric->key, metric->key_len, metric))
			break;
	}

	sput_fail_unless(i == nmetrics, "stored 15000 metrics in table");

	for (i = 0; i < nmetrics; ++i) {
		char buffer[64];
		uint16_t len;

		len = sprintf(buffer, "github.test.metric.%d", i);

		if (!brubeck_hashtable_find(store, buffer, len))
			break;
	}

	sput_fail_unless(i == nmetrics, "lookup all metrics from table");
}

void test_mstore__remove(void)
{
	static const int nmetrics = 15000;
	brubeck_hashtable_t *store;
	struct brubeck_metric *metrics[15000];
	int i, found_odd = 0, found_even = 0;

	store = brubeck_hashtable_new(4096);

	for (i = 0; i < nmetrics; ++i) {
		char buffer[64];
		sprintf(buffer, "github.test.metric.%d", i);
		metrics[i] = new_metric(buffer);
		brubeck_hashtable_insert(store, metrics[i]->key, metrics[i]->key_len, metrics[i]);
	}

	for (i = 0; i < nmetrics; i += 2) {
		if (!brubeck_hashtable_remove(store, metrics[i]->key, metrics[i]->key_len))
			break;
	}
	sput_fail_unless(i >= nmetrics, "removed every even metric");
	sput_fail_unless(brubeck_hashtable_size(store) == nmetrics / 2, "table holds the odd half");

	brubeck_hashtable_gc(store);

	for (i = 0; i < nmetrics; ++i) {
		struct brubeck_metric *m = brubeck_hashtable_find(store, metrics[i]->key, metrics[i]->key_len);
		if (i % 2) found_odd += (m == metrics[i]);
		else found_even += (m != NULL);
	}
	sput_fail_unless(found_odd == nmetrics / 2, "odd metrics still found after gc");
	sput_fail_unless(found_even == 0, "removed metrics not found");

	for (i = 0; i < nmetrics; i += 2) {
		if (!brubeck_hashtable_insert(store, metrics[i]->key, metrics[i]->key_len, metrics[i]))
			break;
	}
	sput_fail_unless(i >= nmetrics, "removed metrics can be inserted again");
	sput_fail_unless(brubeck_hashtable_size(store) == nmetrics, "table full again");
}
