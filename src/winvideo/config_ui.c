/* Native Win16 frontend to the same forms, presets and save engine as PIDOSCFG. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "video.h"
#include "forms.h"

#define CONFIG_PATH "C:\\SYSTEM\\PIDOS.CFG"
#define FIELD_CPU 30
#define VALUE_LIMIT 640
typedef struct { Field fields[FORM_MAX_FIELDS]; int page, branch, count, root; } PageState;
typedef struct { const Field *field; int count; } EditState;
/* Only one value editor is open at a time. A global avoids a large Win16 stack. */
static char (FAR *choice_values)[CFG_VALUE_SIZE];

static void add_choice(HWND dialog, EditState *state, const char *value, const char *label)
{
    int index;
    if (state->count >= VALUE_LIMIT) return;
    index = (int)SendDlgItemMessage(dialog, IDC_VALUES, CB_ADDSTRING, 0, (LPARAM)(LPSTR)label);
    if (index == CB_ERR || index == CB_ERRSPACE) return;
    strcpy(choice_values[state->count], value);
    SendDlgItemMessage(dialog, IDC_VALUES, CB_SETITEMDATA, index, state->count++);
}

static void parse_choice(HWND dialog, EditState *state, char *line)
{
    char *label;
    line[strcspn(line, "\r\n")] = 0;
    if (!*line || *line == '#' || *line == ';') return;
    label = strchr(line, '\t');
    if (!label) label = strchr(line, '=');
    if (label) *label++ = 0;
    else label = line;
    if (strlen(line) < CFG_VALUE_SIZE) add_choice(dialog, state, line, label);
}

static void fill_values(HWND dialog, EditState *state)
{
    const Field *field = state->field;
    const char *p;
    char text[200], path[144];
    int i;
    unsigned length;
    FILE *file;
    if (field->type == FIELD_CPU) {
        for (i = 0; i < PROFILE_COUNT; ++i) {
            sprintf(text, "%d", i);
            add_choice(dialog, state, text, cpu_profiles[i].label);
        }
    } else if (field->type == FIELD_CARD) {
        for (i = 0; i < CARD_COUNT; ++i) {
            sprintf(text, "%d", i); add_choice(dialog, state, text, card_names[i]);
        }
    } else if (field->type == FIELD_VIDEO) {
        for (i = 0; i < VIDEO_PROFILE_COUNT; ++i)
            add_choice(dialog, state, video_profiles[i].id, video_profiles[i].label);
    } else if (field->type == FIELD_MIDI) {
        for (i = 0; i < MIDI_PROFILE_COUNT; ++i)
            add_choice(dialog, state, midi_profiles[i].id, midi_profiles[i].label);
    } else if (field->type == FIELD_CHECKBOX) {
        add_choice(dialog, state, "false", "Disabled"); add_choice(dialog, state, "true", "Enabled");
    } else if (field->type == FIELD_VOODOO) {
        add_choice(dialog, state, "false", "Disabled"); add_choice(dialog, state, "software", "Enabled");
    } else if (field->catalog) {
        sprintf(path, "Z:\\BIN\\%s", field->catalog);
        file = fopen(path, "r");
        if (file) {
            while (fgets(text, sizeof(text), file)) parse_choice(dialog, state, text);
            fclose(file);
        }
    } else if (field->choices) {
        p = field->choices;
        while (*p) {
            length = 0;
            while (*p && *p != '|') { if (length < sizeof(text) - 1) text[length++] = *p; ++p; }
            if (*p) ++p;
            text[length] = 0; parse_choice(dialog, state, text);
        }
    }
}

static const char *field_value(const Field *field)
{
    int index;
    if (field->type == FIELD_CPU) {
        index = cfg_profile(); return index >= 0 ? cpu_profiles[index].label : "Custom";
    }
    if (field->type == FIELD_CARD) {
        index = cfg_card(); return index >= 0 ? card_names[index] : "Custom";
    }
    if (field->key >= 0) return cfg_get(field->key);
    return "Open...";
}

