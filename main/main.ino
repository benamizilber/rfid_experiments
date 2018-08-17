/*
   -----------------------------------------------------------------------------------------
   Typical pin layout used:
   -----------------------------------------------------------------------------------------
               MFRC522      Arduino       Arduino   Arduino    Arduino          Arduino
               Reader/PCD   Uno/101       Mega      Nano v3    Leonardo/Micro   Pro Micro
   Signal      Pin          Pin           Pin       Pin        Pin              Pin
   -----------------------------------------------------------------------------------------
   RST/Reset   RST          9             5         D9         RESET/ICSP-5     RST
   SPI SS      SDA(SS)      10            53        D10        10               10
   SPI MOSI    MOSI         11 / ICSP-4   51        D11        ICSP-4           16
   SPI MISO    MISO         12 / ICSP-1   50        D12        ICSP-1           14
   SPI SCK     SCK          13 / ICSP-3   52        D13        ICSP-3           15
*/

#include <SPI.h>
#include <MFRC522.h>

constexpr uint8_t NR_OF_READERS = 3;


constexpr uint8_t RST_3_PIN = 2;     // Configurable, see typical pin layout above
constexpr uint8_t RST_2_PIN = 3;
constexpr uint8_t RST_1_PIN = 4;
byte resetPins[] = {RST_1_PIN, RST_2_PIN, RST_3_PIN};

constexpr uint8_t SS_3_PIN = 53;     // Configurable, see typical pin layout above
constexpr uint8_t SS_2_PIN = 49;
constexpr uint8_t SS_1_PIN = 48;
byte ssPins[] = {SS_1_PIN, SS_2_PIN, SS_3_PIN};

constexpr uint8_t RED_LED_1 = 13;
constexpr uint8_t RED_LED_2 = 11;
constexpr uint8_t RED_LED_3 = 9;

constexpr uint8_t BLUE_LED_1 = 12;
constexpr uint8_t BLUE_LED_2 = 10;
constexpr uint8_t BLUE_LED_3 = 8;

byte redLed[] = {RED_LED_1, RED_LED_2, RED_LED_3};
byte blueLed[] = {BLUE_LED_1, BLUE_LED_2, BLUE_LED_3};

int yellowLed = 7;

MFRC522 *rfid[NR_OF_READERS];   // Create MFRC522 instance.

bool is_equal(byte id1[4], byte id2[4]) {
  return (id1[0] == id2[0] && id1[1] == id2[1] && id1[2] == id2[2] && id1[3] == id2[3]);
}

bool is_id0_equal(byte* id1, byte* id2) {
  return (id1[0] == id2[0]);
}

#define NUM_CARDS_IN_GAME 7
#define MAX_CARDS_IN_HAND 3
#define MIN_CARDS_IN_HAND 2
#define ID_LEN 4
#define CYCLE_TIME_FOR_A_CARD_MS 20

byte card_ids[NUM_CARDS_IN_GAME][ID_LEN] =
{
  {77, 165, 131, 171},
  {7, 167, 122, 137},
  {153, 152, 81, 233},
  {154, 142, 6, 133},
  {86, 64, 120, 137},
  {245, 17, 7, 133},
  {12, 82, 230, 43}
};

MFRC522::Uid cards[NUM_CARDS_IN_GAME];

int JOKER_VALUE = 7;

int index_to_value[NUM_CARDS_IN_GAME] { 8, 8, 
                                        2, 32,
                                        4, 16,
                                        JOKER_VALUE};
                                        
int GROUP_MULTIPLEX_RESULT = index_to_value[0]*index_to_value[1];

int card_value(MFRC522::Uid &card) {
  for (int i = 0; i < NUM_CARDS_IN_GAME; i++) {
    if (is_id0_equal(card_ids[i], card.uidByte)) {
        return index_to_value[i];
     }
  }
}

MFRC522::Uid curr_seen[NR_OF_READERS][MAX_CARDS_IN_HAND];
int curr_seen_count[NR_OF_READERS] = {0};

MFRC522::Uid last_seen[MAX_CARDS_IN_HAND];
int last_seen_count = 0;

void setup() {
  pinMode(yellowLed, OUTPUT);
  digitalWrite(yellowLed, LOW);

  Serial.begin(9600);
  SPI.begin(); // Init SPI bus
  for (uint8_t reader = 0; reader < NR_OF_READERS; reader++) {
    pinMode(redLed[reader], OUTPUT);
    digitalWrite(redLed[reader], LOW);
    pinMode(blueLed[reader], OUTPUT);
    digitalWrite(blueLed[reader], LOW);
    rfid[reader] = new MFRC522(ssPins[reader], resetPins[reader]);
    rfid[reader]->PCD_Init(ssPins[reader], resetPins[reader]); // Init each MFRC522 card
  }

  for (uint8_t reader = 0; reader < NR_OF_READERS; reader++) {
    Serial.print(F("Reader "));
    Serial.print(reader);
    Serial.print(F(": "));
    rfid[reader]->PCD_DumpVersionToSerial();

    for (int i = 0; i < MAX_CARDS_IN_HAND; i++) {
      curr_seen[reader][i].size = ID_LEN;
      //last_seen[i].size = ID_LEN;
    }
  }

  for (int i = 0; i < NUM_CARDS_IN_GAME; i++) {
    cards[i].size = ID_LEN;
    for (int j = 0; j < ID_LEN; j++) {
      cards[i].uidByte[j] = card_ids[i][j];
    }
  }
}

