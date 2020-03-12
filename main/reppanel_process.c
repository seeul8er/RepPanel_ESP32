//
// Created by cyber on 01.03.20.
//

#include <stdio.h>
#include <custom_themes/lv_theme_rep_panel_light.h>
#include "reppanel_process.h"
#include "reppanel.h"

lv_obj_t *label_bed_temp;
lv_obj_t *ddlist_bed_temp_active;
lv_obj_t *ddlist_bed_temp_standby;
lv_obj_t *label_tool_temp;
lv_obj_t *label_extruder_name;
lv_obj_t *ddlist_tool_temp_active;
lv_obj_t *ddlist_tool_temp_standby;
lv_obj_t *button_tool_filament;
lv_obj_t *cont_main;
lv_obj_t *process_container;

static lv_obj_t *ddlist_selected_filament;
static lv_obj_t * cont_filament;

static int current_visible_heater_indx = 0;     // heater/tool/extruder that is currently visible within the UI

char reppanel_bed_temp[MAX_PREPANEL_TEMP_LEN];
char reppanel_tool_temp[MAX_PREPANEL_TEMP_LEN];

/**
 * Send command to RepRap
 * @param new_tmp
 * @param id DDLIST_BED_TMP_ACTIVE, DDLIST_BED_TMP_STANDBY etc.
 */
void set_heater_status(char *new_tmp, int id) {
    int selected_tool_idx = 0;                  // toolname that may will change temperature
    // TODO: Call to RepRap firmware
    switch (id) {
        case DDLIST_BED_TMP_ACTIVE:
            break;
        case DDLIST_BED_TMP_STANDBY:
            break;
        case DDLIST_TOOL_TMP_ACTIVE:
            break;
        case DDLIST_TOOL_TMP_STANDBY:
            break;
        default:
            break;
    }
}

/**
 * Update UI according to RepRap status for tools/heaters
 * @param state : 0,1,2,3
 * @param ddlist_active ddlist for active value
 * @param ddlist_standby ddlist for standby value
 */
void apply_heater_style(int state, lv_obj_t *ddlist_active, lv_obj_t *ddlist_standby) {
    static lv_style_t style_ddlist_off;
    lv_style_copy(&style_ddlist_off, lv_ddlist_get_style(ddlist_bed_temp_active, LV_DDLIST_STYLE_BG));
    style_ddlist_off.text.color = REP_PANEL_DARK_TEXT;
    static lv_style_t style_ddlist_act;
    lv_style_copy(&style_ddlist_act, lv_ddlist_get_style(ddlist_bed_temp_active, LV_DDLIST_STYLE_BG));
    style_ddlist_act.text.color = REP_PANEL_DARK_ACCENT;
    static lv_style_t style_ddlist_fault;
    lv_style_copy(&style_ddlist_fault, lv_ddlist_get_style(ddlist_bed_temp_active, LV_DDLIST_STYLE_BG));
    style_ddlist_fault.text.color = LV_COLOR_RED;

    if (state == 0) {        // off
        lv_ddlist_set_style(ddlist_active, LV_DDLIST_STYLE_BG, &style_ddlist_off);
        lv_ddlist_set_style(ddlist_standby, LV_DDLIST_STYLE_BG, &style_ddlist_off);
    } else if (state == 1) { // standby
        lv_ddlist_set_style(ddlist_active, LV_DDLIST_STYLE_BG, &style_ddlist_off);
        lv_ddlist_set_style(ddlist_standby, LV_DDLIST_STYLE_BG, &style_ddlist_act);
    } else if (state == 2) { // active
        lv_ddlist_set_style(ddlist_active, LV_DDLIST_STYLE_BG, &style_ddlist_act);
        lv_ddlist_set_style(ddlist_standby, LV_DDLIST_STYLE_BG, &style_ddlist_off);
    } else {                 // fault
        lv_ddlist_set_style(ddlist_active, LV_DDLIST_STYLE_BG, &style_ddlist_fault);
        lv_ddlist_set_style(ddlist_standby, LV_DDLIST_STYLE_BG, &style_ddlist_fault);
    }
}

/**
 * Update UI based on RepRap response
 */
