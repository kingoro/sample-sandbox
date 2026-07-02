# C workflow sample build file.
#
# 学習用サンプルなので、あえてMakefileも単純な形にしています。
# ルートで `make` を実行すると、Unit A-EとFunction A/Bの確認用バイナリを作ります。

CC ?= gcc
BUILD_DIR := build

CFLAGS := -std=c99 -Wall -Wextra -Werror
PTHREAD_FLAGS := -pthread

DOMAIN_COMMON_INC := -Isrc/domain/common/include
FUNCTION_A_INC := -Isrc/domain/functionA/include
FUNCTION_B_INC := -Isrc/domain/functionB/include
UNIT_A_INC := -Isrc/unit/unitA/include
UNIT_B_INC := -Isrc/unit/unitB/include
UNIT_C_INC := -Isrc/unit/unitC/include
UNIT_D_INC := -Isrc/unit/unitD/include
UNIT_E_INC := -Isrc/unit/unitE/include
UNIT_INCS := $(UNIT_A_INC) $(UNIT_B_INC) $(UNIT_C_INC) $(UNIT_D_INC) $(UNIT_E_INC)

DOMAIN_COMMON_SRCS := \
	src/domain/common/src/domain_action.c \
	src/domain/common/src/domain_action_queue.c \
	src/domain/common/src/domain_step.c \
	src/domain/common/src/domain_sequence.c \
	src/domain/common/src/domain_scenario.c \
	src/domain/common/src/domain_status_dispatcher.c \
	src/domain/common/src/domain_runner.c

UNIT_SRCS := \
	src/unit/unitA/src/unit_a.c \
	src/unit/unitB/src/unit_b.c \
	src/unit/unitC/src/unit_c.c \
	src/unit/unitD/src/unit_d.c \
	src/unit/unitE/src/unit_e.c

UNIT_TEST_BINS := \
	$(BUILD_DIR)/test_unit_a \
	$(BUILD_DIR)/test_unit_b \
	$(BUILD_DIR)/test_unit_c \
	$(BUILD_DIR)/test_unit_d \
	$(BUILD_DIR)/test_unit_e

DOMAIN_TEST_BINS := \
	$(BUILD_DIR)/test_function_a_manager \
	$(BUILD_DIR)/test_function_b_manager

CONTROL_BINS := \
	$(BUILD_DIR)/control_app \
	$(BUILD_DIR)/control_error_app

.PHONY: all units domain control control-error test run-unit-a run-unit-b run-unit-c run-unit-d run-unit-e run-function-a run-function-b run-control run-control-error clean

all: units domain control

units: $(UNIT_TEST_BINS)

domain: $(DOMAIN_TEST_BINS)

control: $(CONTROL_BINS)

control-error: $(BUILD_DIR)/control_error_app

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/test_unit_a: src/unit/unitA/src/unit_a.c src/unit/unitA/test/test_unit_a.c src/unit/unitA/include/unit_a.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(UNIT_A_INC) src/unit/unitA/src/unit_a.c src/unit/unitA/test/test_unit_a.c -o $@

$(BUILD_DIR)/test_unit_b: src/unit/unitB/src/unit_b.c src/unit/unitB/test/test_unit_b.c src/unit/unitB/include/unit_b.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(UNIT_B_INC) src/unit/unitB/src/unit_b.c src/unit/unitB/test/test_unit_b.c -o $@

$(BUILD_DIR)/test_unit_c: src/unit/unitC/src/unit_c.c src/unit/unitC/test/test_unit_c.c src/unit/unitC/include/unit_c.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(UNIT_C_INC) src/unit/unitC/src/unit_c.c src/unit/unitC/test/test_unit_c.c -o $@

$(BUILD_DIR)/test_unit_d: src/unit/unitD/src/unit_d.c src/unit/unitD/test/test_unit_d.c src/unit/unitD/include/unit_d.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(UNIT_D_INC) src/unit/unitD/src/unit_d.c src/unit/unitD/test/test_unit_d.c -o $@