void print_text_of_status(int status) {
  switch (status) {
    case 0:
      Serial.println("OK");
      break;
    case 1:
      Serial.println("Error");
      break;
    case 2:
      Serial.println("Collision");
      break;
    case 3:
      Serial.println("Timeout");
      break;
    case 4:
      Serial.println("No Room");
      break;
    case 5:
      Serial.println("Internal Error");
      break;
    case 6:
      Serial.println("Invalid");
      break;
    case 7:
      Serial.println("CRC Wrong");
      break;
    case 8:
      Serial.println("Mifare Nack");
      break;
    default:
      Serial.println("undefined status number");
  }
}

//typedef struct {
//    byte    size;     // Number of bytes in the UID. 4, 7 or 10.
//    byte    uidByte[10];
//    byte    sak;      // The SAK (Select acknowledge) byte returned from the PICC after successful selection.
//  } Uid;

void loop() {

  byte bufferATQA[2];
  byte bufferSize = sizeof(bufferATQA);


  //  unsigned long CurrentTime = 0;
  //  unsigned long ElapsedTime = 0;

  for (int reader = 0; reader < NR_OF_READERS; reader++) {
    curr_seen_count[reader] = 0;
    for (int i = 0; i < MAX_CARDS_IN_HAND; i++) {
      for (int j = 0; j < ID_LEN; j++) {
        curr_seen[reader][i].uidByte[j] = 0;
      }
    }
  }

  for (int reader = 0; reader < NR_OF_READERS; reader++) {
    int index = 0;
    //    Serial.print("new crds for reader: ");
    //    Serial.println(reader, DEC);
    while ( (curr_seen_count[reader] < MAX_CARDS_IN_HAND) && (index < NUM_CARDS_IN_GAME)) {

      // Look for new cards
      bufferSize = sizeof(bufferATQA);
      MFRC522::StatusCode result_r = rfid[reader]->PICC_WakeupA(bufferATQA, &bufferSize);
      //    Serial.print(F("reqa status = "));
      //    Serial.print(result_r, DEC);
      //    Serial.println();

      if (result_r != 0) {
        if (result_r == 3) {
          break;
        }
        if (result_r != 2) {
          Serial.println("there was an error with request A");
          Serial.print(F("reqA status = "));
          print_text_of_status(result_r);
          //        Serial.print(result_r, DEC);
          //        Serial.println();
          Serial.print(F("bufferSize = "));
          Serial.print(bufferSize, DEC);
          Serial.println();
        }
        //digitalWrite(red_led, LOW);
        //digitalWrite(blue_led, LOW);
        continue;
      }

      MFRC522::Uid temp = cards[index];
      //    Serial.print("trying to select card:");
      //    Serial.print(temp.uidByte[0], DEC);
      //    Serial.println();
      MFRC522::StatusCode result_s = rfid[reader]->PICC_Select(&temp, 32);
      //Serial.print(F("select status = "));
      //Serial.print(result_s, DEC);
      //Serial.println();

      index++;

      if (result_s != 0) {
        if (result_s != 3) {
          Serial.println("there was an error with select");
          Serial.print(F("select status = "));
          print_text_of_status(result_s);
          //        Serial.print(result_s, DEC);
          //        Serial.println();
        }
        continue;
      }

      //check that this card was not already seen
      bool is_already_seen = false;
      for (int i = 0; i < curr_seen_count[reader]; i++ ) {
        if (curr_seen[reader][i].uidByte[0] == temp.uidByte[0]) {
          is_already_seen = true;
          Serial.println("this card was already seen and should and for some reason found again as a new card:");
          Serial.print(temp.uidByte[0], DEC);
          Serial.println();
          Serial.println("we should halt this card before continue");
          MFRC522::StatusCode result_a = rfid[reader]->PICC_HaltA();
          if (result_a != 0) {
            Serial.println("there was an error halting this card:");
            Serial.print(temp.uidByte[0], DEC);
            Serial.println();
          }
          break;
        }
      }
      if (is_already_seen) {
        continue;
      }

      for (int i = 0; i < ID_LEN; i++) {
        curr_seen[reader][curr_seen_count[reader]].uidByte[i] = temp.uidByte[i];
      }

      //    Serial.println("***************************");
      //    Serial.print(F("found new card: "));
      //    Serial.print(temp.uidByte[0], DEC);
      //    Serial.println();
      //    Serial.println("***************************");

      curr_seen_count[reader]++;

      // Halt PICC
      MFRC522::StatusCode result_a = rfid[reader]->PICC_HaltA();
      if (result_a != 0) {
        Serial.println("there was an error halting this card:");
        Serial.print(curr_seen[reader][curr_seen_count[reader]].uidByte[0], DEC);
        Serial.println();
      }
      //Serial.print(F("halt status = "));
      //Serial.print(result_a, DEC);
      //Serial.println();

    }
  }

  bool someone_has_joker = false;
  bool player_with_joker_has_group = false;
  bool all_players_have_minimum_cards = true;
  bool num_of_players_with_group = 0;

  for (int reader = 0; reader < NR_OF_READERS; reader++) {
    bool player_has_joker = false;
    int32_t cards_value = 1;
    Serial.print("reader ");
    Serial.print(reader, DEC);
    Serial.print(" : ");
    if (curr_seen_count[reader] < MIN_CARDS_IN_HAND) {
      all_players_have_minimum_cards = false;
    }
    for (int i = 0; i < curr_seen_count[reader]; i++) {
      cards_value *= card_value(curr_seen[reader][i]);
      Serial.print(curr_seen[reader][i].uidByte[0], DEC);
      Serial.print(" ");
    }
    Serial.print("value: ");
    Serial.print(cards_value, DEC);
    Serial.print(" ");
    if (cards_value % JOKER_VALUE == 0) {
      Serial.print("joker");
      Serial.print(" ");
      digitalWrite(redLed[reader], HIGH);
      player_has_joker = true;
      someone_has_joker = true;
      cards_value = cards_value / JOKER_VALUE;
    } else {
      digitalWrite(redLed[reader], LOW);
    }
    if (cards_value == GROUP_MULTIPLEX_RESULT) {
      Serial.print("group");
      Serial.print(" ");
      digitalWrite(blueLed[reader], HIGH);
      if (player_has_joker) {
        player_with_joker_has_group = true;
      }
      num_of_players_with_group++;
    } else {
      digitalWrite(blueLed[reader], LOW);
    }
    Serial.println();
  }

  if (all_players_have_minimum_cards && someone_has_joker) {
    if ( (num_of_players_with_group == 1 && !player_with_joker_has_group) ||
         (num_of_players_with_group > 1) ) {
      digitalWrite(yellowLed, HIGH);
    } else {
        digitalWrite(yellowLed, LOW);
    }
  } else {
    Serial.println("SET up is wrong");
    for (int i = 0; i < 1000; i++) {
      digitalWrite(yellowLed, HIGH);
      delay(1);
      digitalWrite(yellowLed, LOW);
      delay(10);
    }
  }

  for (int i = 0; i < (23 - NR_OF_READERS - 1); i++) {
    Serial.println();
  }

  //  bool has_joker = false;
  //  bool has_group = false;
  //
  //  if (curr_seen_count >= MIN_CARDS_IN_HAND ) {
  //    int cards_value_sum = 0;
  //    for (int i = 0; i < curr_seen_count; i++) {
  //      for (int j = 0; j < NUM_CARDS_IN_GAME; j++) {
  //        if (is_id0_equal(card_ids[j], curr_seen[i].uidByte)) {
  //          //          Serial.print(F("id0, card value is = "));
  //          //          Serial.print(curr_seen[i].uidByte[0], DEC);
  //          //          Serial.print(F(", "));
  //          //          Serial.print(index_to_value[j], DEC);
  //          //          Serial.println();
  //          cards_value_sum += index_to_value[j];
  //          break;
  //        }
  //      }
  //    }
  //
  //    //    Serial.print(F("cards_value_sum = "));
  //    //    Serial.print(cards_value_sum, DEC);
  //    //    Serial.println();
  //
  //    if (cards_value_sum > 200) {
  //      has_joker = true;
  //      cards_value_sum -= 200;
  //    }
  //    if ((cards_value_sum == 6) || (cards_value_sum == 60)) {
  //      has_group = true;
  //    }
  //  }


  //  if (has_joker) {
  //    digitalWrite(red_led, HIGH);
  //  } else {
  //    digitalWrite(red_led, LOW);
  //  }
  //
  //  if (has_group) {
  //    digitalWrite(blue_led, HIGH);
  //  } else {
  //    digitalWrite(blue_led, LOW);
  //  }

  //  for (int i = 0; i < curr_seen_count; i++) {
  //    for (int j = 0; j < ID_LEN; j++) {
  //      last_seen[i].uidByte[j] = curr_seen[i].uidByte[j];
  //    }
  //  }
  //  last_seen_count = curr_seen_count;

  //  Serial.println();
  //  Serial.println();
  //  Serial.println();

  return;
}


/**
   Helper routine to dump a byte array as hex values to Serial.
*/
void printHex(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}

/**
   Helper routine to dump a byte array as dec values to Serial.
*/
void printDec(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], DEC);
  }
}