void update_heater_status(const int states[MAX_NUM_TOOLS], int num_heaters) {
    for (int i = 0; i < num_heaters; i++) {
        if (i == 0) {
            apply_heater_style(states[i], ddlist_bed_temp_active, ddlist_bed_temp_standby);
        } else if (i == (current_visible_heater_indx+1)) {
            apply_heater_style(states[i], ddlist_tool_temp_active, ddlist_tool_temp_standby);
        }
    }
    memcpy(toolstates, states, MAX_NUM_TOOLS);
}

/**
 * Event handler for changes by the user to the heater temperatures
 * @param obj
 * @param event
 */
static void extruder_usr_tmp_change_event(lv_obj_t *obj, lv_event_t event) {
    char val_txt_buff[10];
    switch (event) {
        case LV_EVENT_VALUE_CHANGED:
            lv_ddlist_get_selected_str(obj, val_txt_buff, 10);
            printf("Value changed to %s on", val_txt_buff);
            break;
        case LV_EVENT_LONG_PRESSED:
            lv_ddlist_get_selected_str(obj, val_txt_buff, 10);
            int len = strlen(val_txt_buff);
            val_txt_buff[len - 2] = '\0';     // cut off °C
            set_heater_status(val_txt_buff, (int) obj->user_data);
            printf("Long press on");
            break;
        default:
            break;
    }
}

static void redraw_process_event_handler(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
        lv_obj_del_async(lv_obj_get_parent(cont_filament));
        cont_filament = NULL; /* happens before object is actually deleted! */
        draw_process(process_scr);
    }
}

static void load_filament_event_handler(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
        char val_txt_buff[100];
        lv_ddlist_get_selected_str(ddlist_selected_filament, val_txt_buff, 100);
        printf("Loading %s", val_txt_buff);
    }
}

static void unload_filament_event_handler(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
        char val_txt_buff[100];
        lv_ddlist_get_selected_str(ddlist_selected_filament, val_txt_buff, 100);
        printf("Unloading %s", val_txt_buff);
    }
}

static void filament_change_event(lv_obj_t *obj, lv_event_t event) {
    if (event == LV_EVENT_CLICKED) {
        lv_obj_clean(process_container);
        lv_cont_set_fit2(process_container, LV_FIT_FILL, LV_FIT_TIGHT);
        lv_cont_set_layout(process_container, LV_LAYOUT_COL_M);

        cont_filament = lv_cont_create(process_container, NULL);
        lv_cont_set_layout(cont_filament, LV_LAYOUT_COL_M);
        lv_cont_set_fit2(cont_filament, LV_FIT_FILL, LV_FIT_TIGHT);

        lv_obj_t *l = lv_label_create(cont_filament, NULL);
        lv_label_set_text(l, "Filaments");

        lv_obj_t *cont2 = lv_cont_create(cont_filament, NULL);
        lv_cont_set_layout(cont2, LV_LAYOUT_ROW_M);
        lv_cont_set_fit2(cont2, LV_FIT_FILL, LV_FIT_TIGHT);

        ddlist_selected_filament = lv_ddlist_create(cont2, NULL);
        lv_ddlist_set_options(ddlist_selected_filament, filament_names);
        lv_ddlist_set_draw_arrow(ddlist_selected_filament, true);
        lv_ddlist_set_fix_height(ddlist_selected_filament, 150);
        lv_ddlist_set_sb_mode(ddlist_selected_filament, LV_SB_MODE_AUTO);
        lv_obj_align(ddlist_selected_filament, cont2, LV_ALIGN_IN_TOP_MID, 0, 0);
        static lv_obj_t *button_load_filament;
        create_button(cont2, button_load_filament, "Load", load_filament_event_handler);
        static lv_obj_t *button_unload_filament;
        create_button(cont2, button_unload_filament, "Unload", unload_filament_event_handler);

        static lv_obj_t *button_exit;
        create_button(process_container, button_exit, "Ok", redraw_process_event_handler);

        lv_obj_align(cont_filament, process_container, LV_ALIGN_CENTER, 0, 0); /*Align to the corner*/
    }
}

/**
 * Draw process screen showing temperatures with possibility to change them. Stuff to edit FFF process related parameters
 * @param parent_screen Parent screen to draw elements on
 */
