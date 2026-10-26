cpplint:
	cpplint --repository=. --recursive --filter=-whitespace/line_length,-legal/copyright,-runtime/printf,-build/include,-build/namespace,-whitespace/indent_namespace ./src ./tests
.PHONY: cpplint

TEST_CXXFLAGS ?= -fsanitize=address,undefined

test:
	@set -eu; test_dir=$$(mktemp -d); trap 'rm -rf "$$test_dir"' EXIT; \
	for platform in ESP32 ESP8266; do \
	  for test_source in tests/*.cpp; do \
	    $(CXX) $(CPPFLAGS) $(CXXFLAGS) $(TEST_CXXFLAGS) -std=c++17 -funsigned-char -UNDEBUG \
	      -D$$platform -DARDUINO_ARCH_$$platform -Itests/stubs -Isrc \
	      "$$test_source" src/AsyncMqttClient.cpp \
	      src/AsyncMqttClient/Packets/*.cpp src/AsyncMqttClient/Packets/Out/*.cpp \
	      $(LDFLAGS) -o "$$test_dir/test"; \
	    "$$test_dir/test"; \
	  done; \
	done
.PHONY: test
