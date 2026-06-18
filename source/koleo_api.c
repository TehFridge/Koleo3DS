#include <3ds.h>
#include <citro2d.h>
#include "koleo_api.h"
#include "scene_manager.h"
#include "main.h"
#include "audio_shit.h"
#include "sprites.h"
#include <math.h>
#include <stdio.h>
#include <jansson.h>
#include <unistd.h>
#include <stdlib.h>
#include <curl/curl.h>
#include "request.h"
#include "config.h"
#include "cJSON.h"
#include "utils.h"
#include <ctype.h>

ResponseBuffer testreq = {NULL, 0, 0, false};

char* refreshToken = NULL;
char* authToken = NULL;

void testrequest(){
    queue_request("https://inpost.pl/404", NULL, NULL, &testreq, false, GET);
}

char* dawajAuth() {
    size_t auth_len = strlen("Authorization: Bearer ") + strlen(authToken) + 1;

    char *authheader = malloc(auth_len);
    if (!authheader) {
        log_to_file("[getPaczkas] ERROR: malloc failed");
        return NULL;
    }

    snprintf(authheader, auth_len, "Authorization: Bearer %s", authToken);
    return authheader;
}

struct curl_slist* dajHeadery(bool auth) {
    struct curl_slist *headers = NULL;

    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "X-Koleo-Version: 1");
    headers = curl_slist_append(headers, "X-Koleo-Client: Android-101009");
    headers = curl_slist_append(headers, "Accept-EOL-Response-Version: 2");
    headers = curl_slist_append(headers, "User-Agent: okhttp/5.3.2");

    if (auth && authToken) {
        char authheader[512];
        snprintf(authheader, sizeof(authheader), "Authorization: Bearer %s", authToken);
        headers = curl_slist_append(headers, authheader);
    }

    return headers;
}

char* sklejURL(const char* endpoint){
    char* url = malloc(128);
    if (!url) return NULL;

    snprintf(url, 128, "%s%s", KOLEO_BASE_URL, endpoint);
    return url;
}

void format_time(const char *iso, char *out, size_t out_size) {
    if (!iso || strlen(iso) < 16) {
        snprintf(out, out_size, "--:--");
        return;
    }
    snprintf(out, out_size, "%.2s:%.2s", iso + 11, iso + 14);
}

/* TO CO KOLEO PRELOADUJE NA STARCIE */

ResponseBuffer all_stacjas = {NULL, 0, 0, false}; 
ResponseBuffer typy_miejsc = {NULL, 0, 0, false};
ResponseBuffer znizki = {NULL, 0, 0, false};
ResponseBuffer typy_wagon = {NULL, 0, 0, false};
ResponseBuffer atrybut_definicje = {NULL, 0, 0, false};
ResponseBuffer przewoznicy = {NULL, 0, 0, false};
ResponseBuffer marki = {NULL, 0, 0, false};

void getAll_Stacje(){
    queue_request(sklejURL("v2/main/stations"), "", dajHeadery(false), &all_stacjas, false, GET);
}
void getAll_TypyMiejsc(){
    queue_request(sklejURL("v2/main/seat_types"), "", dajHeadery(false), &typy_miejsc, false, GET);
}
void getAll_Znizki(){
    queue_request(sklejURL("v2/main/discount_cards"), "", dajHeadery(false), &znizki, false, GET);
}
void getAll_WagonTypy(){
    queue_request(sklejURL("v2/main/carriage_types"), "", dajHeadery(false), &typy_wagon, false, GET);
}
void getAll_AtrybutDefinicje(){
    queue_request(sklejURL("v2/main/attribute_definitions"), "", dajHeadery(false), &atrybut_definicje, false, GET);
}
void getAll_Przewoznicy(){
    queue_request(sklejURL("v2/main/carriers"), "", dajHeadery(false), &przewoznicy, false, GET);
}
void getAll_Marki(){
    queue_request(sklejURL("v2/main/brands"), "", dajHeadery(false), &marki, false, GET);
}

/* DANE Z KONTA ITP. */

ResponseBuffer konto_login = {NULL, 0, 0, false};
ResponseBuffer user_dane = {NULL, 0, 0, false};
ResponseBuffer pasazerowie = {NULL, 0, 0, false};
ResponseBuffer deselect_pasazer = {NULL, 0, 0, false};
ResponseBuffer select_pasazer = {NULL, 0, 0, false};
ResponseBuffer pobierz_bilety = {NULL, 0, 0, false};
ResponseBuffer pobierz_bilet = {NULL, 0, 0, false};
ResponseBuffer refresh_token = {NULL, 0, 0, false};

void kontoLogin(char email[60], char haslo[60]){
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "username", email);
    cJSON_AddStringToObject(root, "password", haslo);
    cJSON_AddStringToObject(root, "grant_type", "password");
    cJSON_AddStringToObject(root, "client_id", KOLEO_OAUTH_KEY);
    char *request = cJSON_PrintUnformatted(root); 
    cJSON_Delete(root);
    queue_request(sklejURL("v2/main/oauth/token"), request, dajHeadery(false), &konto_login, false, POST);
}
void refresh_the_Token(){
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "grant_type", "refresh_token");
    cJSON_AddStringToObject(root, "refresh_token", refreshToken);
    cJSON_AddStringToObject(root, "client_id", KOLEO_OAUTH_KEY);
    char *request = cJSON_PrintUnformatted(root); 
    cJSON_Delete(root);
    queue_request(sklejURL("v2/main/oauth/token"), request, dajHeadery(false), &refresh_token, false, POST);
}
void get_Pasazerowie(){ 
    queue_request(sklejURL("v2/main/passengers"), "", dajHeadery(true), &pasazerowie, false, GET);
}
void get_UserDane(){
    queue_request(sklejURL("v2/main/user"), "", dajHeadery(true), &user_dane, false, GET);
}
void deselect_Pasazer(int pasazer_id){
    char endpoint[128];
    snprintf(endpoint, sizeof(endpoint), "v2/main/passengers/%d/deselect", pasazer_id);
    char* full_url = sklejURL(endpoint);
    queue_request(full_url, "", dajHeadery(true), &deselect_pasazer, false, PATCH);
    free(full_url);
}
void select_Pasazer(int pasazer_id){
    char endpoint[128];
    snprintf(endpoint, sizeof(endpoint), "v2/main/passengers/%d/select", pasazer_id);
    char* full_url = sklejURL(endpoint);
    queue_request(full_url, "", dajHeadery(true), &select_pasazer, false, PATCH);
    free(full_url);
}

void pobierz_Bilety(){
    queue_request(sklejURL("v2/main/orders/active"), "", dajHeadery(true), &pobierz_bilety, false, GET);
}

void pobierz_Bilet(long long order_id){
    char endpoint[128];
    snprintf(endpoint, sizeof(endpoint), "v2/main/orders/%lld", order_id);
    char* full_url = sklejURL(endpoint);
    queue_request(full_url, "", dajHeadery(true), &pobierz_bilet, false, GET);
    free(full_url);
}
/* WYSZUKIWANIE POŁĄCZEŃ */

ResponseBuffer daj_cene_przejazdu = {NULL, 0, 0, false};
ResponseBuffer dawaj_stacje_for_me = {NULL, 0, 0, false};
ResponseBuffer dawaj_connection_id = {NULL, 0, 0, false};
ResponseBuffer connection_details_buf = {NULL, 0, 0, false};
ResponseBuffer connection_price_buf = {NULL, 0, 0, false};

