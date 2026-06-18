#ifndef KOLEO_H
#define KOLEO_H

#include "request.h"
#define KOLEO_BASE_URL "https://api.koleo.pl/"

typedef struct {
    char imie[64];  
    char nazwisko[64];
    int pasazer_id;
    bool selected;
} Pasazer;

typedef struct {
    Pasazer *ziomy;
    size_t count;
} Pasazer_List;

typedef struct {
    char imie[64];
    char nazwisko[64];
    int pasazer_id;
    float stan_konta;
    char urodziny[32];
    int id_znizki;
    char email[128];
} UserDane;

typedef struct {
    char nazwa[128];  
    char nazwa_slug[128];
    char kraj[64];
    char region[64];
    char miasto[64];
    int stacja_id;
} Stacja;

typedef struct {
    Stacja *station;
    size_t count;
} Stacja_List;

typedef struct {
    char uuid[64];
    char nazwa[128];
    float cena;
    char waluta[16];
    char czas_odjazdu[32];
    char czas_dotarcia[32];
    int dlugosc_trasy;
    bool bezposrednie;
    int przesiadki;      
    long long connection_id;   
} Polaczenie;

typedef struct {
    Polaczenie *polaczenia;
    size_t count;
} Polaczenie_List;

typedef struct {
    int station_id;
    char name[64];
    char arrival[16];
    char departure[16];
    char platform[16];
    char track[16];
} PolaczenieStop;

typedef struct {
    int id;
    char train_name[64];
    char train_nr[32]; 
    PolaczenieStop* stops;
    size_t stop_count;
} PolaczenieTrain;

typedef struct {
    PolaczenieTrain* trains;
    size_t train_count;
} PolaczenieSzczegoly;

#define MAX_TICKETS 90

typedef struct {
    long long order_id;
    char name[128];
    char valid_to[32];
    char valid_from[32]; 
} ActiveTicket;

extern ResponseBuffer testreq;

extern char* refreshToken;
extern char* authToken;

void testrequest();
char* dawajAuth();
struct curl_slist* dajHeadery(bool auth);
char* sklejURL(const char* endpoint);
void format_time(const char *iso, char *out, size_t out_size);

extern ResponseBuffer all_stacjas; 
extern ResponseBuffer typy_miejsc;
extern ResponseBuffer znizki;
extern ResponseBuffer typy_wagon;
extern ResponseBuffer atrybut_definicje;
extern ResponseBuffer przewoznicy;
extern ResponseBuffer marki;

void getAll_Stacje();
void getAll_TypyMiejsc();
void getAll_Znizki();
void getAll_WagonTypy();
void getAll_AtrybutDefinicje();
void getAll_Przewoznicy();
void getAll_Marki();

extern ResponseBuffer konto_login;
extern ResponseBuffer user_dane;
extern ResponseBuffer pasazerowie;
extern ResponseBuffer deselect_pasazer;
extern ResponseBuffer select_pasazer;
extern ResponseBuffer pobierz_bilety;
extern ResponseBuffer pobierz_bilet;
extern ResponseBuffer refresh_token;

void kontoLogin(char email[60], char haslo[60]);
void refresh_the_Token();
void get_Pasazerowie();
void get_UserDane();
void deselect_Pasazer(int pasazer_id);
void select_Pasazer(int pasazer_id);

void pobierz_Bilety();
void pobierz_Bilet(long long order_id);

extern ResponseBuffer daj_cene_przejazdu;
extern ResponseBuffer dawaj_stacje_for_me;
extern ResponseBuffer dawaj_connection_id;
extern ResponseBuffer connection_details_buf;
extern ResponseBuffer connection_price_buf;

void getStacje_for_Me(int startowa_stacja, int koncowa_stacja, const char* kiedy, bool bezposrednie);
void getPrzejazd_Cena(const char* uuid);

void get_ConnectionId(const char* uuid);

void get_ConnectionDetails(long long connection_id);
void get_ConnectionPrice(long long connection_id);

extern ResponseBuffer doladuj_konto;
extern ResponseBuffer nwm_blik;
extern ResponseBuffer platnosc_blik;
extern ResponseBuffer platnosc_status;
extern ResponseBuffer rezerwuj_polaczenie;
extern ResponseBuffer zaplać_stanem_konta;

void rezerwuj_Polaczenie(long long connection_id, float price, int* tariff_ids, int tariff_count, PolaczenieSzczegoly* details);

long long parse_PaymentId(const char* json_string);
void doladuj_Konto(float ilosc);
void platnosc_BLIK(int id_platnosc, const char* blik_kod);

