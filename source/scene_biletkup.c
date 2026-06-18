#include "scene_biletkup.h"
#include "scene_manager.h"
#include "koleo_api.h"
#include "drawing.h"
#include "utils.h"
#include "audio_shit.h"
#include "scene_mainmenu.h"
#include "sprites.h"
#include "button.h" 


long long biletkup_connection_id = -1;
static long long current_payment_id = -1;

typedef enum {
    STATE_LOADING_DETAILS,
    STATE_DETAILS,
    STATE_LOADING_COMPOSITION,
    STATE_CLASS_SELECT,
    STATE_LOADING_LAYOUT,
    STATE_SEAT_SELECT,
    STATE_RESERVING,
    STATE_PAYMENT_SELECT,
    STATE_PAYING,
    STATE_AWAITING_BLIK_ORDER,    
    STATE_AWAITING_ORDER_READY,   
    STATE_SUCCESS
} BiletKupState;

static char current_blik_code[7] = "";
static long long current_order_id = -1;
static int poll_timer = 0; 

static BiletKupState current_state = STATE_LOADING_DETAILS;

static ConnectionPlaceTypes conn_places = {0};
static TrainComposition train_comp = {0};
static int selected_class_idx = 0;
static int selected_carriage_idx = -1;
static int class_option_count = 0;
static int carriage_option_count = 0;

static float fade_alpha = 255.0f;
static float spinner_angle = 0.0f;
static PolaczenieSzczegoly details = {0};
static float current_price = 0.0f;

static GFX_TEXTBUF static_buf;
static GFX_TEXTBUF dyn_buf;
static GFX_TEXTBUF seat_buf; 
static GFX_TEXT title_text, hint_text, train_text, btn_rez_text, btn_fast_rez_text, btn_blik_text, btn_konto_text, status_text;
static GFX_TEXT seat_hint_text, btn_confirm_seat_text; 
static GFX_TEXT class_title_text, btn_class1_text, btn_class2_text; 

static float scroll_x = 0.0f, last_touch_x = 0.0f, last_touch_y = 0.0f;
static float scroll_velocity = 0.0f; 
static bool touching = false, touch_consumed = false, is_scrolling = false;
static float train_min_x = 0, train_max_x = 320, train_max_y = 0; 
static bool prevTouchHeld = false; 
static u16 cached_touch_x = 0;
static u16 cached_touch_y = 0;

#define MAX_STOPS 64
static GFX_TEXT stop_texts[MAX_STOPS];
static int rendered_stops_count = 0;

#define MAX_TARIFFS 3 
#define MAX_TARIFF_IDS 8

typedef struct {
    int ids[MAX_TARIFF_IDS];
    int id_count;
    float price;
    char name[64];
} Tariff;

static Tariff available_tariffs[MAX_TARIFFS];
static int num_tariffs = 0;
static int selected_tariff_idx = 0;

static Button btn_tariffs[MAX_TARIFFS];
static Button btn_fast_res;
static Button btn_map_res;
static Button btn_classes[4];
static Button btn_confirm_seat;
static Button btn_blik;
static Button btn_konto;

static KoleoTrainLayout current_layout = {0};
#define MAX_LOADED_CARRIAGES 16
static KoleoTrainLayout loaded_layouts[MAX_LOADED_CARRIAGES];
static float carriage_offsets[MAX_LOADED_CARRIAGES];
static char loaded_carriage_nrs[MAX_LOADED_CARRIAGES][16];
static int loaded_carriage_count = 0;
static GFX_TEXT carriage_labels[MAX_LOADED_CARRIAGES];

#define MAX_SELECTED 8
static int selected_seats[MAX_SELECTED];
static const char* selected_carriage_nrs_ptrs[MAX_SELECTED];
static int selected_compartment_types[MAX_SELECTED]; 
static int selected_seat_count = 0;
static int selected_seat_nr = -1;

static GFX_TEXT seat_texts[150]; 
static int seat_text_count = 0;

static GFX_TEXT top_price_texts[MAX_TARIFFS];
static GFX_TEXT tariff_texts[MAX_TARIFFS];
static GFX_TEXT tariff_price_texts[MAX_TARIFFS];
static GFX_TEXT class_option_texts[4];
static GFX_TEXT carriage_option_texts[16];

static void prepare_class_option_texts(void) {
    for (int i = 0; i < class_option_count; i++) {
        char buffer[64];
        
        const TicketClass *opt = &conn_places.trains[0].classes[i];
        snprintf(buffer, sizeof(buffer), "%s %.2f PLN", opt->name, opt->price);
        GFX_TextParse(&class_option_texts[i], dyn_buf, buffer);
        GFX_TextOptimize(&class_option_texts[i]);
    }
}

static void prepare_carriage_option_texts(void) {
    for (int i = 0; i < carriage_option_count; i++) {
        char buffer[64];
        const Carriage *car = &train_comp.carriages[i];
        snprintf(buffer, sizeof(buffer), "Wagon %s %s", car->number, car->bookable ? "(OK)" : "(Niedostępny)");
        GFX_TextParse(&carriage_option_texts[i], dyn_buf, buffer);
        GFX_TextOptimize(&carriage_option_texts[i]);
    }
}