void getStacje_for_Me(int startowa_stacja, int koncowa_stacja, const char* kiedy, bool bezposrednie)
{
    cJSON *root = cJSON_CreateObject();

    cJSON_AddNumberToObject(root, "start_id", startowa_stacja);
    cJSON_AddNumberToObject(root, "end_id", koncowa_stacja);
    cJSON_AddStringToObject(root, "departure_after", kiedy);
    cJSON_AddBoolToObject(root, "only_direct", bezposrednie);

    cJSON *brands = cJSON_AddArrayToObject(root, "allowed_brands");
    int allowed[] = {1,2,3,4,5,6,8,9,10,11,12,13,14,18,20,27,28,29,33,38,40,43,45,46,47,48,49,51,52,53,54,56,57,58,59,61,62,63,64,65,66};
    int count = sizeof(allowed) / sizeof(allowed[0]);

    for (int i = 0; i < count; i++) {
        cJSON_AddItemToArray(brands, cJSON_CreateNumber(allowed[i]));
    }

    char *request = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    queue_request(sklejURL("v2/main/eol_connections/search"), request, dajHeadery(true), &dawaj_stacje_for_me, false, POST);
}

void getPrzejazd_Cena(const char* uuid){
    char endpoint[128];
    snprintf(endpoint, sizeof(endpoint), "v2/main/eol_connections/%s/price", uuid);
    char* full_url = sklejURL(endpoint);
    queue_request(full_url, "", dajHeadery(true), &daj_cene_przejazdu, false, GET);
    free(full_url);
}

void get_ConnectionId(const char* uuid){
    char endpoint[128];
    snprintf(endpoint, sizeof(endpoint), "v2/main/eol_connections/%s/connection_id", uuid);
    char* full_url = sklejURL(endpoint);
    queue_request(full_url, "", dajHeadery(true), &dawaj_connection_id, false, PUT);
    free(full_url);
}

void get_ConnectionDetails(long long connection_id) {
    char endpoint[128];
    snprintf(endpoint, sizeof(endpoint), "v2/main/connections/%lld", connection_id);
    char* full_url = sklejURL(endpoint);
    queue_request(full_url, "", dajHeadery(true), &connection_details_buf, false, GET);
    free(full_url);
}

void get_ConnectionPrice(long long connection_id) {
    char endpoint[128];
    snprintf(endpoint, sizeof(endpoint), "v2/main/connections/%lld/price?context=traveloptions", connection_id);
    char* full_url = sklejURL(endpoint);
    queue_request(full_url, "", dajHeadery(true), &connection_price_buf, false, GET);
    free(full_url);
}

/* PŁATNOŚCI (BLIK I KONTO) */

ResponseBuffer doladuj_konto = {NULL, 0, 0, false};
ResponseBuffer nwm_blik = {NULL, 0, 0, false};
ResponseBuffer platnosc_blik = {NULL, 0, 0, false};
ResponseBuffer platnosc_status = {NULL, 0, 0, false};
ResponseBuffer rezerwuj_polaczenie = {NULL, 0, 0, false};
ResponseBuffer zaplać_stanem_konta = {NULL, 0, 0, false};

void rezerwuj_Polaczenie(long long connection_id, float price, int* tariff_ids, int tariff_count, PolaczenieSzczegoly* details) {
    cJSON *root = cJSON_CreateObject();
    
    cJSON *tariffs = cJSON_CreateIntArray(tariff_ids, tariff_count);
    cJSON_AddItemToObject(root, "tariff_ids", tariffs);

    if (details && details->train_count > 1) {
        cJSON *place_types = cJSON_CreateArray();
        for (size_t i = 0; i < details->train_count; i++) {
            cJSON *pt_item = cJSON_CreateObject();
            cJSON_AddStringToObject(pt_item, "train_nr", details->trains[i].train_nr);

            int type_id = (i == 0) ? 5 : -1;
            cJSON_AddNumberToObject(pt_item, "place_type_id", type_id);
            
            cJSON_AddItemToArray(place_types, pt_item);
        }
        cJSON_AddItemToObject(root, "place_types", place_types);
    }
    
    cJSON_AddItemToObject(root, "option_groups", cJSON_CreateArray());
    
    char formatted_price[32];
    snprintf(formatted_price, sizeof(formatted_price), "%.2f", price);
    cJSON_AddRawToObject(root, "displayed_price", formatted_price);

    char *json_body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    char endpoint[128];
    snprintf(endpoint, sizeof(endpoint), "v2/main/connections/%lld/reservation", connection_id);
    char* full_url = sklejURL(endpoint);

    queue_request(full_url, json_body, dajHeadery(true), &rezerwuj_polaczenie, false, POST);

    free(full_url);
    free(json_body); 
}

long long parse_PaymentId(const char* json_string) {
    long long payment_id = -1;
    cJSON *root = cJSON_Parse(json_string);
    if (root) {
        cJSON *pid = cJSON_GetObjectItem(root, "payment_id");
        if (cJSON_IsNumber(pid)) {
            payment_id = (long long)pid->valuedouble;
        }
        cJSON_Delete(root);
    }
    return payment_id;
}

void doladuj_Konto(float ilosc){
    if (ilosc >= 20.00f){
        cJSON *root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "amount", ilosc);
        char *request = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);
        queue_request(sklejURL("v2/main/wallet"), request, dajHeadery(true), &doladuj_konto, false, POST);
    }
}

void platnosc_BLIK(int id_platnosc, const char* blik_kod){
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "blik_code", blik_kod);
    cJSON_AddBoolToObject(root, "invoice_requested", false);
    char *request = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    char endpoint[128];
    snprintf(endpoint, sizeof(endpoint), "v2/main/payments/%d/blik", id_platnosc);
    char* full_url = sklejURL(endpoint);
    queue_request(full_url, request, dajHeadery(true), &platnosc_blik, false, POST);
    free(full_url);
}

void get_Order_ID_from_BLIK(int id_platnosc, const char* blik_kod){
    char endpoint[128];
    snprintf(endpoint, sizeof(endpoint), "v2/main/payments/%d/blik/%s", id_platnosc, blik_kod);
    char* full_url = sklejURL(endpoint);
    queue_request(full_url, "", dajHeadery(true), &nwm_blik, false, GET);
    free(full_url);
}

void getPlatnosc_Status(int id_platnosc){
    char endpoint[128];
    snprintf(endpoint, sizeof(endpoint), "v2/main/payments/%d", id_platnosc);
    char* full_url = sklejURL(endpoint);
    queue_request(full_url, "", dajHeadery(true), &platnosc_status, false, GET);
    free(full_url);
}

void zaplac_stanemKonta(long long payment_id){
    char endpoint[128];
    snprintf(endpoint, sizeof(endpoint), "v2/main/payments/%lld?invoice_requested=false", payment_id);
    char* full_url = sklejURL(endpoint);
    queue_request(full_url, "", dajHeadery(true), &zaplać_stanem_konta, false, PATCH);
    free(full_url);
}

/* PARSERY DO PASAŻERÓW */
Pasazer_List global_lista;

Pasazer create_pasazer(const char *imie, const char *nazwisko, int pasazer_id) {
    Pasazer p;
    strncpy(p.imie, imie ? imie : "", sizeof(p.imie) - 1);
    p.imie[sizeof(p.imie) - 1] = '\0';
    strncpy(p.nazwisko, nazwisko ? nazwisko : "", sizeof(p.nazwisko) - 1);
    p.nazwisko[sizeof(p.nazwisko) - 1] = '\0';
    p.pasazer_id = pasazer_id;
    return p;
}

