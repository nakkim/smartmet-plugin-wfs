# Examples (default: run smartmet-plugin-test directly)
#    TEST_RUNNER="gdb --args"
#    TEST_RUNNER="valgrind --log-file=valgrind.out --leak-check=full --leak-resolution=high"
TEST_RUNNER ?=

test:
	$(TEST_RUNNER) smartmet-plugin-test --handler /wfs \
		--reactor-config cnf/reactor.conf \
		--failures-dir failures \
		--timeout 300

clean:
	rm -rf failures
