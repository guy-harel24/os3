#include <stdlib.h>
#include <string.h>
#include "log.h"

// Opaque struct definition
struct Server_Log {
    // TODO: Implement internal log storage (e.g., dynamic buffer, linked list, etc.)
};

// Creates a new server log instance (stub)
server_log create_log() {
    // TODO: Allocate and initialize internal log structure
    return (server_log)malloc(sizeof(struct Server_Log));
}

// Destroys and frees the log (stub)
void destroy_log(server_log log) {
    // TODO: Free all internal resources used by the log
    free(log);
}

// Returns dummy log content as string (stub)
int get_log(server_log log, char** dst) {
    // TODO: Return the full contents of the log as a dynamically allocated string
    // This function should handle concurrent access

    const char* dummy = "Log is not implemented.\n";
    int len = strlen(dummy);
    *dst = (char*)malloc(len + 1); // Allocate for caller
    if (*dst != NULL) {
        strcpy(*dst, dummy);
    }
    return len;
}

// Appends a new entry to the log (no-op stub)
void add_to_log(server_log log, const char* data, int data_len) {
    // TODO: Append the provided data to the log
    // This function should handle concurrent access
}