Pasazer_List parse_Pasazerowie(const char *json_string) {
    Pasazer_List list = {0};
    cJSON *root = cJSON_Parse(json_string);
    if (!root) return list;

    cJSON *passengers = cJSON_GetObjectItem(root, "passengers");
    if (!cJSON_IsArray(passengers)) {
        cJSON_Delete(root);
        return list;
    }

    int count = cJSON_GetArraySize(passengers);
    list.ziomy = malloc(sizeof(Pasazer) * count);
    list.count = count;

    int i = 0;
    cJSON *passenger = NULL;
    cJSON_ArrayForEach(passenger, passengers) {
        cJSON *id = cJSON_GetObjectItem(passenger, "id");
        cJSON *first_name = cJSON_GetObjectItem(passenger, "first_name");
        cJSON *last_name = cJSON_GetObjectItem(passenger, "last_name");
        cJSON *selected = cJSON_GetObjectItem(passenger, "is_selected");

        const char *fname = (cJSON_IsString(first_name)) ? first_name->valuestring : "";
        const char *lname = (cJSON_IsString(last_name)) ? last_name->valuestring : "";
        int pid = (cJSON_IsNumber(id)) ? id->valueint : 0;
        bool sel = (cJSON_IsBool(selected)) ? cJSON_IsTrue(selected) : false;

        list.ziomy[i++] = create_pasazer(fname, lname, pid);
        list.ziomy[i-1].selected = sel; 
    }
    cJSON_Delete(root);
    return list;
}

/* AUTH PARSERY */
void parse_AuthResponse(const char* json){
    cJSON *root = cJSON_Parse(json);
    cJSON *access_token = cJSON_GetObjectItem(root, "access_token");
    cJSON *refresh_token = cJSON_GetObjectItem(root, "refresh_token");
    free(authToken);
    authToken = strdup(access_token->valuestring);
    free(refreshToken);
    refreshToken = strdup(refresh_token->valuestring);
    cJSON_Delete(root);

    cJSON *rootfile = cJSON_CreateObject();
    cJSON_AddStringToObject(rootfile, "access_token", authToken);
    cJSON_AddStringToObject(rootfile, "refresh_token", refreshToken);

    char *json_string = cJSON_PrintUnformatted(rootfile);
    if (json_string) {
        FILE *file = fopen("/3ds/Koleo3DS/auth.json", "w");
        if (file) {
            fprintf(file, "%s", json_string);
            fclose(file);
        }
        free(json_string);
    }
    cJSON_Delete(rootfile);
}

/* PARSERY DO DANYCH KONTA ITP. */
UserDane dane_uzytkownika;

void parse_UserDane(const char* json){
    cJSON *root = cJSON_Parse(json);
    cJSON *email = cJSON_GetObjectItem(root, "email");
    cJSON *name = cJSON_GetObjectItem(root, "name");
    cJSON *surname = cJSON_GetObjectItem(root, "surname");
    cJSON *birthday = cJSON_GetObjectItem(root, "birthday");
    cJSON *discount_id = cJSON_GetObjectItem(root, "discount_id");
    cJSON *koleo_wallet_balance = cJSON_GetObjectItem(root, "koleo_wallet_balance");

    strncpy(dane_uzytkownika.imie, name->valuestring, sizeof(dane_uzytkownika.imie) - 1);
    dane_uzytkownika.imie[sizeof(dane_uzytkownika.imie) - 1] = '\0';

    strncpy(dane_uzytkownika.nazwisko, surname->valuestring, sizeof(dane_uzytkownika.nazwisko) - 1);
    dane_uzytkownika.nazwisko[sizeof(dane_uzytkownika.nazwisko) - 1] = '\0';

    strncpy(dane_uzytkownika.email, email->valuestring, sizeof(dane_uzytkownika.email) - 1);
    dane_uzytkownika.email[sizeof(dane_uzytkownika.email) - 1] = '\0';

    strncpy(dane_uzytkownika.urodziny, birthday->valuestring, sizeof(dane_uzytkownika.urodziny) - 1);
    dane_uzytkownika.urodziny[sizeof(dane_uzytkownika.urodziny) - 1] = '\0';

    dane_uzytkownika.stan_konta = atof(koleo_wallet_balance->valuestring);
    dane_uzytkownika.id_znizki = discount_id->valueint;

    cJSON_Delete(root);
}

/* PARSERY STACJI I POŁĄCZEŃ */
static inline char normalize_polish_char(unsigned char c1, unsigned char c2, int *bytes_consumed) {
    *bytes_consumed = 1;

    if (c1 == 0xC5 || c1 == 0xC4 || c1 == 0xC3) {
        *bytes_consumed = 2;
        if (c1 == 0xC5) {
            if (c2 == 0x81 || c2 == 0x82) return 'l'; // Ł, ł
            if (c2 == 0x9A || c2 == 0x9B) return 's'; // Ś, ś
            if (c2 == 0xB9 || c2 == 0xBA) return 'z'; // Ź, ź
            if (c2 == 0xBB || c2 == 0xBC) return 'z'; // Ż, ż
            if (c2 == 0x83 || c2 == 0x84) return 'n'; // Ń, ń
        } else if (c1 == 0xC4) {
            if (c2 == 0x84 || c2 == 0x85) return 'a'; // Ą, ą
            if (c2 == 0x86 || c2 == 0x87) return 'c'; // Ć, ć
            if (c2 == 0x98 || c2 == 0x99) return 'e'; // Ę, ę
        } else if (c1 == 0xC3) {
            if (c2 == 0x93 || c2 == 0xB3) return 'o'; // Ó, ó
        }
        return '?'; // Unknown UTF-8 sequence
    }

    switch (c1) {
        case 0xA5: case 0xB9: case 0xA1: case 0xB1: return 'a'; // Ą, ą
        case 0xC6: case 0xE6: return 'c'; // Ć, ć
        case 0xCA: case 0xEA: case 0xC2: case 0xE2: return 'e'; // Ę, ę
        case 0xA3: case 0xB3: case 0xB2: return 'l'; // Ł, ł
        case 0xD1: case 0xF1: return 'n'; // Ń, ń
        case 0xD3: case 0xF3: return 'o'; // Ó, ó
        case 0x8C: case 0x9C: case 0xA6: case 0xB6: return 's'; // Ś, ś
        case 0x8F: case 0x9F: case 0xAC: case 0xBC: return 'z'; // Ź, ź
        case 0xAF: case 0xBF: case 0xAE: case 0xBE: return 'z'; // Ż, ż
        default: return tolower(c1); // Standard character
    }
}

bool match_station_query(const char *haystack, const char *query) {
    if (!haystack || !query) return false;
    if (*query == '\0') return true;

    size_t h_len = strlen(haystack);
    size_t q_len = strlen(query);

    char *flat_h = malloc(h_len + 1);
    char *flat_q = malloc(q_len + 1);
    if (!flat_h || !flat_q) {
        free(flat_h); free(flat_q);
        return false;
    }

    size_t i = 0, h_idx = 0;
    while (i < h_len) {
        int consumed = 1;
        flat_h[h_idx++] = normalize_polish_char((unsigned char)haystack[i], (unsigned char)haystack[i+1], &consumed);
        i += consumed;
    }
    flat_h[h_idx] = '\0';

    i = 0; size_t q_idx = 0;
    while (i < q_len) {
        int consumed = 1;
        flat_q[q_idx++] = normalize_polish_char((unsigned char)query[i], (unsigned char)query[i+1], &consumed);
        i += consumed;
    }
    flat_q[q_idx] = '\0';

    bool found = (strstr(flat_h, flat_q) != NULL);

    free(flat_h);
    free(flat_q);
    return found;
}

Stacja_List all_stacja_list;

Stacja create_stacja(const char *nazwa, const char *nazwa_slug, const char *miasto, const char *region, const char *kraj, int stacja_id) {
    Stacja p;
    strncpy(p.nazwa, nazwa ? nazwa : "", sizeof(p.nazwa) - 1);
    p.nazwa[sizeof(p.nazwa) - 1] = '\0';
    strncpy(p.nazwa_slug, nazwa_slug ? nazwa_slug : "", sizeof(p.nazwa_slug) - 1);
    p.nazwa_slug[sizeof(p.nazwa_slug) - 1] = '\0';
    strncpy(p.miasto, miasto ? miasto : "", sizeof(p.miasto) - 1);
    p.miasto[sizeof(p.miasto) - 1] = '\0';
    strncpy(p.region, region ? region : "", sizeof(p.region) - 1);
    p.region[sizeof(p.region) - 1] = '\0';
    strncpy(p.kraj, kraj ? kraj : "", sizeof(p.kraj) - 1);
    p.kraj[sizeof(p.kraj) - 1] = '\0';
    p.stacja_id = stacja_id;
    return p;
}