static void parse_tariffs_from_json(const char* json) {
    num_tariffs = 0;
    if (!json) return;

    const char* p = json;
    log_to_file("Parsuje taryfy");

    while ((p = strstr(p, "\"value\"")) != NULL && num_tariffs < MAX_TARIFFS) {
        log_to_file("Znaleziono pole \"value\" dla taryfy %d", num_tariffs + 1);

        const char* value_ptr = strchr(p, ':');
        if (!value_ptr) break;

        value_ptr++;
        while (*value_ptr == ' ' || *value_ptr == '\t' || *value_ptr == '\n' || *value_ptr == '\r') {
            value_ptr++;
        }

        if (*value_ptr == '"') value_ptr++;
        available_tariffs[num_tariffs].price = atof(value_ptr);

        available_tariffs[num_tariffs].name[0] = '\0';
        const char* name_ptr = strstr(p, "\"tariff_names\"");

        if (name_ptr) {
            name_ptr = strchr(name_ptr, '[');
            if (name_ptr) {
                name_ptr = strchr(name_ptr, '"');
                if (name_ptr) {
                    name_ptr++;
                    const char* end = strchr(name_ptr, '"');
                    if (end) {
                        int len = (int)(end - name_ptr);
                        if (len > 63) len = 63;
                        strncpy(available_tariffs[num_tariffs].name, name_ptr, len);
                        available_tariffs[num_tariffs].name[len] = '\0';
                    }
                }
            }
        }

        available_tariffs[num_tariffs].id_count = 0;
        const char* id_ptr = strstr(p, "\"tariff_ids\"");

        if (id_ptr) {
            id_ptr = strchr(id_ptr, '[');
            if (id_ptr) {
                id_ptr++;
                while (*id_ptr && *id_ptr != ']' && available_tariffs[num_tariffs].id_count < MAX_TARIFF_IDS) {
                    available_tariffs[num_tariffs].ids[available_tariffs[num_tariffs].id_count++] = atoi(id_ptr);
                    
                    const char* next_comma = strchr(id_ptr, ',');
                    const char* end_bracket = strchr(id_ptr, ']');
                    
                    if (next_comma && (!end_bracket || next_comma < end_bracket)) {
                        id_ptr = next_comma + 1;
                    } else {
                        break;
                    }
                }
            }
        }

        num_tariffs++;
        p++;
    }
}

static int get_required_seats() {
    int req = 0;
    for (int i = 0; i < global_lista.count; i++) {
        if (global_lista.ziomy[i].selected) req++;
    }
    return req > 0 ? req : 1; 
}

