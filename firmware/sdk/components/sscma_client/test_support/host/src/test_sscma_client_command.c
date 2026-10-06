#include "sscma_client_command.h"
#include "sscma_client_sensor_parser.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void assert_command_name(const char *command, const char *expected) {
    char actual[32];
    sscma_client_command_name(command, actual, sizeof(actual));
    assert(strcmp(actual, expected) == 0);
}

static void assert_sensor_payload(const char *json, int expected_type, int expected_state, int expected_opt_id,
                                  const char *expected_opt_detail) {
    cJSON *payload = cJSON_Parse(json);
    int id = 0;
    int type = 0;
    int state = 0;
    int opt_id = 0;
    char *opt_detail = NULL;

    assert(payload != NULL);
    assert(sscma_client_parse_sensor_payload(payload, &id, &type, &state, &opt_id, &opt_detail));
    assert(id == 1);
    assert(type == expected_type);
    assert(state == expected_state);
    assert(opt_id == expected_opt_id);
    assert(opt_detail != NULL);
    assert(strcmp(opt_detail, expected_opt_detail) == 0);

    free(opt_detail);
    cJSON_Delete(payload);
}

static void assert_invalid_sensor_payload_preserves_output(const char *json) {
    cJSON *payload = cJSON_Parse(json);
    int id = 7;
    int type = 8;
    int state = 9;
    int opt_id = 10;
    char *opt_detail = (char *)"unchanged";

    assert(payload != NULL);
    assert(!sscma_client_parse_sensor_payload(payload, &id, &type, &state, &opt_id, &opt_detail));
    assert(id == 7);
    assert(type == 8);
    assert(state == 9);
    assert(opt_id == 10);
    assert(strcmp(opt_detail, "unchanged") == 0);

    cJSON_Delete(payload);
}

int main(void) {
    assert_command_name("AT+MODEL?\r\n", "MODEL");
    assert_command_name("AT+MODEL=3\r\n", "MODEL");
    assert_command_name("AT+INVOKE=-1,0,0\r\n", "INVOKE");
    assert_command_name("MODEL?\r\n", "MODEL");
    assert(sscma_client_command_name_equal("MODEL", "MODEL?"));
    assert(sscma_client_command_name_equal("AT+INFO?\r\n", "INFO"));
    assert(!sscma_client_command_name_equal("MODEL", "INFO?"));
    assert_sensor_payload("{\"type\":0,\"name\":\"SENSOR?\",\"code\":0,\"data\":{\"id\":1,\"type\":1,\"state\":1,"
                          "\"opt_id\":3,\"opt_detail\":\"640x480 Auto\"}}",
                          1, 1, 3, "640x480 Auto");
    assert_sensor_payload("{\"code\":0,\"data\":{\"sensor\":{\"id\":1,\"type\":1,\"state\":1,\"opt_id\":0,"
                          "\"opt_detail\":\"240x240 Auto\"}}}",
                          1, 1, 0, "240x240 Auto");
    assert_sensor_payload("{\"code\":0,\"data\":{\"id\":1,\"opt_id\":3,\"opt_detail\":\"640x480 Auto\"}}", 0, 0, 3,
                          "640x480 Auto");
    assert_invalid_sensor_payload_preserves_output(
        "{\"code\":0,\"data\":{\"id\":1,\"opt_detail\":\"missing opt_id\"}}");
    puts("sscma_client_command_host_tests: PASS");
    return 0;
}