#define INITIAL_CAPACITY 64

Stacja_List search_stations(const char *json_string, const char *query) {
    Stacja_List list = {0};
    int capacity = INITIAL_CAPACITY;
    list.station = malloc(sizeof(Stacja) * capacity);
    if (!list.station || !json_string || !query || query[0] == '\0') return list;

    const char *ptr = json_string;

    while ((ptr = strstr(ptr, "{\"id\":")) != NULL) {
        const char *end_of_obj = strchr(ptr, '}');
        if (!end_of_obj) break;
        size_t obj_len = (end_of_obj - ptr) + 1;

        char *temp_chunk = malloc(obj_len + 1);
        if (temp_chunk) {
            memcpy(temp_chunk, ptr, obj_len);
            temp_chunk[obj_len] = '\0';

            cJSON *stacja = cJSON_Parse(temp_chunk);
            if (stacja) {
                cJSON *name = cJSON_GetObjectItem(stacja, "name");
                cJSON *name_slug = cJSON_GetObjectItem(stacja, "name_slug");
                cJSON *region = cJSON_GetObjectItem(stacja, "region");
                cJSON *city = cJSON_GetObjectItem(stacja, "city");
                cJSON *country = cJSON_GetObjectItem(stacja, "country");
                cJSON *id = cJSON_GetObjectItem(stacja, "id");

                const char *name_c = cJSON_IsString(name) ? name->valuestring : "";
                const char *name_slug_c = cJSON_IsString(name_slug) ? name_slug->valuestring : "";
                const char *region_c = cJSON_IsString(region) ? region->valuestring : "";
                const char *city_c = cJSON_IsString(city) ? city->valuestring : "";
                const char *country_c = cJSON_IsString(country) ? country->valuestring : "";
                int s_id = cJSON_IsNumber(id) ? id->valueint : 0;

                if (match_station_query(city_c, query) || 
                    match_station_query(name_c, query) || 
                    match_station_query(name_slug_c, query)) {
                    
                    if (list.count >= capacity) {
                        capacity *= 2;
                        Stacja *tmp = realloc(list.station, sizeof(Stacja) * capacity);
                        if (!tmp) { cJSON_Delete(stacja); free(temp_chunk); break; }
                        list.station = tmp;
                    }
                    list.station[list.count++] = create_stacja(name_c, name_slug_c, city_c, region_c, country_c, s_id);
                }
                cJSON_Delete(stacja);
            }
            free(temp_chunk);
        }
        ptr = end_of_obj;
    }
    return list;
}

int znajdz_stacja_id(const char *query) {
    if (!query || query[0] == '\0') return -1;
    FILE *f = fopen("/3ds/koleo3ds/all_stacjas.json", "rb");
    if (!f) return -1;

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    rewind(f);

    char *json = malloc(size + 1);
    if (!json) { fclose(f); return -1; }

    fread(json, 1, size, f);
    json[size] = '\0';
    fclose(f);

    Stacja_List list = search_stations(json, query);
    int found_id = -1;

    if (list.count > 0) {
        for (int i = 0; i < list.count; i++) {
            if (strcasecmp(list.station[i].nazwa, query) == 0) {
                found_id = list.station[i].stacja_id;
                break;
            }
        }
        if (found_id == -1) found_id = list.station[0].stacja_id;
    }
    free(list.station);
    free(json);
    return found_id;
}

void get_stacja_nazwa_by_id_fast(int id, char* out_name, size_t max_len, const char* data_ptr) {
    snprintf(out_name, max_len, "Stacja: %d", id);
    if (!data_ptr) return;

    char search_str[32];
    snprintf(search_str, sizeof(search_str), "\"id\":%d", id);
    char *ptr = strstr(data_ptr, search_str);
    if (!ptr) return;

    char *start = ptr;
    while(start > data_ptr && *start != '{') start--;

    char *end = strchr(ptr, '}');
    if (!end) return;

    size_t len = end - start + 1;
    char *chunk = malloc(len + 1);
    if(chunk) {
        memcpy(chunk, start, len);
        chunk[len] = '\0';
        cJSON *obj = cJSON_Parse(chunk);
        if(obj) {
            cJSON *name = cJSON_GetObjectItem(obj, "name");
            if(cJSON_IsString(name)) {
                strncpy(out_name, name->valuestring, max_len - 1);
                out_name[max_len - 1] = '\0';
            }
            cJSON_Delete(obj);
        }
        free(chunk);
    }
}

void get_stacja_nazwa_by_id(int id, char* out_name, size_t max_len) {
    char *file_buf = NULL;
    const char *data_ptr = all_stacjas.data;

    if (!data_ptr) {
        FILE *f = fopen("/3ds/koleo3ds/all_stacjas.json", "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            size_t size = ftell(f);
            rewind(f);
            file_buf = malloc(size + 1);
            if (file_buf) {
                fread(file_buf, 1, size, f);
                file_buf[size] = '\0';
                data_ptr = file_buf;
            }
            fclose(f);
        }
    }
    
    get_stacja_nazwa_by_id_fast(id, out_name, max_len, data_ptr);
    
    if (file_buf) free(file_buf);
}

Polaczenie_List global_polaczenia_list;

Polaczenie create_polaczenie(const char* uuid, const char* nazwa, float cena, const char* waluta, const char* czas_odjazdu, const char* czas_dotarcia, int dlugosc_trasy, bool bezposrednie, int przesiadki, int connection_id) {
    Polaczenie p;
    strncpy(p.uuid, uuid ? uuid : "", sizeof(p.uuid) - 1);
    p.uuid[sizeof(p.uuid) - 1] = '\0';
    strncpy(p.nazwa, nazwa ? nazwa : "", sizeof(p.nazwa) - 1);
    p.nazwa[sizeof(p.nazwa) - 1] = '\0';
    strncpy(p.waluta, waluta ? waluta : "", sizeof(p.waluta) - 1);
    p.waluta[sizeof(p.waluta) - 1] = '\0';
    strncpy(p.czas_odjazdu, czas_odjazdu ? czas_odjazdu : "", sizeof(p.czas_odjazdu) - 1);
    p.czas_odjazdu[sizeof(p.czas_odjazdu) - 1] = '\0';
    strncpy(p.czas_dotarcia, czas_dotarcia ? czas_dotarcia : "", sizeof(p.czas_dotarcia) - 1);
    p.czas_dotarcia[sizeof(p.czas_dotarcia) - 1] = '\0';
    p.cena = cena;
    p.dlugosc_trasy = dlugosc_trasy;
    p.bezposrednie = bezposrednie;
    p.przesiadki = przesiadki;
    p.connection_id = connection_id;
    return p;
}

