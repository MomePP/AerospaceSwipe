#define AEROSPACE_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

typedef struct aerospace aerospace;

aerospace* aerospace_new(const char* socketPath);

int aerospace_is_initialized(aerospace* client);

void aerospace_close(aerospace* client);

char* aerospace_switch(aerospace* client, const char* direction);

char* aerospace_workspace(aerospace* client, int wrap_around, const char* ws_command, const char* stdin_payload);

char* aerospace_list_workspaces(aerospace* client, bool include_empty);

// Name of the workspace currently visible on the monitor under the mouse
// cursor. Caller frees. NULL if it can't be resolved.
//
// Focus it with aerospace_workspace(client, 0, name, "") to re-anchor
// AeroSpace's focused monitor onto the one the cursor is over.
char* aerospace_mouse_visible_workspace(aerospace* client);

// Extracts a single workspace name from command output: trims surrounding
// whitespace and keeps only the first line. Caller frees. NULL when the
// output holds no name at all.
char* parse_workspace_name(const char* out);