BOOL FAR PASCAL __export ValueDialog(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam)
{
    EditState FAR *state = (EditState FAR *)GetWindowLong(dialog, DWL_USER);
    const Field *field;
    char value[CFG_VALUE_SIZE];
    int i, index, current = -1, ok = 1;
    if (message == WM_INITDIALOG) {
        state = (EditState FAR *)lparam;
        SetWindowLong(dialog, DWL_USER, lparam);
        field = state->field;
        SetWindowText(dialog, field->label); SetDlgItemText(dialog, IDC_HELP, field->help);
        SendDlgItemMessage(dialog, IDC_VALUE, EM_LIMITTEXT, CFG_VALUE_SIZE - 1, 0);
        fill_values(dialog, state);
        if (field->type == FIELD_CPU) current = cfg_profile();
        else if (field->type == FIELD_CARD) current = cfg_card();
        else for (i = 0; i < state->count; ++i)
            if (!stricmp(choice_values[i], field_value(field))) current = i;
        SendDlgItemMessage(dialog, IDC_VALUES, CB_SETCURSEL, current, 0);
        SetDlgItemText(dialog, IDC_VALUE, field->key >= 0 ? cfg_get(field->key) : "");
        if (!state->count) ShowWindow(GetDlgItem(dialog, IDC_VALUES), SW_HIDE);
        if (state->count && field->type != FIELD_VOLUME && !field->catalog)
            ShowWindow(GetDlgItem(dialog, IDC_VALUE), SW_HIDE);
        SetDlgItemText(dialog, IDC_STATUS, field->type == FIELD_MIDI_PORT ?
            "Use the USB MIDI identity already selected in DOS PIDOSCFG.\n"
            "Device discovery is available in PIDOSCFG outside Windows." :
            "Changes here are saved only when you click Save in Pi-DOS settings.");
        return TRUE;
    }
    if (!state) return FALSE;
    field = state->field;
    if (message == WM_COMMAND) {
        if (wparam == IDCANCEL) { EndDialog(dialog, 0); return TRUE; }
        if (wparam == IDC_VALUES && HIWORD(lparam) == CBN_SELCHANGE) {
            index = (int)SendDlgItemMessage(dialog, IDC_VALUES, CB_GETCURSEL, 0, 0);
            if (index != CB_ERR) {
                index = (int)SendDlgItemMessage(dialog, IDC_VALUES, CB_GETITEMDATA, index, 0);
                SetDlgItemText(dialog, IDC_VALUE, choice_values[index]);
            }
            return TRUE;
        }
        if (wparam == IDOK) {
            if (IsWindowVisible(GetDlgItem(dialog, IDC_VALUE)))
                GetDlgItemText(dialog, IDC_VALUE, value, sizeof(value));
            else {
                index = (int)SendDlgItemMessage(dialog, IDC_VALUES, CB_GETCURSEL, 0, 0);
                if (index == CB_ERR) return TRUE;
                index = (int)SendDlgItemMessage(dialog, IDC_VALUES, CB_GETITEMDATA, index, 0);
                strcpy(value, choice_values[index]);
            }
            if (field->key == K_TIMEZONE) {
                for (i = 0; i < state->count; ++i)
                    if (!strcmp(value, choice_values[i])) break;
                if (i == state->count) {
                    MessageBox(dialog, "Choose an installed timezone from the list.",
                               "Pi-DOS settings", MB_OK | MB_ICONEXCLAMATION);
                    return TRUE;
                }
            }
            if (field->type == FIELD_CPU) ok = cfg_select_cpu(atoi(value));
            else if (field->type == FIELD_CARD) cfg_select_card(atoi(value));
            else if (field->type == FIELD_MIDI) cfg_select_midi(midi_profile_find(value));
            else if (field->type == FIELD_VOODOO) {
                if (strcmp(value, cfg_get(K_VOODOO))) cfg_toggle_voodoo();
            } else {
                ok = cfg_set(field->key, value);
                if (ok && field->key == K_RAM) ok = cfg_set(K_RAM_KB, "0");
                if (ok && field->key == K_MIDI_PORT && cfg_midi() == MIDI_EXTERNAL)
                    ok = cfg_set(K_MIDI_CONFIG, value);
            }
            if (ok) EndDialog(dialog, 1);
            else MessageBox(dialog, cfg_error, "Pi-DOS settings", MB_OK | MB_ICONEXCLAMATION);
            return TRUE;
        }
    }
    return FALSE;
}