Polaczenie_List parse_Polaczenia(const char* json_string) {
    Polaczenie_List list = {0};
    int capacity = INITIAL_CAPACITY;
    list.polaczenia = malloc(sizeof(Polaczenie) * capacity);
    if (!list.polaczenia) return list;

    const char *ptr = json_string;

    while ((ptr = strstr(ptr, "{\"uuid\":")) != NULL) {
        const char *end = ptr;
        int depth = 0;
        do {
            if (*end == '{') depth++;
            else if (*end == '}') depth--;
            end++;
        } while (depth > 0 && *end);

        if (depth != 0) break;
        size_t len = end - ptr;
        char *chunk = malloc(len + 1);
        if (!chunk) break;

        memcpy(chunk, ptr, len);
        chunk[len] = '\0';
        cJSON *connection = cJSON_Parse(chunk);
        free(chunk);

        if (connection) {
            cJSON *uuid = cJSON_GetObjectItem(connection, "uuid");
            cJSON *departure = cJSON_GetObjectItem(connection, "departure");
            cJSON *arrival = cJSON_GetObjectItem(connection, "arrival");
            cJSON *duration = cJSON_GetObjectItem(connection, "duration");
            cJSON *changes = cJSON_GetObjectItem(connection, "changes");

            const char *uuid_c = cJSON_IsString(uuid) ? uuid->valuestring : "";
            const char *dep_c = cJSON_IsString(departure) ? departure->valuestring : "";
            const char *arr_c = cJSON_IsString(arrival) ? arrival->valuestring : "";

            int duration_i = cJSON_IsNumber(duration) ? duration->valueint : 0;
            int changes_i = cJSON_IsNumber(changes) ? changes->valueint : 0;

            char dep_fmt[32] = "--:--";
            char arr_fmt[32] = "--:--";
            if (strlen(dep_c) >= 16) snprintf(dep_fmt, sizeof(dep_fmt), "%.2s:%.2s", dep_c + 11, dep_c + 14);
            if (strlen(arr_c) >= 16) snprintf(arr_fmt, sizeof(arr_fmt), "%.2s:%.2s", arr_c + 11, arr_c + 14);

            char nazwa_buf[128];
            snprintf(nazwa_buf, sizeof(nazwa_buf), "%s → %s", dep_fmt, arr_fmt);
            bool bezposrednie = (changes_i == 0);

            if (list.count >= capacity) {
                capacity *= 2;
                Polaczenie *tmp = realloc(list.polaczenia, sizeof(Polaczenie) * capacity);
                if (!tmp) { cJSON_Delete(connection); break; }
                list.polaczenia = tmp;
            }

            list.polaczenia[list.count++] = create_polaczenie(uuid_c, nazwa_buf, 0.0f, "", dep_fmt, arr_fmt, duration_i, bezposrednie, changes_i, 0);
            cJSON_Delete(connection);
        }
        ptr = end;
    }
    return list;
}

long long parse_ConnectionId(const char* json_string) {
    long long conn_id = 0;
    cJSON *root = cJSON_Parse(json_string);
    if (root) {
        cJSON *id = cJSON_GetObjectItem(root, "id");
        if (!id) id = cJSON_GetObjectItem(root, "connection_id"); 
        if (cJSON_IsNumber(id)) {
            conn_id = (long long)id->valuedouble; 
        }
        cJSON_Delete(root);
    }
    return conn_id;
}

float parse_ConnectionPrice(const char* json_string) {
    float price = 0.0f;
    log_to_file("Cena przed: 0 (init)");
    cJSON *root = cJSON_Parse(json_string);
    if (root) {
        cJSON *prices = cJSON_GetObjectItem(root, "prices");
        log_to_file("array size: %d", cJSON_GetArraySize(prices));
        if (cJSON_IsArray(prices) && cJSON_GetArraySize(prices) > 0) {
            cJSON *first = cJSON_GetArrayItem(prices, 0);
            cJSON *value = cJSON_GetObjectItem(first, "value");
            if (cJSON_IsString(value)) {
                price = atof(value->valuestring);
            }
        }
        cJSON_Delete(root);
    }
    log_to_file("Cena po parsowaniu: %.2f", price);
    return price;
}

PolaczenieSzczegoly parse_ConnectionDetails(const char* json_string) {
    PolaczenieSzczegoly details = {0};
    cJSON *root = cJSON_Parse(json_string);
    if (!root) return details;

    char *stations_json = NULL;
    if (!all_stacjas.data) {
        FILE *f = fopen("/3ds/koleo3ds/all_stacjas.json", "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            size_t size = ftell(f);
            rewind(f);
            stations_json = malloc(size + 1);
            if (stations_json) {
                fread(stations_json, 1, size, f);
                stations_json[size] = '\0';
            }
            fclose(f);
        }
    }
    const char *data_ptr = all_stacjas.data ? all_stacjas.data : stations_json;

    cJSON *trains = cJSON_GetObjectItem(root, "trains");
    if (cJSON_IsArray(trains)) {
        details.train_count = cJSON_GetArraySize(trains);
        details.trains = malloc(sizeof(PolaczenieTrain) * details.train_count);

        for (int i = 0; i < details.train_count; i++) {
            cJSON *train = cJSON_GetArrayItem(trains, i);
            PolaczenieTrain *t = &details.trains[i];
            memset(t, 0, sizeof(PolaczenieTrain));

            cJSON *tid = cJSON_GetObjectItem(train, "id");
            if (cJSON_IsNumber(tid)) {
                t->id = tid->valueint;
            }

            cJSON *tname = cJSON_GetObjectItem(train, "train_name");
            if (cJSON_IsString(tname)) {
                strncpy(t->train_name, tname->valuestring, sizeof(t->train_name) - 1);
            }

            cJSON *tnr = cJSON_GetObjectItem(train, "train_nr");
            if (!tnr) tnr = cJSON_GetObjectItem(train, "number");
            if (!tnr) tnr = cJSON_GetObjectItem(train, "name");

            if (cJSON_IsString(tnr)) {
                strncpy(t->train_nr, tnr->valuestring, sizeof(t->train_nr) - 1);
            } else if (cJSON_IsNumber(tnr)) {
                snprintf(t->train_nr, sizeof(t->train_nr), "%d", tnr->valueint);
            } else {
                strncpy(t->train_nr, t->train_name, sizeof(t->train_nr) - 1); 
            }

            cJSON *stops = cJSON_GetObjectItem(train, "stops");
            if (cJSON_IsArray(stops)) {
                t->stop_count = cJSON_GetArraySize(stops);
                t->stops = malloc(sizeof(PolaczenieStop) * t->stop_count);

                for (int j = 0; j < t->stop_count; j++) {
                    cJSON *stop = cJSON_GetArrayItem(stops, j);
                    PolaczenieStop *s = &t->stops[j];
                    memset(s, 0, sizeof(PolaczenieStop));

                    cJSON *sid = cJSON_GetObjectItem(stop, "station_id");
                    if (cJSON_IsNumber(sid)) s->station_id = sid->valueint;

                    get_stacja_nazwa_by_id_fast(s->station_id, s->name, sizeof(s->name), data_ptr);

                    cJSON *arr = cJSON_GetObjectItem(stop, "arrival");
                    if (cJSON_IsString(arr)) format_time(arr->valuestring, s->arrival, sizeof(s->arrival));

                    cJSON *dep = cJSON_GetObjectItem(stop, "departure");
                    if (cJSON_IsString(dep)) format_time(dep->valuestring, s->departure, sizeof(s->departure));

                    cJSON *plat = cJSON_GetObjectItem(stop, "platform");
                    if (cJSON_IsString(plat)) strncpy(s->platform, plat->valuestring, sizeof(s->platform) - 1);

                    cJSON *track = cJSON_GetObjectItem(stop, "track");
                    if (cJSON_IsString(track)) strncpy(s->track, track->valuestring, sizeof(s->track) - 1);
                }
            }
        }
    }
    cJSON_Delete(root);
    
    if (stations_json) {
        free(stations_json);
    }
    
    return details;
}

void free_ConnectionDetails(PolaczenieSzczegoly* details) {
    if (!details) return;
    for (int i = 0; i < details->train_count; i++) {
        if (details->trains[i].stops) {
            free(details->trains[i].stops);
        }
    }
    if (details->trains) {
        free(details->trains);
    }
    details->train_count = 0;
    details->trains = NULL;
}

