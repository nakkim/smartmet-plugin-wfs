include $(shell echo $${PREFIX-/usr})/share/smartmet/devel/makefile.inc

# Examples (default: run smartmet-plugin-test directly)
#    TEST_RUNNER="gdb --args"
#    TEST_RUNNER="valgrind --log-file=valgrind.out --leak-check=full --leak-resolution=high"
TEST_RUNNER ?=

test test-grid:	start-redis-db
	if $(MAKE) test-impl; then ok=true; else ok=false; fi; $(MAKE) stop-redis-db; $$ok

test-impl:
	$(TEST_RUNNER) smartmet-plugin-test --handler /wfs \
		--reactor-config cnf/reactor.conf \
		--failures-dir failures \
		--timeout 300

clean:
	rm -rf failures
	rm -rf cnf/grid

start-redis-db:
	rm -rf cnf/grid
	cp -pr $(datadir)/smartmet/test/grid cnf/
	@echo "*** Creating the Redis configuration file."
	smartmet-grid-test-config-creator cnf/environment.conf cnf/grid/redis/redis-template.conf cnf/grid/redis/redis.conf
	@echo "*** Starting the Redis database."
	@redis-server cnf/grid/redis/redis.conf

stop-redis-db:
	@if [ -f cnf/grid/redis/redis-server.pid ] ; then \
		cat cnf/grid/redis/redis-server.pid | xargs kill -9; \
		rm -f cnf/grid/redis/redis-server.pid; \
	fi