void sceneBiletkupInit(void) {
    stopAudio(2);  
    for(int i = 0; i < loaded_carriage_count; i++) {
        koleo_api_free_layout(&loaded_layouts[i]);
    }
    
    loaded_carriage_count = 0;
    fade_alpha = 255.0f;
    bgm_playing = false;  
    spinner_angle = 0.0f;
    current_state = STATE_LOADING_DETAILS;
    scroll_x = 0.0f;
    rendered_stops_count = 0;
    current_price = 0.0f;
    current_payment_id = -1;
    num_tariffs = 0;
    selected_tariff_idx = 0;
    selected_seat_nr = -1; 
    selected_class_idx = 0;
    selected_carriage_idx = -1;
    class_option_count = 0;
    carriage_option_count = 0;
    
    touching = false;
    touch_consumed = false;
    is_scrolling = false;
    prevTouchHeld = false;
    
    seat_text_count = 0;
    
    memset(&details, 0, sizeof(details));
    memset(available_tariffs, 0, sizeof(available_tariffs));
    memset(&current_layout, 0, sizeof(current_layout));
    if (train_comp.carriages) {
        free(train_comp.carriages);
        train_comp.carriages = NULL;
        train_comp.count = 0;
    }
    memset(&conn_places, 0, sizeof(conn_places));

    static_buf = GFX_TextBufNew(256);
    dyn_buf = GFX_TextBufNew(4096);
    seat_buf = GFX_TextBufNew(2048);

    GFX_TextParse(&title_text, static_buf, "SZCZEGÓŁY POŁĄCZENIA");
    GFX_TextParse(&hint_text, static_buf, "B = Powrót");
    GFX_TextParse(&btn_rez_text, static_buf, "Wybierz Miejsce (BETA)"); 
    GFX_TextParse(&btn_fast_rez_text, static_buf, "Szybka Rezerwacja");
    GFX_TextParse(&btn_confirm_seat_text, static_buf, "Potwierdź Miejsce"); 
    GFX_TextParse(&seat_hint_text, static_buf, "Przesuń mapę pociągu (Dolny Ekran)"); 
    GFX_TextParse(&class_title_text, static_buf, "Wybierz Klasę"); 
    GFX_TextParse(&btn_class1_text, static_buf, "1 Klasa"); 
    GFX_TextParse(&btn_class2_text, static_buf, "2 Klasa"); 
    GFX_TextParse(&btn_blik_text, static_buf, "BLIK");
    GFX_TextParse(&btn_konto_text, static_buf, "Stan Konta");
    GFX_TextParse(&status_text, static_buf, "Płatność udana!");
    
    GFX_TextOptimize(&title_text); GFX_TextOptimize(&hint_text);
    GFX_TextOptimize(&btn_rez_text); GFX_TextOptimize(&btn_confirm_seat_text);
    GFX_TextOptimize(&seat_hint_text); GFX_TextOptimize(&class_title_text);
    GFX_TextOptimize(&btn_class1_text); GFX_TextOptimize(&btn_class2_text);
    GFX_TextOptimize(&btn_blik_text); GFX_TextOptimize(&btn_konto_text); GFX_TextOptimize(&status_text);
    GFX_TextOptimize(&btn_fast_rez_text);

    for(int i = 0; i < MAX_TARIFFS; i++) {
        Create_button(&btn_tariffs[i], 20, 20 + (i * 50), 280, 45, NULL, NULL, GFX_COLOR_RGBA(100, 100, 100, 255), GFX_COLOR_RGBA(120, 120, 120, 255), 0.8f);
    }
    Create_button(&btn_fast_res, 20, 180, 130, 40, NULL, NULL, GFX_COLOR_RGBA(48, 130, 74, 255), GFX_COLOR_RGBA(68, 150, 94, 255), 0.8f);
    Create_button(&btn_map_res, 170, 180, 130, 40, NULL, NULL, GFX_COLOR_RGBA(48, 74, 130, 255), GFX_COLOR_RGBA(68, 94, 150, 255), 0.8f);
    for(int i = 0; i < 4; i++) {
        Create_button(&btn_classes[i], 40, 40 + (i * 45), 240, 38, NULL, NULL, GFX_COLOR_RGBA(80, 120, 200, 255), GFX_COLOR_RGBA(100, 140, 220, 255), 0.8f);
    }
    Create_button(&btn_confirm_seat, 60, 200, 200, 35, NULL, NULL, GFX_COLOR_RGBA(100, 100, 100, 255), GFX_COLOR_RGBA(120, 120, 120, 255), 0.8f);
    Create_button(&btn_blik, 40, 40, 240, 60, NULL, NULL, GFX_COLOR_RGBA(200, 20, 20, 255), GFX_COLOR_RGBA(220, 40, 40, 255), 0.8f);
    Create_button(&btn_konto, 40, 120, 240, 60, NULL, NULL, GFX_COLOR_RGBA(40, 40, 200, 255), GFX_COLOR_RGBA(60, 60, 220, 255), 0.8f);

    connection_details_buf.done = false;
    connection_price_buf.done = false;
    flat_train_place_types_buf.done = false;
    train_composition_buf.done = false;
    seats_availability_buf.done = false;

    if (biletkup_connection_id > 0) {
        get_ConnectionDetails(biletkup_connection_id);
        get_ConnectionPrice(biletkup_connection_id);
    }
    playAudio(4, true, 0.4f);
    current_blik_code[0] = '\0';
    current_order_id = -1;
    poll_timer = 0;
    nwm_blik.done = false;
    pobierz_bilet.done = false;
}