bool koleo_api_fetch_and_parse_layout(const char* train_id, int train_class, KoleoTrainLayout* layout_out) {
    if (!train_id || !layout_out) return false;
    
    char* local_buffer = NULL;
    const char* json_data = (const char*)typy_wagon.data; 

    if (!json_data) {
        FILE *f = fopen("/3ds/koleo3ds/typy_wagon.json", "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            size_t size = ftell(f);
            rewind(f);
            local_buffer = malloc(size + 1);
            if (local_buffer) {
                fread(local_buffer, 1, size, f);
                local_buffer[size] = '\0';
                json_data = local_buffer;
            }
            fclose(f);
        }
    }

    if (!json_data) return false;

    cJSON *root = cJSON_Parse(json_data);
    bool found = false;
    if (root) {
        cJSON *entry = NULL;
        cJSON_ArrayForEach(entry, root) {
            cJSON *key = cJSON_GetObjectItem(entry, "key");
            cJSON *class_val = cJSON_GetObjectItem(entry, "class");
            
            int entry_class = (class_val && cJSON_IsNumber(class_val)) ? class_val->valueint : 2; 

            if (cJSON_IsString(key) && strcmp(key->valuestring, train_id) == 0 && entry_class == train_class) {
                
                cJSON *img_key = cJSON_GetObjectItem(entry, "image_key");
                if (cJSON_IsString(img_key)) {
                    layout_out->image_key = strdup(img_key->valuestring);
                }
                layout_out->key = strdup(key->valuestring);

                cJSON *seats_arr = cJSON_GetObjectItem(entry, "seats");
                if (cJSON_IsArray(seats_arr)) {
                    layout_out->seat_count = cJSON_GetArraySize(seats_arr);
                    layout_out->seats = malloc(sizeof(KoleoSeat) * layout_out->seat_count);
                    
                    for (int i = 0; i < layout_out->seat_count; i++) {
                        cJSON *seat = cJSON_GetArrayItem(seats_arr, i);
                        
                        cJSON *nr = cJSON_GetObjectItem(seat, "number");
                        cJSON *seat_type = cJSON_GetObjectItem(seat, "seat_type_id");
                        cJSON *x = cJSON_GetObjectItem(seat, "x");
                        cJSON *y = cJSON_GetObjectItem(seat, "y");
                        cJSON *comp_type = cJSON_GetObjectItem(seat, "compartment_type_id");
                        cJSON *place_id = cJSON_GetObjectItem(seat, "placement_id");
                        
                        layout_out->seats[i].nr = nr ? (cJSON_IsString(nr) ? atoi(nr->valuestring) : nr->valueint) : 0;
                        layout_out->seats[i].seat_type_id = seat_type ? seat_type->valueint : 0;
                        layout_out->seats[i].x = x ? x->valueint : 0;
                        layout_out->seats[i].y = y ? y->valueint : 0;
                        layout_out->seats[i].compartment_type_id = comp_type ? comp_type->valueint : 0;
                        layout_out->seats[i].placement_id = place_id ? place_id->valueint : 0;
                        layout_out->seats[i].is_reserved = false; 
                    }
                }
                found = true;
                break;
            }
        }
        cJSON_Delete(root);
    }

    if (local_buffer) free(local_buffer);
    return found;
}

void koleo_api_free_layout(KoleoTrainLayout* layout) {
    if (!layout) return;
    if (layout->key) free(layout->key);
    if (layout->image_key) free(layout->image_key);
    if (layout->seats) free(layout->seats);
    memset(layout, 0, sizeof(KoleoTrainLayout));
}

bool koleo_api_reserve_seat(long long connection_id, uint16_t seat_nr, float price, int* tariff_ids, int tariff_count) {
    cJSON *root = cJSON_CreateObject();
    
    cJSON *tariffs = cJSON_CreateIntArray(tariff_ids, tariff_count);
    cJSON_AddItemToObject(root, "tariff_ids", tariffs);
    
    char formatted_price[32];
    snprintf(formatted_price, sizeof(formatted_price), "%.2f", price);
    cJSON_AddRawToObject(root, "displayed_price", formatted_price);

    cJSON *extras = cJSON_CreateArray();
    cJSON *seat_extra = cJSON_CreateObject();
    cJSON_AddStringToObject(seat_extra, "type", "seat");
    cJSON_AddNumberToObject(seat_extra, "seat_number", seat_nr);
    cJSON_AddItemToArray(extras, seat_extra);
    cJSON_AddItemToObject(root, "extras", extras);

    char *json_body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    char endpoint[128];
    snprintf(endpoint, sizeof(endpoint), "v2/main/connections/%lld/reservation", connection_id);
    char* full_url = sklejURL(endpoint);

    queue_request(full_url, json_body, dajHeadery(true), &rezerwuj_polaczenie, false, POST);

    free(full_url);
    free(json_body);
    return true;
}

ResponseBuffer flat_train_place_types_buf = {NULL, 0, 0, false};
ResponseBuffer train_composition_buf = {NULL, 0, 0, false};
ResponseBuffer seats_availability_buf = {NULL, 0, 0, false};

void get_FlatTrainPlaceTypes(long long connection_id, int* tariff_ids, int tariff_count) {
    cJSON *root = cJSON_CreateObject();
    
    cJSON *tariffs = cJSON_CreateIntArray(tariff_ids, tariff_count);
    cJSON_AddItemToObject(root, "tariff_ids", tariffs);
    
    char *json_body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    char endpoint[128];
    snprintf(endpoint, sizeof(endpoint), "v2/main/flat_train_place_types/%lld", connection_id);
    char* full_url = sklejURL(endpoint);

    queue_request(full_url, json_body, dajHeadery(true), &flat_train_place_types_buf, false, POST);

    free(full_url);
    free(json_body);
}

void get_TrainComposition(long long connection_id, int train_nr, int place_type_id) {
    char endpoint[128];
    snprintf(endpoint, sizeof(endpoint), "v2/main/train_composition/%lld/%d/%d", connection_id, train_nr, place_type_id);
    char* full_url = sklejURL(endpoint);

    queue_request(full_url, "", dajHeadery(true), &train_composition_buf, false, GET);
    free(full_url);
}

void get_SeatsAvailability(long long connection_id, int train_nr, int place_type_id) {
    char endpoint[128];
    snprintf(endpoint, sizeof(endpoint), "v2/main/seats_availability/%lld/%d/%d", connection_id, train_nr, place_type_id);
    char* full_url = sklejURL(endpoint);

    queue_request(full_url, "", dajHeadery(true), &seats_availability_buf, false, GET);
    free(full_url);
}

typedef struct {
    int id;
    bool direction; // true for left, false for right/other
} SeatTypeDirectionMap;

static SeatTypeDirectionMap g_seat_type_directions[64];
static int g_seat_type_count = 0;

bool get_seat_direction(int seat_type_id) {
    for (int i = 0; i < g_seat_type_count; i++) {
        if (g_seat_type_directions[i].id == seat_type_id) {
            return g_seat_type_directions[i].direction;
        }
    }
    return false; 
}

bool load_seat_types(void) {
    FILE *f = fopen("/3ds/koleo3ds/typy_miejsc.json", "rb");
    log_to_file("Loading seat types from typy_miejsc.json");
    if (!f) return false;
    log_to_file("File opened successfully");

    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    rewind(f);

    char *buffer = malloc(size + 1);
    if (!buffer) {
        fclose(f);
        return false;
    }

    fread(buffer, 1, size, f);
    buffer[size] = '\0';
    fclose(f);

    cJSON *root = cJSON_Parse(buffer);
    free(buffer);
    if (!root) return false;

    g_seat_type_count = 0;
    if (cJSON_IsArray(root)) {
        cJSON *entry = NULL;
        cJSON_ArrayForEach(entry, root) {
            if (g_seat_type_count >= 64) break;

            cJSON *id_obj = cJSON_GetObjectItem(entry, "id");
            cJSON *key_obj = cJSON_GetObjectItem(entry, "key");

            if (cJSON_IsNumber(id_obj) && cJSON_IsString(key_obj)) {
                g_seat_type_directions[g_seat_type_count].id = id_obj->valueint;
                
                log_to_file("Loading seat type: id=%d, key=%s", id_obj->valueint, key_obj->valuestring);
                if (strstr(key_obj->valuestring, "left")) {
                    g_seat_type_directions[g_seat_type_count].direction = true;
                } else {
                    g_seat_type_directions[g_seat_type_count].direction = false;
                }
                g_seat_type_count++;
            }
        }
    }

    cJSON_Delete(root);
    return g_seat_type_count > 0;
}