static void fill_fields(HWND dialog, PageState FAR *state, int selected)
{
    char text[180];
    int i;
    if (state->branch == FIELD_MIDI_SETTINGS) state->count = form_midi_fields(state->fields);
    else if (state->branch) state->count = form_network_fields(state->branch, state->fields);
    else if (state->page == PAGE_CPU) {
        Field *f = &state->fields[0];
        f->key = -1; f->type = FIELD_CPU; f->label = "CPU profile"; f->choices = NULL; f->catalog = NULL;
        f->help = "Select the same CPU profiles as DOS PIDOSCFG. Reboot Pi-DOS to apply.";
        state->count = 1;
    } else if (state->page == PAGE_MEMORY) {
        Field *f = &state->fields[0];
        f->key = K_RAM; f->type = FIELD_CHOICE; f->label = "Memory (MiB)";
        f->choices = "1|2|4|8|16|32|64|128"; f->catalog = NULL;
        f->help = "RAM available to DOS and Windows. CPU address limits are checked when saving.";
        state->count = 1;
    } else state->count = form_fields(state->page, state->fields);
    SendDlgItemMessage(dialog, IDC_FIELDS, LB_RESETCONTENT, 0, 0);
    for (i = 0; i < state->count; ++i) {
        sprintf(text, "%s: %s", state->fields[i].label, field_value(&state->fields[i]));
        SendDlgItemMessage(dialog, IDC_FIELDS, LB_ADDSTRING, 0, (LPARAM)(LPSTR)text);
    }
    if (selected >= state->count) selected = state->count - 1;
    if (selected < 0) selected = 0;
    SendDlgItemMessage(dialog, IDC_FIELDS, LB_SETCURSEL, selected, 0);
    SetDlgItemText(dialog, IDC_HELP, state->count ? state->fields[selected].help : "");
}