void draw_process(lv_obj_t *parent_screen) {
    process_container = lv_cont_create(parent_screen, NULL);
    lv_cont_set_layout(process_container, LV_LAYOUT_ROW_T);
    lv_cont_set_fit2(process_container, LV_FIT_TIGHT, LV_FIT_TIGHT);

    lv_obj_t *holder1 = lv_cont_create(process_container, NULL);
    lv_cont_set_fit(holder1, true);
    lv_cont_set_layout(holder1, LV_LAYOUT_COL_M);
    lv_obj_t *holder2 = lv_cont_create(process_container, NULL);
    lv_cont_set_fit(holder2, true);
    lv_cont_set_layout(holder2, LV_LAYOUT_COL_M);
    lv_obj_t *holder3 = lv_cont_create(process_container, NULL);
    lv_cont_set_fit(holder3, true);
    lv_cont_set_layout(holder3, LV_LAYOUT_COL_M);

    lv_obj_t *holder_empty = lv_cont_create(holder1, NULL);
    lv_cont_set_fit(holder_empty, true);
    lv_cont_set_layout(holder_empty, LV_LAYOUT_ROW_T);

    lv_obj_t *label_empty0 = lv_label_create(holder_empty, NULL);
    lv_label_set_text(label_empty0, "");
    lv_obj_t *label_empty1 = lv_label_create(holder1, NULL);
    lv_label_set_text(label_empty1, "");

    lv_obj_t *label_active = lv_label_create(holder1, NULL);
    lv_label_set_text(label_active, "Active");
    lv_obj_t *label_standby = lv_label_create(holder1, NULL);
    lv_label_set_text(label_standby, "Standby");

    lv_obj_t *holder_bed = lv_cont_create(holder2, NULL);
    lv_cont_set_fit(holder_bed, true);
    lv_cont_set_layout(holder_bed, LV_LAYOUT_ROW_T);

    lv_obj_t *label_bed = lv_label_create(holder_bed, NULL);
    lv_label_set_text(label_bed, "Bed");
    label_bed_temp = lv_label_create(holder2, NULL);
    lv_label_set_text(label_bed_temp, reppanel_bed_temp);

    ddlist_bed_temp_active = lv_ddlist_create(holder2, NULL);
    lv_ddlist_set_options(ddlist_bed_temp_active, bed_tmps_active);
    lv_ddlist_set_fix_width(ddlist_bed_temp_active, 130);
    lv_ddlist_set_draw_arrow(ddlist_bed_temp_active, false);
    lv_ddlist_set_fix_height(ddlist_bed_temp_active, 110);
    lv_ddlist_set_sb_mode(ddlist_bed_temp_active, LV_SB_MODE_AUTO);
    lv_obj_align(ddlist_bed_temp_active, holder2, LV_ALIGN_IN_TOP_MID, 0, 0);
    lv_obj_set_event_cb(ddlist_bed_temp_active, extruder_usr_tmp_change_event);
    lv_obj_set_user_data(ddlist_bed_temp_active, (lv_obj_user_data_t) DDLIST_BED_TMP_ACTIVE);

    ddlist_bed_temp_standby = lv_ddlist_create(holder2, NULL);
    lv_ddlist_set_options(ddlist_bed_temp_standby, bed_tmps_standby);
    lv_obj_set_auto_realign(ddlist_bed_temp_standby, true);
    lv_ddlist_set_fix_width(ddlist_bed_temp_standby, 130);
    lv_ddlist_set_fix_height(ddlist_bed_temp_standby, 110);
    lv_ddlist_set_draw_arrow(ddlist_bed_temp_standby, false);
    lv_ddlist_set_sb_mode(ddlist_bed_temp_standby, LV_SB_MODE_AUTO);
    lv_obj_align(ddlist_bed_temp_standby, holder2, LV_ALIGN_IN_BOTTOM_MID, 0, 0);
    lv_obj_set_event_cb(ddlist_bed_temp_standby, extruder_usr_tmp_change_event);
    lv_obj_set_user_data(ddlist_bed_temp_standby, (lv_obj_user_data_t) DDLIST_BED_TMP_STANDBY);

    lv_obj_t *holder_extruder = lv_cont_create(holder3, NULL);
    lv_cont_set_fit(holder_extruder, true);
    lv_cont_set_layout(holder_extruder, LV_LAYOUT_ROW_M);

    lv_obj_t *prev_extruder_label = lv_label_create(holder_extruder, NULL);
    lv_label_set_text(prev_extruder_label, LV_SYMBOL_LEFT);

    label_extruder_name = lv_label_create(holder_extruder, NULL);
    lv_label_set_text(label_extruder_name, tool_names_map[0]);

    lv_obj_t *next_extruder_label = lv_label_create(holder_extruder, NULL);
    lv_label_set_text(next_extruder_label, LV_SYMBOL_RIGHT);

    label_tool_temp = lv_label_create(holder3, NULL);
    lv_label_set_text(label_tool_temp, reppanel_tool_temp);

    ddlist_tool_temp_active = lv_ddlist_create(holder3, NULL);
    lv_ddlist_set_options(ddlist_tool_temp_active, extruder_tmps_active);
    lv_ddlist_set_fix_width(ddlist_tool_temp_active, 130);
    lv_ddlist_set_fix_height(ddlist_tool_temp_active, 110);
    lv_ddlist_set_draw_arrow(ddlist_tool_temp_active, false);
    lv_ddlist_set_sb_mode(ddlist_tool_temp_active, LV_SB_MODE_AUTO);
    lv_obj_align(ddlist_tool_temp_active, holder3, LV_ALIGN_IN_TOP_MID, 0, 0);
    lv_obj_set_event_cb(ddlist_tool_temp_active, extruder_usr_tmp_change_event);
    lv_obj_set_user_data(ddlist_tool_temp_active, (lv_obj_user_data_t) DDLIST_TOOL_TMP_ACTIVE);

    ddlist_tool_temp_standby = lv_ddlist_create(holder3, NULL);
    lv_ddlist_set_options(ddlist_tool_temp_standby, extruder_tmps_standby);
    lv_ddlist_set_fix_width(ddlist_tool_temp_standby, 130);
    lv_ddlist_set_fix_height(ddlist_tool_temp_standby, 110);
    lv_ddlist_set_draw_arrow(ddlist_tool_temp_standby, false);
    lv_ddlist_set_sb_mode(ddlist_tool_temp_standby, LV_SB_MODE_AUTO);
    lv_obj_align(ddlist_tool_temp_standby, holder3, LV_ALIGN_IN_TOP_MID, 0, 0);
    lv_obj_set_event_cb(ddlist_tool_temp_standby, extruder_usr_tmp_change_event);
    lv_obj_set_user_data(ddlist_tool_temp_standby, (lv_obj_user_data_t) DDLIST_TOOL_TMP_STANDBY);

    create_button(holder3, button_tool_filament, "Filament", filament_change_event);

    // light big font for current temps
    static lv_style_t style_label;
    lv_style_copy(&style_label, &lv_style_plain);
    style_label.text.color = REP_PANEL_DARK_HIGHLIGHT;
    style_label.text.font = &reppanel_font_roboto_light_36;
    lv_obj_set_style(label_empty1, &style_label);
    lv_obj_set_style(label_bed_temp, &style_label);
    lv_obj_set_style(label_tool_temp, &style_label);

    // Bold font for headings
    static lv_style_t style_label_heading;
    lv_style_copy(&style_label_heading, &lv_style_plain);
    style_label_heading.text.color = REP_PANEL_DARK_TEXT;
    style_label_heading.text.font = &reppanel_font_roboto_bold_22;
    lv_obj_set_style(label_extruder_name, &style_label_heading);
    lv_obj_set_style(label_bed, &style_label_heading);

    // built in icon font to display arrows
    static lv_style_t style_label_icon;
    lv_style_copy(&style_label_icon, &lv_style_plain);
    style_label_icon.text.font = &lv_font_roboto_22;
    style_label_icon.text.color = REP_PANEL_DARK_ACCENT;
    lv_obj_set_style(prev_extruder_label, &style_label_icon);
    lv_obj_set_style(next_extruder_label, &style_label_icon);
}