static int fast_atoi(const char *s)
{
    int r = 0;
    int sign = 1;

    if (*s == '-') { sign = -1; s++; }

    while (*s >= '0' && *s <= '9')
        r = r * 10 + (*s++ - '0');

    return r * sign;
}

static const char* find_key(const char *ptr, const char *key)
{
    size_t klen = strlen(key);

    while ((ptr = strstr(ptr, key)) != NULL)
    {
        if (ptr[klen] == '"' || ptr[klen] == ':' || ptr[klen] == ' ')
            return ptr;
        ptr++;
    }
    return NULL;
}

bool load_and_parse_carriage(int carriage_type_id, KoleoTrainLayout* out)
{
    if (!out) return false;
    memset(out, 0, sizeof(*out));

    const char *json = (const char*)typy_wagon.data;
    char *alloc = NULL;

    if (!json)
    {
        FILE *f = fopen("/3ds/koleo3ds/typy_wagon.json", "rb");
        if (!f) return false;

        fseek(f, 0, SEEK_END);
        size_t sz = ftell(f);
        rewind(f);

        alloc = malloc(sz + 1);
        if (!alloc) { fclose(f); return false; }

        fread(alloc, 1, sz, f);
        alloc[sz] = '\0';
        fclose(f);

        json = alloc;
    }

    const char *p = json;

    while ((p = strchr(p, '{')) != NULL)
    {
        const char *start = p;
        int depth = 0;
        const char *end = NULL;

        for (const char *c = p; *c; c++)
        {
            if (*c == '{') depth++;
            else if (*c == '}')
            {
                depth--;
                if (depth == 0)
                {
                    end = c;
                    break;
                }
            }
        }

        if (!end) break;

        const char *id_pos = find_key(start, "\"id\"");
        if (id_pos)
        {
            const char *colon = strchr(id_pos, ':');
            if (colon && fast_atoi(colon + 1) == carriage_type_id)
            {
                const char *seats_key = find_key(start, "\"seats\"");
                if (!seats_key)
                {
                    if (alloc) free(alloc);
                    return false;
                }

                const char *arr = strchr(seats_key, '[');
                if (!arr)
                {
                    if (alloc) free(alloc);
                    return false;
                }

                int count = 0;
                for (const char *c = arr; *c && *c != ']'; c++)
                    if (*c == '{') count++;

                out->seat_count = count;
                out->seats = malloc(sizeof(KoleoSeat) * count);
                if (!out->seats)
                {
                    if (alloc) free(alloc);
                    return false;
                }

                int i = 0;
                const char *c = arr;

                while (*c && i < count)
                {
                    if (*c != '{') { c++; continue; }

                    const char *s_start = c;
                    int d = 0;

                    do {
                        if (*c == '{') d++;
                        else if (*c == '}') d--;
                        c++;
                    } while (*c && d);

                    size_t len = c - s_start;
                    char *tmp = malloc(len + 1);
                    if (!tmp) break;

                    memcpy(tmp, s_start, len);
                    tmp[len] = '\0';

                    KoleoSeat *s = &out->seats[i++];
                    memset(s, 0, sizeof(*s));

                    const char *nr = find_key(tmp, "\"nr\"");
                    if (!nr) nr = find_key(tmp, "\"number\"");
                    if (!nr) nr = find_key(tmp, "\"seat_number\"");

                    if (nr)
                    {
                        const char *col = strchr(nr, ':');
                        s->nr = col ? fast_atoi(col + 1) : 0;
                    }

                    const char *x = find_key(tmp, "\"x\"");
                    const char *y = find_key(tmp, "\"y\"");
                    const char *t = find_key(tmp, "\"seat_type_id\"");
                    const char *cp = find_key(tmp, "\"compartment_type_id\"");
                    const char *pl = find_key(tmp, "\"placement_id\"");

                    if (x) s->x = fast_atoi(strchr(x, ':') + 1);
                    if (y) s->y = fast_atoi(strchr(y, ':') + 1);
                    if (t) s->seat_type_id = fast_atoi(strchr(t, ':') + 1);
                    if (cp) s->compartment_type_id = fast_atoi(strchr(cp, ':') + 1);
                    if (pl) s->placement_id = fast_atoi(strchr(pl, ':') + 1);

                    s->direction = get_seat_direction(s->seat_type_id);
                    s->is_reserved = false;

                    free(tmp);
                }

                if (alloc) free(alloc);
                return true;
            }
        }

        p = end + 1;
    }

    if (alloc) free(alloc);
    return false;
}

void koleo_api_apply_availability(const char* availability_json, KoleoTrainLayout* layout, const char* carriage_nr) {
    if (!availability_json || !layout || !carriage_nr) return;
    log_to_file("Applying availability for carriage %s", carriage_nr);

    cJSON *root = cJSON_Parse(availability_json);
    if (!root) return;

    for (int i = 0; i < layout->seat_count; i++) {
        log_to_file("Marking seat %d as reserved by default", layout->seats[i].nr);
        layout->seats[i].is_reserved = true; 
    }

    cJSON *seats_arr = cJSON_GetObjectItem(root, "seats");
    if (cJSON_IsArray(seats_arr)) {
        cJSON *seat_item;
        cJSON_ArrayForEach(seat_item, seats_arr) {
            cJSON *c_nr = cJSON_GetObjectItem(seat_item, "carriage_nr");
            log_to_file("Processing availability for carriage_nr: %s", cJSON_IsString(c_nr) ? c_nr->valuestring : "N/A");
            
            char current_c_nr[32] = {0};
            if (cJSON_IsString(c_nr)) {
                strncpy(current_c_nr, c_nr->valuestring, 31);
            } else if (cJSON_IsNumber(c_nr)) {
                snprintf(current_c_nr, sizeof(current_c_nr), "%d", c_nr->valueint);
            }

            if (strcmp(current_c_nr, carriage_nr) != 0) continue;

            cJSON *s_nr = cJSON_GetObjectItem(seat_item, "seat_nr");
            cJSON *state = cJSON_GetObjectItem(seat_item, "state");
            cJSON *special_comp = cJSON_GetObjectItem(seat_item, "special_compartment_type_id");

            if (s_nr && cJSON_IsString(state)) {
                int seat_num = 0;
                if (cJSON_IsString(s_nr)) seat_num = atoi(s_nr->valuestring);
                else if (cJSON_IsNumber(s_nr)) seat_num = s_nr->valueint;

                bool is_free = (strcmp(state->valuestring, "FREE") == 0);

                for (int i = 0; i < layout->seat_count; i++) {
                    if (layout->seats[i].nr == seat_num) {
                        layout->seats[i].is_reserved = !is_free;
                        
                        if (cJSON_IsNumber(special_comp)) {
                            layout->seats[i].compartment_type_id = special_comp->valueint;
                        }
                        break; 
                    }
                }
                log_to_file("Seat %d in carriage %s is marked as %s", seat_num, carriage_nr, is_free ? "FREE" : "RESERVED");
            }
        }
    }
    cJSON_Delete(root);
}

