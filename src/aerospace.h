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

// Monitor id of the display the mouse cursor is currently over, or -1 if it
// can't be resolved.
int aerospace_mouse_monitor(aerospace* client);

// Monitor id that currently holds focus, or -1.
int aerospace_focused_monitor(aerospace* client);

// Window id that currently holds focus, or -1 when the focused workspace is
// empty.
int aerospace_focused_window(aerospace* client);

// Makes monitor_id the focused monitor, so the monitor-relative commands
// above act on it. Returns false if AeroSpace rejected the command.
bool aerospace_focus_monitor(aerospace* client, int monitor_id);

// Returns focus to a specific window. Returns false if AeroSpace rejected the
// command — e.g. the window has since been closed.
bool aerospace_focus_window(aerospace* client, int window_id);

// Parses a non-negative id out of `--format %{...-id}` output, tolerating
// surrounding whitespace. Returns -1 for anything that isn't a non-negative
// integer — including the error text a failed query returns.
int parse_id(const char* out);