BOOL FAR PASCAL __export OptionsDialog(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam)
{
    PageState FAR *state = (PageState FAR *)GetWindowLong(dialog, DWL_USER);
    PageState child;
    EditState edit;
    Field *field;
    int i, selected;
    if (message == WM_INITDIALOG) {
        state = (PageState FAR *)lparam; SetWindowLong(dialog, DWL_USER, lparam);
        for (i = 0; i < PAGE_COUNT; ++i)
            SendDlgItemMessage(dialog, IDC_PAGE, CB_ADDSTRING, 0, (LPARAM)(LPSTR)page_names[i]);
        SendDlgItemMessage(dialog, IDC_PAGE, CB_SETCURSEL, state->page, 0);
        if (!state->root) {
            EnableWindow(GetDlgItem(dialog, IDC_PAGE), FALSE);
            EnableWindow(GetDlgItem(dialog, IDC_DEFAULTS), FALSE);
            SetDlgItemText(dialog, IDOK, "Done");
        }
        fill_fields(dialog, state, 0); return TRUE;
    }
    if (!state) return FALSE;
    if (message == WM_COMMAND) {
        if (wparam == IDCANCEL) {
            if (state->root && cfg_changed() && MessageBox(dialog, "Discard the unsaved Pi-DOS settings?",
                    "Pi-DOS settings", MB_OKCANCEL | MB_ICONQUESTION) != IDOK) return TRUE;
            EndDialog(dialog, 0); return TRUE;
        }
        if (wparam == IDOK) {
            if (!state->root) { EndDialog(dialog, 1); return TRUE; }
            if (!cfg_save_changes(CONFIG_PATH))
                MessageBox(dialog, cfg_error, "Pi-DOS settings", MB_OK | MB_ICONSTOP);
            else {
                MessageBox(dialog, "Settings saved. Exit Windows and reboot Pi-DOS to apply.\n"
                           "Windows driver installation is a separate choice on the main page.",
                           "Pi-DOS settings", MB_OK | MB_ICONINFORMATION);
                EndDialog(dialog, 1);
            }
            return TRUE;
        }
        if (wparam == IDC_PAGE && HIWORD(lparam) == CBN_SELCHANGE) {
            state->page = (int)SendDlgItemMessage(dialog, IDC_PAGE, CB_GETCURSEL, 0, 0);
            fill_fields(dialog, state, 0); return TRUE;
        }
        if (wparam == IDC_DEFAULTS) {
            if (MessageBox(dialog, "Reset this category to its Pi-DOS defaults?", "Pi-DOS settings",
                           MB_OKCANCEL | MB_ICONQUESTION) == IDOK) {
                cfg_reset_page(state->page); fill_fields(dialog, state, 0);
            }
            return TRUE;
        }
        selected = (int)SendDlgItemMessage(dialog, IDC_FIELDS, LB_GETCURSEL, 0, 0);
        if (selected < 0 || selected >= state->count) return FALSE;
        field = &state->fields[selected];
        if (wparam == IDC_FIELDS && HIWORD(lparam) == LBN_SELCHANGE) {
            SetDlgItemText(dialog, IDC_HELP, field->help); return TRUE;
        }
        if (wparam == IDC_EDIT || (wparam == IDC_FIELDS && HIWORD(lparam) == LBN_DBLCLK)) {
            if (field->type == FIELD_MIDI_SETTINGS || field->type == FIELD_NET_NIC ||
                    field->type == FIELD_NET_TCP || field->type == FIELD_NET_IPX) {
                memset(&child, 0, sizeof(child)); child.page = state->page; child.branch = field->type;
                DialogBoxParam(pv_instance(), MAKEINTRESOURCE(IDD_OPTIONS), dialog, OptionsDialog, (LPARAM)&child);
            } else {
                edit.field = field; edit.count = 0;
                DialogBoxParam(pv_instance(), MAKEINTRESOURCE(IDD_VALUE), dialog, ValueDialog, (LPARAM)&edit);
            }
            fill_fields(dialog, state, selected); return TRUE;
        }
    }
    return FALSE;
}

void pv_options(HWND owner)
{
    PageState state;
    HGLOBAL choices;
    FILE *file = fopen(CONFIG_PATH, "rb");
    if (!file) {
        MessageBox(owner, "C:\\SYSTEM\\PIDOS.CFG is unavailable. Run this page inside Pi-DOS.",
                   "Pi-DOS settings", MB_OK | MB_ICONSTOP); return;
    }
    fclose(file);
    if (!cfg_load(CONFIG_PATH)) { MessageBox(owner, cfg_error, "Pi-DOS settings", MB_OK | MB_ICONSTOP); return; }
    choices = GlobalAlloc(GMEM_MOVEABLE, (DWORD)VALUE_LIMIT * CFG_VALUE_SIZE);
    choice_values = choices ? GlobalLock(choices) : NULL;
    if (!choice_values) {
        if (choices) GlobalFree(choices);
        MessageBox(owner, "Not enough Windows memory for the settings lists.", "Pi-DOS settings", MB_OK | MB_ICONSTOP);
        return;
    }
    memset(&state, 0, sizeof(state)); state.root = 1;
    DialogBoxParam(pv_instance(), MAKEINTRESOURCE(IDD_OPTIONS), owner, OptionsDialog, (LPARAM)&state);
    GlobalUnlock(choices); GlobalFree(choices); choice_values = NULL;
}