void rezerwuj_Polaczenie_Complex(long long connection_id, float price, int* tariff_ids, int tariff_count, const char* train_nr, int place_type_id, const char* carriage_nr, int* seats, int* compartment_types, int seat_count, PolaczenieSzczegoly* details) {
    cJSON *root = cJSON_CreateObject();
    
    cJSON_AddItemToObject(root, "tariff_ids", cJSON_CreateIntArray(tariff_ids, tariff_count));
    
    cJSON *place_types = cJSON_CreateArray();
    if (details && details->train_count > 0) {
        for (size_t i = 0; i < details->train_count; i++) {
            cJSON *pt_item = cJSON_CreateObject();
            cJSON_AddStringToObject(pt_item, "train_nr", details->trains[i].train_nr);
            
            if (strcmp(details->trains[i].train_nr, train_nr) == 0) {
                cJSON_AddNumberToObject(pt_item, "place_type_id", place_type_id);
            } else {
                cJSON_AddNumberToObject(pt_item, "place_type_id", -1);
            }
            cJSON_AddItemToArray(place_types, pt_item);
        }
    } else {
        cJSON *pt_item = cJSON_CreateObject();
        cJSON_AddStringToObject(pt_item, "train_nr", train_nr);
        cJSON_AddNumberToObject(pt_item, "place_type_id", place_type_id);
        cJSON_AddItemToArray(place_types, pt_item);
    }
    cJSON_AddItemToObject(root, "place_types", place_types);

    cJSON *seats_res = cJSON_CreateArray();
    cJSON *sr_item = cJSON_CreateObject();
    cJSON_AddStringToObject(sr_item, "train_nr", train_nr);
    cJSON_AddStringToObject(sr_item, "used_reservation_mode", "seat_map");
    
    cJSON *seats_arr = cJSON_CreateArray();
    for (int i = 0; i < seat_count; i++) {
        cJSON *seat = cJSON_CreateObject();
        cJSON_AddStringToObject(seat, "carriage_nr", carriage_nr);
        
        char s_nr[16];
        snprintf(s_nr, sizeof(s_nr), "%d", seats[i]);
        cJSON_AddStringToObject(seat, "seat_nr", s_nr);
        
        if (compartment_types && (compartment_types[i] == 7 || compartment_types[i] == 17)) {
            cJSON_AddNumberToObject(seat, "special_compartment_type_id", compartment_types[i]);
        }
        
        cJSON_AddItemToArray(seats_arr, seat);
    }
    
    cJSON_AddItemToObject(sr_item, "seats", seats_arr);
    cJSON_AddItemToArray(seats_res, sr_item);
    cJSON_AddItemToObject(root, "seats_reservations", seats_res);

    cJSON_AddItemToObject(root, "option_groups", cJSON_CreateArray());

    char price_str[32];
    snprintf(price_str, sizeof(price_str), "%.2f", price);
    cJSON_AddRawToObject(root, "displayed_price", price_str);

    char *json_body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    char endpoint[128];
    snprintf(endpoint, sizeof(endpoint), "v2/main/connections/%lld/reservation", connection_id);
    char* full_url = sklejURL(endpoint);

    queue_request(full_url, json_body, dajHeadery(true), &rezerwuj_polaczenie, false, POST);

    free(full_url);
    free(json_body); 
}

ConnectionPlaceTypes parse_FlatTrainPlaceTypes(const char* json_string) {
    ConnectionPlaceTypes result = {0};
    cJSON *root = cJSON_Parse(json_string);
    if (!root || !cJSON_IsArray(root)) {
        if (root) cJSON_Delete(root);
        return result;
    }

    int i = 0;
    cJSON *train_node = NULL;
    cJSON_ArrayForEach(train_node, root) {
        if (i >= MAX_CONN_TRAINS) break;
        
        TrainPlaceTypes *tpt = &result.trains[i];
        tpt->fallback_id = -1; 

        cJSON *tnr = cJSON_GetObjectItem(train_node, "train_nr");
        if (cJSON_IsNumber(tnr)) {
            snprintf(tpt->train_nr, sizeof(tpt->train_nr), "%d", tnr->valueint);
        } else if (cJSON_IsString(tnr)) {
            strncpy(tpt->train_nr, tnr->valuestring, sizeof(tpt->train_nr) - 1);
        }

        cJSON *flat_place_types = cJSON_GetObjectItem(train_node, "flat_place_types");
        if (flat_place_types && cJSON_IsArray(flat_place_types) && cJSON_GetArraySize(flat_place_types) > 0) {
            cJSON *category = cJSON_GetArrayItem(flat_place_types, 0);
            cJSON *place_types = cJSON_GetObjectItem(category, "place_types");
            
            if (place_types && cJSON_IsArray(place_types)) {
                int c_idx = 0;
                cJSON *pt = NULL;
                cJSON_ArrayForEach(pt, place_types) {
                    if (c_idx >= MAX_TRAIN_CLASSES) break;
                    
                    cJSON *id = cJSON_GetObjectItem(pt, "id");
                    tpt->classes[c_idx].id = cJSON_IsNumber(id) ? id->valueint : 0;
                    
                    cJSON *name = cJSON_GetObjectItem(pt, "name");
                    if (cJSON_IsString(name)) {
                        strncpy(tpt->classes[c_idx].name, name->valuestring, sizeof(tpt->classes[c_idx].name) - 1);
                    }
                    
                    cJSON *price = cJSON_GetObjectItem(pt, "price");
                    if (cJSON_IsString(price)) tpt->classes[c_idx].price = atof(price->valuestring);
                    else if (cJSON_IsNumber(price)) tpt->classes[c_idx].price = price->valuedouble;
                    
                    cJSON *available = cJSON_GetObjectItem(pt, "available");
                    tpt->classes[c_idx].available = cJSON_IsTrue(available);
                    
                    c_idx++;
                }
                tpt->class_count = c_idx;
            }
        } else {
            tpt->class_count = 0;
        }
        i++;
    }
    result.train_count = i;
    cJSON_Delete(root);
    return result;
}

TrainComposition parse_TrainComposition(const char* json_string) {
    TrainComposition result = {NULL, 0};
    cJSON *root = cJSON_Parse(json_string);
    if (!root) return result;

    cJSON *carriages = cJSON_GetObjectItem(root, "carriages");
    if (!carriages || !cJSON_IsArray(carriages)) {
        cJSON_Delete(root);
        return result;
    }

    result.count = cJSON_GetArraySize(carriages);
    result.carriages = malloc(sizeof(Carriage) * result.count);
    if (!result.carriages) {
        cJSON_Delete(root);
        result.count = 0;
        return result;
    }

    int i = 0;
    cJSON *c = NULL;
    cJSON_ArrayForEach(c, carriages) {
        cJSON *position = cJSON_GetObjectItem(c, "position");
        cJSON *number = cJSON_GetObjectItem(c, "number");
        cJSON *type_id = cJSON_GetObjectItem(c, "carriage_type_id");
        cJSON *bookable = cJSON_GetObjectItem(c, "bookable");

        result.carriages[i].position = cJSON_IsNumber(position) ? position->valueint : 0;
        if (cJSON_IsString(number)) {
            strncpy(result.carriages[i].number, number->valuestring, sizeof(result.carriages[i].number) - 1);
            result.carriages[i].number[sizeof(result.carriages[i].number) - 1] = '\0';
        } else {
            result.carriages[i].number[0] = '\0';
        }
        result.carriages[i].type_id = cJSON_IsNumber(type_id) ? type_id->valueint : 0;
        result.carriages[i].bookable = cJSON_IsTrue(bookable);
        i++;
    }
    cJSON_Delete(root);
    return result;
}

SeatLegendList parse_SeatsAvailability(const char* json_string) {
    SeatLegendList list = { .count = 0 };
    cJSON *root = cJSON_Parse(json_string);
    if (!root || !cJSON_IsArray(root)) return list;

    int i = 0;
    cJSON *item = NULL;
    cJSON_ArrayForEach(item, root) {
        if (i >= 8) break;
        list.items[i].id = cJSON_GetObjectItem(item, "id")->valueint;
        strncpy(list.items[i].name, cJSON_GetObjectItem(item, "name")->valuestring, 31);
        i++;
    }
    list.count = i;
    cJSON_Delete(root);
    return list;
}