void sceneBiletkupUpdate(uint32_t kDown, uint32_t kHeld) {
    if (kDown & KEY_B) {
        sceneManagerSwitchTo(SCENE_POLACZENIA);
        return;
    }

    if (fade_alpha > 0.0f) fade_alpha -= 5.0f;

    touchPosition touch;
    hidTouchRead(&touch);
    uint32_t kUp = hidKeysUp();
    
    bool touchDown = (kDown & KEY_TOUCH) != 0;
    bool touchHeld = (kHeld & KEY_TOUCH) != 0;
    bool touchUp   = (!touchHeld && prevTouchHeld);
    prevTouchHeld  = touchHeld;

    if (touchDown || touchHeld) {
        cached_touch_x = touch.px;
        cached_touch_y = touch.py;
    }

    if (touchUp) {
        if (touch_consumed) {
            touch.px = 999; 
            touch.py = 999;
        } else {
            touch.px = cached_touch_x;
            touch.py = cached_touch_y;
        }
    }

    switch (current_state) {
        case STATE_LOADING_DETAILS:
            spinner_angle += 0.15f;
            if (connection_details_buf.done && connection_price_buf.done) {
                details = parse_ConnectionDetails(connection_details_buf.data);
                
                if (details.train_count > 0) {
                    char t_buf[128];
                    snprintf(t_buf, sizeof(t_buf), "Pociąg: %s", details.trains[0].train_name);
                    GFX_TextParse(&train_text, dyn_buf, t_buf);
                    GFX_TextOptimize(&train_text);
                }

                parse_tariffs_from_json(connection_price_buf.data);
                
                if (num_tariffs > 0) {
                    selected_tariff_idx = 0;
                    current_price = available_tariffs[0].price;

                    for (int i = 0; i < num_tariffs; i++) {
                        char price_buf[32];
                        snprintf(price_buf, sizeof(price_buf), "Cena: %.2f PLN", available_tariffs[i].price);
                        GFX_TextParse(&top_price_texts[i], dyn_buf, price_buf);
                        GFX_TextOptimize(&top_price_texts[i]);

                        GFX_TextParse(&tariff_texts[i], dyn_buf, available_tariffs[i].name);
                        GFX_TextOptimize(&tariff_texts[i]);

                        char short_price[16];
                        snprintf(short_price, sizeof(short_price), "%.2f PLN", available_tariffs[i].price);
                        GFX_TextParse(&tariff_price_texts[i], dyn_buf, short_price);
                        GFX_TextOptimize(&tariff_price_texts[i]);
                    }
                }
                current_state = STATE_DETAILS;
            }
            break;

        case STATE_DETAILS:
            for (int i = 0; i < num_tariffs; i++) {
                Update_button(&btn_tariffs[i], touch, touchDown, touchHeld, touchUp);
                if (btn_tariffs[i].clicked) {
                    selected_tariff_idx = i;
                    current_price = available_tariffs[i].price;
                }
            }

            Update_button(&btn_fast_res, touch, touchDown, touchHeld, touchUp);
            if (btn_fast_res.clicked) {
                if (details.train_count > 0 && num_tariffs > 0) {
                    playNavAudio(13, true);
                    rezerwuj_polaczenie.done = false;
                    
                    rezerwuj_Polaczenie(biletkup_connection_id, current_price, available_tariffs[selected_tariff_idx].ids, available_tariffs[selected_tariff_idx].id_count, &details);
                    current_state = STATE_RESERVING;
                }
            }
            
            Update_button(&btn_map_res, touch, touchDown, touchHeld, touchUp);
            if (btn_map_res.clicked) {
                if (details.train_count > 0 && num_tariffs > 0) {
                    playNavAudio(5, true);
                    flat_train_place_types_buf.done = false;
                    train_composition_buf.done = false;
                    seats_availability_buf.done = false;
                    
                    get_FlatTrainPlaceTypes(biletkup_connection_id, available_tariffs[selected_tariff_idx].ids, available_tariffs[selected_tariff_idx].id_count);
                    current_state = STATE_LOADING_COMPOSITION;
                }
            }
            break;

        case STATE_LOADING_COMPOSITION:
            spinner_angle += 0.15f;
            if (flat_train_place_types_buf.done) {
                conn_places = parse_FlatTrainPlaceTypes(flat_train_place_types_buf.data);
                
                if (conn_places.train_count > 0 && conn_places.trains[0].class_count > 0) {
                    class_option_count = conn_places.trains[0].class_count;
                    selected_class_idx = 0;
                    prepare_class_option_texts();
                    current_state = STATE_CLASS_SELECT;
                } else {
                    
                    current_state = STATE_DETAILS;
                }
            }
            break;

        case STATE_CLASS_SELECT:
            for (int i = 0; i < class_option_count; i++) {
                Update_button(&btn_classes[i], touch, touchDown, touchHeld, touchUp);
                if (btn_classes[i].clicked) {
                    selected_class_idx = i;
                    selected_carriage_idx = -1;
                    if (train_comp.carriages) {
                        free(train_comp.carriages);
                        train_comp.carriages = NULL;
                        train_comp.count = 0;
                    }
                    train_composition_buf.done = false;
                    seats_availability_buf.done = false;
                    
                    int train_nr_int = atoi(conn_places.trains[0].train_nr);
                    int place_type_id = conn_places.trains[0].classes[i].id;

                    get_TrainComposition(biletkup_connection_id, train_nr_int, place_type_id);
                    get_SeatsAvailability(biletkup_connection_id, train_nr_int, place_type_id);
                    current_state = STATE_LOADING_LAYOUT;
                    break;
                }
            }
            break;

        case STATE_LOADING_LAYOUT:
            spinner_angle += 0.15f;
            if (train_composition_buf.done && seats_availability_buf.done) { 
                if (train_comp.carriages) {
                    free(train_comp.carriages);
                    train_comp.carriages = NULL;
                    train_comp.count = 0;
                }
                train_comp = parse_TrainComposition(train_composition_buf.data);
                
                for(int i = 0; i < loaded_carriage_count; i++) {
                    koleo_api_free_layout(&loaded_layouts[i]);
                }
                loaded_carriage_count = 0;

                float current_offset_x = 0.0f;
                train_min_x = 9999;
                train_max_x = -9999;
                train_max_y = 0;
                
                for (int i = 0; i < (int)train_comp.count; i++) {
                    if (!train_comp.carriages[i].bookable) continue;
                    if (loaded_carriage_count >= MAX_LOADED_CARRIAGES) break;

                    KoleoTrainLayout* layout = &loaded_layouts[loaded_carriage_count];
                    if (load_and_parse_carriage(train_comp.carriages[i].type_id, layout)) {
                        if (seats_availability_buf.done) {
                            koleo_api_apply_availability(seats_availability_buf.data, layout, train_comp.carriages[i].number);
                        }
                        
                        if (layout->seat_count > 0) {
                            carriage_offsets[loaded_carriage_count] = current_offset_x;
                            strncpy(loaded_carriage_nrs[loaded_carriage_count], train_comp.carriages[i].number, 15);
                            loaded_carriage_nrs[loaded_carriage_count][15] = '\0';

                            char wagon_buf[32];
                            snprintf(wagon_buf, sizeof(wagon_buf), "Wagon %s", train_comp.carriages[i].number);
                            GFX_TextParse(&carriage_labels[loaded_carriage_count], dyn_buf, wagon_buf);
                            GFX_TextOptimize(&carriage_labels[loaded_carriage_count]);

                            float local_max_x = 0;
                            for (int j = 0; j < layout->seat_count; j++) {
                                float global_x = layout->seats[j].x + current_offset_x;
                                if (global_x < train_min_x) train_min_x = global_x;
                                if (global_x > train_max_x) train_max_x = global_x;
                                if (layout->seats[j].y > train_max_y) train_max_y = layout->seats[j].y;
                                if (layout->seats[j].x > local_max_x) local_max_x = layout->seats[j].x;
                            }
                            
                            current_offset_x += local_max_x + 120.0f; 
                            loaded_carriage_count++;
                        } else {
                            koleo_api_free_layout(layout);
                        }
                    }
                }

                if (loaded_carriage_count > 0) {
                    train_min_x -= 20;
                    train_max_x += 36;
                    train_max_y += 40;
                    scroll_x = 0;
                    
                    selected_seat_count = 0;

                    current_state = STATE_SEAT_SELECT;
                } else {
                    current_state = STATE_DETAILS;
                }
            }
            break;

        case STATE_SEAT_SELECT:
            Update_button(&btn_confirm_seat, touch, touchDown, touchHeld, touchUp);

            if (!(kHeld & KEY_TOUCH)) {
                if (fabs(scroll_velocity) > 0.1f) {
                    scroll_x += scroll_velocity;
                    scroll_velocity *= 0.90f; 

                    float max_scroll = (train_max_x - train_min_x) - 320;
                    if (max_scroll < 0) max_scroll = 0;
                    
                    if (scroll_x > 0) {
                        scroll_x = 0;
                        scroll_velocity = 0; 
                    }
                    if (scroll_x < -max_scroll) {
                        scroll_x = -max_scroll;
                        scroll_velocity = 0; 
                    }
                } else {
                    scroll_velocity = 0.0f; 
                }
            }

            if (kDown & KEY_TOUCH) {
                touching = true;
                touch_consumed = false;
                is_scrolling = false;
                last_touch_x = touch.px;
                last_touch_y = touch.py; 
                scroll_velocity = 0.0f; 
            }

            if (kHeld & KEY_TOUCH) {
                if (touching && !touch_consumed) {
                    float dx = touch.px - last_touch_x;
                    if (fabs(dx) > 4.0f || is_scrolling) { 
                        is_scrolling = true;
                        scroll_x += dx;
                        scroll_velocity = dx; 
                        
                        float max_scroll = (train_max_x - train_min_x) - 320;
                        if (max_scroll < 0) max_scroll = 0;
                        if (scroll_x > 0) scroll_x = 0;
                        if (scroll_x < -max_scroll) scroll_x = -max_scroll;
                        
                        last_touch_x = touch.px;
                    }
                }
            }

            if (kUp & KEY_TOUCH) {
                if (!is_scrolling && touching && !touch_consumed) {
                    
                    if (btn_confirm_seat.clicked && selected_seat_count == get_required_seats()) {
                        rezerwuj_polaczenie.done = false;
                        
                        rezerwuj_Polaczenie_Complex(
                            biletkup_connection_id,
                            current_price,
                            available_tariffs[selected_tariff_idx].ids,
                            available_tariffs[selected_tariff_idx].id_count,
                            conn_places.trains[0].train_nr,
                            conn_places.trains[0].classes[selected_class_idx].id,
                            selected_carriage_nrs_ptrs[0], 
                            selected_seats,
                            selected_compartment_types, 
                            selected_seat_count,
                            &details
                        );
                        current_state = STATE_RESERVING;
                        break;
                    }

                    if (last_touch_y < 200) {
                        bool toggled = false;
                        for (int c = 0; c < loaded_carriage_count; c++) {
                            KoleoTrainLayout* layout = &loaded_layouts[c];
                            for (int i = 0; i < layout->seat_count; i++) {
                                KoleoSeat *s = &layout->seats[i];
                                if (s->is_reserved || s->nr <= 0) continue;

                                float sx = s->x + carriage_offsets[c] + scroll_x;
                                float sy = s->y;

                                if (last_touch_x >= sx - 8 && last_touch_x <= sx + 24 &&
                                    last_touch_y >= sy - 8 && last_touch_y <= sy + 24)
                                {
                                    bool already = false;
                                    for (int k = 0; k < selected_seat_count; k++) {
                                        if (selected_seats[k] == s->nr && strcmp(selected_carriage_nrs_ptrs[k], loaded_carriage_nrs[c]) == 0) {
                                            for (int m = k; m < selected_seat_count - 1; m++) {
                                                selected_seats[m] = selected_seats[m + 1];
                                                selected_carriage_nrs_ptrs[m] = selected_carriage_nrs_ptrs[m + 1];
                                                selected_compartment_types[m] = selected_compartment_types[m + 1];
                                            }
                                            selected_seat_count--;
                                            already = true;
                                            break;
                                        }
                                    }

                                    if (!already && selected_seat_count < get_required_seats()) {
                                        if (selected_seat_count > 0 && strcmp(selected_carriage_nrs_ptrs[0], loaded_carriage_nrs[c]) != 0) {
                                            selected_seat_count = 0; 
                                        }
                                        selected_seats[selected_seat_count] = s->nr;
                                        selected_carriage_nrs_ptrs[selected_seat_count] = loaded_carriage_nrs[c];
                                        selected_compartment_types[selected_seat_count] = s->compartment_type_id; 
                                        selected_seat_count++;
                                    }
                                    toggled = true;
                                    break;
                                }
                            }
                            if (toggled) break;
                        }
                    }
                }
                touching = false; 
            }
            break;

        case STATE_RESERVING:
            spinner_angle += 0.15f;
            if (rezerwuj_polaczenie.done) {
                current_payment_id = parse_PaymentId(rezerwuj_polaczenie.data);
                current_state = (current_payment_id > 0) ? STATE_PAYMENT_SELECT : STATE_DETAILS;
            }
            break;

        case STATE_PAYMENT_SELECT:
            Update_button(&btn_blik, touch, touchDown, touchHeld, touchUp);
            if (btn_blik.clicked) {
                playNavAudio(1,true);
                SwkbdState swkbd;
                char buf[7] = "";
                swkbdInit(&swkbd, SWKBD_TYPE_NUMPAD, 1, 6);
                swkbdSetValidation(&swkbd, SWKBD_ANYTHING, 0, 0);
                swkbdSetHintText(&swkbd, "Wpisz 6-cyfrowy kod BLIK");
                
                if (swkbdInputText(&swkbd, buf, sizeof(buf)) && strlen(buf) == 6) {
                    strncpy(current_blik_code, buf, 7); 
                    platnosc_blik.done = false;
                    platnosc_BLIK((int)current_payment_id, current_blik_code);
                    current_state = STATE_PAYING;
                }
            } 
            
            Update_button(&btn_konto, touch, touchDown, touchHeld, touchUp);
            if (btn_konto.clicked && dane_uzytkownika.stan_konta >= current_price) {
                playNavAudio(11,true);
                zaplać_stanem_konta.done = false;
                zaplac_stanemKonta(current_payment_id);
                current_state = STATE_PAYING;
            }
            break;

        case STATE_PAYING:
            spinner_angle += 0.15f;
            if (platnosc_blik.done) {
                
                nwm_blik.done = false;
                get_Order_ID_from_BLIK((int)current_payment_id, current_blik_code);
                poll_timer = 60; 
                current_state = STATE_AWAITING_BLIK_ORDER;
            } else if (zaplać_stanem_konta.done) {
                
                current_state = STATE_SUCCESS;
            }
            break;

        case STATE_AWAITING_BLIK_ORDER:
            spinner_angle += 0.15f;
            if (nwm_blik.done) {
                long long fetched_order_id = -1;
                cJSON *root = cJSON_Parse(nwm_blik.data);
                
                if (root) {
                    cJSON *order_ids = cJSON_GetObjectItem(root, "order_ids");
                    if (cJSON_IsArray(order_ids) && cJSON_GetArraySize(order_ids) > 0) {
                        cJSON *first_id = cJSON_GetArrayItem(order_ids, 0);
                        if (cJSON_IsNumber(first_id)) {
                            fetched_order_id = (long long)first_id->valuedouble;
                        }
                    }
                    cJSON_Delete(root);
                }

                if (fetched_order_id > 0) {
                    
                    current_order_id = fetched_order_id;
                    pobierz_bilet.done = false;
                    pobierz_Bilet(current_order_id);
                    poll_timer = 60;
                    current_state = STATE_AWAITING_ORDER_READY;
                } else {
                    
                    if (--poll_timer <= 0) {
                        nwm_blik.done = false;
                        get_Order_ID_from_BLIK((int)current_payment_id, current_blik_code);
                        poll_timer = 60;
                    }
                }
            }
            break;

        case STATE_AWAITING_ORDER_READY:
            spinner_angle += 0.15f;
            if (pobierz_bilet.done) {
                bool is_error = false;
                cJSON *root = cJSON_Parse(pobierz_bilet.data);
                
                if (root) {
                    cJSON *status = cJSON_GetObjectItem(root, "status");
                    if (cJSON_IsString(status) && strcmp(status->valuestring, "error") == 0) {
                        is_error = true;
                    }
                    cJSON_Delete(root);
                }

                if (is_error || !root) {
                    
                    if (--poll_timer <= 0) {
                        pobierz_bilet.done = false;
                        pobierz_Bilet(current_order_id);
                        poll_timer = 60;
                    }
                } else {
                    
                    current_state = STATE_SUCCESS;
                }
            }
            break;
            
        case STATE_SUCCESS:
            break;
    }
}