void get_Order_ID_from_BLIK(int id_platnosc, const char* blik_kod);

void getPlatnosc_Status(int id_platnosc);
void zaplac_stanemKonta(long long payment_id);

extern Pasazer_List global_lista;

Pasazer create_pasazer(const char *imie, const char *nazwisko, int pasazer_id);
Pasazer_List parse_Pasazerowie(const char *json_string);

void parse_AuthResponse(const char* json);

extern UserDane dane_uzytkownika;

void parse_UserDane(const char* json);

extern Stacja_List all_stacja_list;

Stacja create_stacja(const char *nazwa, const char *nazwa_slug, const char *miasto, const char *region, const char *kraj, int stacja_id);

Stacja_List search_stations(const char *json_string, const char *query);
int znajdz_stacja_id(const char *query);
void get_stacja_nazwa_by_id_fast(int id, char* out_name, size_t max_len, const char* data_ptr);

void get_stacja_nazwa_by_id(int id, char* out_name, size_t max_len);

extern Polaczenie_List global_polaczenia_list;

Polaczenie create_polaczenie(const char* uuid, const char* nazwa, float cena, const char* waluta, const char* czas_odjazdu, const char* czas_dotarcia, int dlugosc_trasy, bool bezposrednie, int przesiadki, int connection_id);

Polaczenie_List parse_Polaczenia(const char* json_string);

long long parse_ConnectionId(const char* json_string);

float parse_ConnectionPrice(const char* json_string);

PolaczenieSzczegoly parse_ConnectionDetails(const char* json_string);

void free_ConnectionDetails(PolaczenieSzczegoly* details);

typedef enum {
    TrainClass1st = 1,
    TrainClass2nd = 2,
    TrainClassAny = 0
} TrainClass;

typedef struct {
    uint16_t nr;
    uint8_t seat_type_id;
    uint16_t x;
    uint16_t y;
    uint8_t compartment_type_id;
    uint8_t placement_id;
    bool is_reserved; 
    bool direction; 
} KoleoSeat;

typedef struct {
    char* key;          
    char* image_key;    
    KoleoSeat* seats;
    size_t seat_count;
} KoleoTrainLayout;

bool koleo_api_fetch_and_parse_layout(const char* train_id, int train_class, KoleoTrainLayout* layout_out);
bool load_seat_types(void);
bool load_and_parse_carriage(int carriage_type_id, KoleoTrainLayout* layout_out);
void koleo_api_apply_availability(const char* availability_json, KoleoTrainLayout* layout, const char* carriage_nr);
void koleo_api_free_layout(KoleoTrainLayout* layout);
bool koleo_api_reserve_seat(long long connection_id, uint16_t seat_nr, float price, int* tariff_ids, int tariff_count);

extern ResponseBuffer flat_train_place_types_buf;
extern ResponseBuffer train_composition_buf;
extern ResponseBuffer seats_availability_buf;

void get_FlatTrainPlaceTypes(long long connection_id, int* tariff_ids, int tariff_count);
void get_TrainComposition(long long connection_id, int train_nr, int place_type_id);
void get_SeatsAvailability(long long connection_id, int train_nr, int place_type_id);

void rezerwuj_Polaczenie_Complex(long long connection_id, float price, int* tariff_ids, int tariff_count, const char* train_nr, int place_type_id, const char* carriage_nr, int* seats, int* compartment_types, int seat_count, PolaczenieSzczegoly* details);

#define MAX_CONN_TRAINS 4
#define MAX_TRAIN_CLASSES 4

typedef struct {
    int id;
    char name[32];
    float price;
    bool available;
} TicketClass;

typedef struct {
    char train_nr[32]; 
    TicketClass classes[MAX_TRAIN_CLASSES];
    int class_count;
    int fallback_id; 
} TrainPlaceTypes;

typedef struct {
    TrainPlaceTypes trains[MAX_CONN_TRAINS];
    int train_count;
} ConnectionPlaceTypes;

typedef struct {
    int position;
    char number[8];
    int type_id;
    bool bookable;
} Carriage;

typedef struct {
    Carriage* carriages;
    int count;
} TrainComposition;

typedef struct {
    int id;
    char name[32];
} SeatLegend;

typedef struct {
    SeatLegend items[8];
    int count;
} SeatLegendList;

ConnectionPlaceTypes parse_FlatTrainPlaceTypes(const char* json_string);

TrainComposition parse_TrainComposition(const char* json_string);

SeatLegendList parse_SeatsAvailability(const char* json_string);
#endif