$(BUILD_DIR)/test_unit_e: src/unit/unitE/src/unit_e.c src/unit/unitE/test/test_unit_e.c src/unit/unitE/include/unit_e.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(UNIT_E_INC) src/unit/unitE/src/unit_e.c src/unit/unitE/test/test_unit_e.c -o $@

$(BUILD_DIR)/test_function_a_manager: $(DOMAIN_COMMON_SRCS) $(UNIT_SRCS) src/domain/functionA/src/function_a_manager.c src/domain/functionA/test/test_function_a_manager.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(DOMAIN_COMMON_INC) $(FUNCTION_A_INC) $(UNIT_INCS) $(DOMAIN_COMMON_SRCS) src/domain/functionA/src/function_a_manager.c src/domain/functionA/test/test_function_a_manager.c $(UNIT_SRCS) -o $@

$(BUILD_DIR)/test_function_b_manager: $(DOMAIN_COMMON_SRCS) $(UNIT_SRCS) src/domain/functionB/src/function_b_manager.c src/domain/functionB/test/test_function_b_manager.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(DOMAIN_COMMON_INC) $(FUNCTION_B_INC) $(UNIT_INCS) $(DOMAIN_COMMON_SRCS) src/domain/functionB/src/function_b_manager.c src/domain/functionB/test/test_function_b_manager.c $(UNIT_SRCS) -o $@

$(BUILD_DIR)/control_app: $(DOMAIN_COMMON_SRCS) $(UNIT_SRCS) src/domain/functionA/src/function_a_manager.c src/control/src/main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(PTHREAD_FLAGS) $(DOMAIN_COMMON_INC) $(FUNCTION_A_INC) $(UNIT_INCS) $(DOMAIN_COMMON_SRCS) src/domain/functionA/src/function_a_manager.c src/control/src/main.c $(UNIT_SRCS) -o $@

$(BUILD_DIR)/control_error_app: $(DOMAIN_COMMON_SRCS) $(UNIT_SRCS) src/domain/functionB/src/function_b_manager.c src/control/src/error_demo_main.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(DOMAIN_COMMON_INC) $(FUNCTION_B_INC) $(UNIT_INCS) $(DOMAIN_COMMON_SRCS) src/domain/functionB/src/function_b_manager.c src/control/src/error_demo_main.c $(UNIT_SRCS) -o $@

test: all
	$(BUILD_DIR)/test_unit_a
	$(BUILD_DIR)/test_unit_b
	$(BUILD_DIR)/test_unit_c
	$(BUILD_DIR)/test_unit_d
	$(BUILD_DIR)/test_unit_e
	$(BUILD_DIR)/test_function_a_manager
	$(BUILD_DIR)/test_function_b_manager
	$(BUILD_DIR)/control_app
	$(BUILD_DIR)/control_error_app

run-unit-a: $(BUILD_DIR)/test_unit_a
	$(BUILD_DIR)/test_unit_a

run-unit-b: $(BUILD_DIR)/test_unit_b
	$(BUILD_DIR)/test_unit_b

run-unit-c: $(BUILD_DIR)/test_unit_c
	$(BUILD_DIR)/test_unit_c

run-unit-d: $(BUILD_DIR)/test_unit_d
	$(BUILD_DIR)/test_unit_d

run-unit-e: $(BUILD_DIR)/test_unit_e
	$(BUILD_DIR)/test_unit_e

run-function-a: $(BUILD_DIR)/test_function_a_manager
	$(BUILD_DIR)/test_function_a_manager

run-function-b: $(BUILD_DIR)/test_function_b_manager
	$(BUILD_DIR)/test_function_b_manager

run-control: $(BUILD_DIR)/control_app
	$(BUILD_DIR)/control_app

run-control-error: $(BUILD_DIR)/control_error_app
	$(BUILD_DIR)/control_error_app

clean:
	rm -rf $(BUILD_DIR)