void sceneBiletkupRender(void) {
    for (int side = 0; side < 2; side++) {
        GFX_BeginSceneTop(side, true);
        GFX_DrawRectSolid(0, 0, 0.1f, 400, 240, GFX_COLOR_RGBA(48,74,130,255));
        narysujpociagi(40);
        
        GFX_DrawRectSolid(0, 0, 0.8f, 400, 50, GFX_COLOR_RGBA(255,255,255,190));
        GFX_DrawText(&title_text, 200, 15, 0.9f, 0.7f, 0.7f, GFX_ALIGN_CENTER, GFX_COLOR_RGBA(48,74,130,255));

        if (current_state == STATE_CLASS_SELECT) {
            GFX_DrawRectSolid(40, 80, 0.6f, 320, 80, GFX_COLOR_RGBA(0, 0, 0, 160));
            GFX_DrawText(&class_title_text, 200, 110, 0.9f, 0.6f, 0.6f, GFX_ALIGN_CENTER, GFX_COLOR_RGBA(255, 255, 255, 255));
        } else if (current_state == STATE_SEAT_SELECT) {
            GFX_DrawRectSolid(40, 80, 0.6f, 320, 80, GFX_COLOR_RGBA(0, 0, 0, 160));
            GFX_DrawText(&seat_hint_text, 200, 110, 0.9f, 0.6f, 0.6f, GFX_ALIGN_CENTER, GFX_COLOR_RGBA(255, 255, 255, 255));
        } else if (current_state >= STATE_DETAILS) {
            GFX_DrawRectSolid(40, 80, 0.6f, 320, 80, GFX_COLOR_RGBA(0, 0, 0, 160));
            GFX_DrawText(&train_text, 200, 95, 0.9f, 0.65f, 0.65f, GFX_ALIGN_CENTER, GFX_COLOR_RGBA(255, 255, 255, 255));
            if (num_tariffs > 0) {
                GFX_DrawText(&top_price_texts[selected_tariff_idx], 200, 130, 0.9f, 0.65f, 0.65f, GFX_ALIGN_CENTER, GFX_COLOR_RGBA(120, 255, 120, 255));
            }
        }
        GFX_DrawShadowedText(&hint_text, 200, 210, 0.5f, 0.6f, 0.6f, GFX_ALIGN_CENTER, GFX_COLOR_RGBA(255,255,255,255), GFX_COLOR_RGBA(0,0,0,255));
    }

    GFX_BeginSceneBottom();
    GFX_DrawRectSolid(0, 0, 0.1f, 320, 240, GFX_COLOR_RGBA(48,74,130,255));
    narysujpociagi(40);
    GFX_DrawRectSolid(0, 0, 0.6f, 320, 240, GFX_COLOR_RGBA(0,0,0,120));

    if (current_state == STATE_LOADING_DETAILS || current_state == STATE_LOADING_LAYOUT || 
        current_state == STATE_RESERVING || current_state == STATE_PAYING ||
        current_state == STATE_AWAITING_BLIK_ORDER || current_state == STATE_AWAITING_ORDER_READY) {
        
        float cx = 160, cy = 120;
        for (int i = 0; i < 8; i++) {
            float a = spinner_angle - i*(M_PI/4);
            u8 alpha = 255 - i*30;
            GFX_DrawLine(cx + cos(a)*10, cy + sin(a)*10, GFX_COLOR_RGBA(255,255,255,alpha),
                         cx + cos(a)*18, cy + sin(a)*18, GFX_COLOR_RGBA(255,255,255,alpha), 3.0f, 0.8f);
        }
    } 
    else if (current_state == STATE_DETAILS) {
        for (int i = 0; i < num_tariffs; i++) {
            float y_pos = 20 + (i * 50);
            if (i == selected_tariff_idx) {
                btn_tariffs[i].color = GFX_COLOR_RGBA(80, 180, 80, 255);
                btn_tariffs[i].color_pressed = GFX_COLOR_RGBA(100, 200, 100, 255);
            } else {
                btn_tariffs[i].color = GFX_COLOR_RGBA(100, 100, 100, 255);
                btn_tariffs[i].color_pressed = GFX_COLOR_RGBA(120, 120, 120, 255);
            }
            Draw_button(&btn_tariffs[i]);
            GFX_DrawText(&tariff_texts[i], 160, y_pos + 8, 0.9f, 0.5f, 0.5f, GFX_ALIGN_CENTER, GFX_COLOR_RGBA(255, 255, 255, 255));
            GFX_DrawText(&tariff_price_texts[i], 160, y_pos + 25, 0.9f, 0.5f, 0.5f, GFX_ALIGN_CENTER, GFX_COLOR_RGBA(200, 255, 200, 255));
        }
        
        Draw_button(&btn_fast_res);
        GFX_DrawText(&btn_fast_rez_text, 85, 192, 0.8f, 0.5f, 0.5f, GFX_ALIGN_CENTER, GFX_COLOR_RGBA(255, 255, 255, 255));

        Draw_button(&btn_map_res);
        GFX_DrawText(&btn_rez_text, 235, 195, 0.8f, 0.34f, 0.34f, GFX_ALIGN_CENTER, GFX_COLOR_RGBA(255, 255, 255, 255));
    }
    else if (current_state == STATE_CLASS_SELECT) {
        GFX_DrawText(&class_title_text, 160, 10, 0.9f, 0.6f, 0.6f, GFX_ALIGN_CENTER, GFX_COLOR_RGBA(255, 255, 255, 255));
        for (int i = 0; i < class_option_count; i++) {
            float y_pos = 40 + (i * 45);
            if (i == selected_class_idx) {
                btn_classes[i].color = GFX_COLOR_RGBA(60, 180, 60, 255);
                btn_classes[i].color_pressed = GFX_COLOR_RGBA(80, 200, 80, 255);
            } else {
                btn_classes[i].color = GFX_COLOR_RGBA(80, 120, 200, 255);
                btn_classes[i].color_pressed = GFX_COLOR_RGBA(100, 140, 220, 255);
            }
            Draw_button(&btn_classes[i]);
            GFX_DrawText(&class_option_texts[i], 160, y_pos + 10, 0.9f, 0.5f, 0.5f, GFX_ALIGN_CENTER, GFX_COLOR_RGBA(255, 255, 255, 255));
        }
    }
    else if (current_state == STATE_SEAT_SELECT) {
        GFX_DrawRectSolid(train_min_x + scroll_x, -20, 0.75f, train_max_x - train_min_x, train_max_y + 40, GFX_COLOR_RGBA(210, 210, 210, 60));

        for (int c = 0; c < loaded_carriage_count; c++) {
            KoleoTrainLayout* layout = &loaded_layouts[c];
            GFX_DrawText(&carriage_labels[c], carriage_offsets[c] + scroll_x + 50, 10, 0.8f, 0.6f, 0.6f, GFX_ALIGN_LEFT, GFX_COLOR_RGBA(255, 255, 255, 255));

            for (int i = 0; i < layout->seat_count; i++) {
                KoleoSeat *s = &layout->seats[i];
                float angle = (s->direction == true) ? 3.14159265f : 0.0f;
                float sx = s->x + carriage_offsets[c] + scroll_x;
                
                GFX_IMAGE* img_puste = siedzenie_puste;
                GFX_IMAGE* img_zajete = siedzenie_zajete;
                GFX_IMAGE* img_wybrane = siedzenie;
                
                if (s->compartment_type_id == 7) { 
                    img_puste = siedzenie_wozek_puste; 
                    img_zajete = siedzenie_wozek_zajete;
                    img_wybrane = siedzenie_wozek;
                } else if (s->compartment_type_id == 17) {
                    img_puste = siedzenie_caretaker_puste;
                    img_zajete = siedzenie_caretaker_zajete;
                    img_wybrane = siedzenie_caretaker;
                }
                
                GFX_DrawImageAtRotated(img_puste, sx, s->y, 0.9f, angle, NULL, 1.0f, 1.0f);

                bool is_selected = false;
                for (int k = 0; k < selected_seat_count; k++) {
                    if (s->nr == selected_seats[k] && strcmp(selected_carriage_nrs_ptrs[k], loaded_carriage_nrs[c]) == 0) { 
                        is_selected = true; break; 
                    }
                }
                
                if (is_selected && s->nr > 0) {
                    GFX_DrawImageAtRotated(img_wybrane, sx, s->y, 0.9f, angle, NULL, 1.0f, 1.0f);
                } else if (s->is_reserved || s->nr <= 0) {
                    GFX_DrawImageAtRotated(img_zajete, sx, s->y, 0.9f, angle, NULL, 1.0f, 1.0f);
                } else if (s->seat_type_id == 1) {
                    GFX_DrawImageAtRotated(img_puste, sx, s->y, 0.9f, angle, NULL, 1.0f, 1.0f); 
                }
            }
        }

        if (selected_seat_count == get_required_seats()) {
            btn_confirm_seat.color = GFX_COLOR_RGBA(80, 180, 80, 255);
            btn_confirm_seat.color_pressed = GFX_COLOR_RGBA(100, 200, 100, 255);
            Draw_button(&btn_confirm_seat);
            GFX_DrawText(&btn_confirm_seat_text, 160, 208, 0.95f, 0.5f, 0.5f, GFX_ALIGN_CENTER, GFX_COLOR_RGBA(255, 255, 255, 255));
        } else {
            btn_confirm_seat.color = GFX_COLOR_RGBA(100, 100, 100, 255);
            btn_confirm_seat.color_pressed = GFX_COLOR_RGBA(120, 120, 120, 255);
            Draw_button(&btn_confirm_seat);
            GFX_DrawText(&btn_confirm_seat_text, 160, 208, 0.95f, 0.5f, 0.5f, GFX_ALIGN_CENTER, GFX_COLOR_RGBA(180, 180, 180, 255));
        }
    }
    else if (current_state == STATE_PAYMENT_SELECT) {
        Draw_button(&btn_blik);
        GFX_DrawText(&btn_blik_text, 160, 60, 0.9f, 0.7f, 0.7f, GFX_ALIGN_CENTER, GFX_COLOR_RGBA(255, 255, 255, 255));

        Draw_button(&btn_konto);
        GFX_DrawText(&btn_konto_text, 160, 140, 0.9f, 0.7f, 0.7f, GFX_ALIGN_CENTER, GFX_COLOR_RGBA(255, 255, 255, 255));
    } 
    else if (current_state == STATE_SUCCESS) {
        GFX_DrawRectSolid(40, 90, 0.8f, 240, 60, GFX_COLOR_RGBA(80, 180, 80, 255));
        GFX_DrawText(&status_text, 160, 110, 0.9f, 0.7f, 0.7f, GFX_ALIGN_CENTER, GFX_COLOR_RGBA(255, 255, 255, 255));
    }
}

void sceneBiletkupExit(void) {
    if (current_state >= STATE_DETAILS) free_ConnectionDetails(&details);
    koleo_api_free_layout(&current_layout);
    if (train_comp.carriages) {
        free(train_comp.carriages);
        train_comp.carriages = NULL;
        train_comp.count = 0;
    }
    GFX_TextBufClear(static_buf); GFX_TextBufDelete(static_buf);
    GFX_TextBufClear(dyn_buf); GFX_TextBufDelete(dyn_buf);
    GFX_TextBufClear(seat_buf); GFX_TextBufDelete(seat_buf);
    stopAudio(4);
}