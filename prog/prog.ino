#include <Servo.h>

// ================================================
// CONFIG
// ================================================

#define SERVO_MIN 470
#define SERVO_MAX 2385
#define SERVO_SPEED float(100.0/60.0 * 4.5)

// #define SERVO_LEFT_PIN PIN_PB1
// #define SERVO_RIGHT_PIN PIN_PB2
// #define SERVO_ROTATE_PIN PIN_PB3

// #define BUTTON_TOGGLE A5
// #define RESET_BUTTON A4
// #define TEST_BUTTON A3

#define SERVO_LEFT_PIN PIN_PD6
#define SERVO_RIGHT_PIN PIN_PD5
#define SERVO_ROTATE_PIN PIN_PD3

// ================================================

// ================================================
// GLOBALS
// ================================================
unsigned long currentTime = 0; // 49 dni do przepełnienia
bool continouse_play = true;


// Globalne stany robota
enum OperationMode {  
  STOPPED, // robot stoi i nic nie robi
  PLAYING, // robot gra wybraną piosenkę
  RESET,
  CALIBRATION, // niezaimplementowany
  INTERACTIVE,
};
OperationMode opMode = STOPPED;

// Wewnętrzny stan robota podczas grania (opMode = PLAYING)
enum State {
  PLAY_NOTE,
  RETURN_TO_REST,
  ROTATE,
};

int lastButtonToggleState = LOW;

// struct trzymający info o pozycji nut
struct NotePos {
  char note; // nuta literowa dla łatwiejszego debugowania
  int leftDegree; // rotacja dla lewego ramienia
  int rightDegree; // rotacja dla prawego ramienia

  // Robot decyduje którym ramieniem uderzy nutę na podstawie tego które ramię jest bliżej danej nuty

  NotePos(char note, int leftDegree, int rightDegree) : note(note), leftDegree(leftDegree), rightDegree(rightDegree) {}
};

// predefiniowane pozycje nut w świecie
#define CANNOT_REACH 99999
#define NOTE_COUNT 7
NotePos notes[NOTE_COUNT] = {
  NotePos('c', 62, CANNOT_REACH),
  NotePos('d', 32, CANNOT_REACH),
  NotePos('e', 15, 58),
  NotePos('f', 0, 45),
  NotePos('g', CANNOT_REACH, 32),
  NotePos('a', CANNOT_REACH, 12),
  NotePos('h', CANNOT_REACH, 7),
};

// zwraca pozycję podanej nuty w świecie
struct NotePos getNotePos(char note) {
  if (note >= 'A' && note <= 'Z') note += 32; // from upper to lower case
  for (int i = 0; i < NOTE_COUNT; i++)
    if (note == notes[i].note) return notes[i];
  return notes[0];
}


// struct nuty
struct Note {
  char note; // literowa postać nuty dla łatwiejszego debugowania
  float time; // czas nuty (idk jak to się nazywa muzycznie)

  Note(char note, float time) : note(note), time(time) {};
  Note() : note(' '), time(0) {};

  // zwraca pozycje nuty w świecie
  NotePos getNotePosition() {
    return getNotePos(this->note);
  }
};

// struct trzymający długość i nuty piosenki
struct Song {
  int length;
  Note *notes;
};


// Struct trzymający informacje o aktualnym stanie servo
struct ServoInfo {
  Servo s; // obiekt servo z biblioteki
  String name; // nazwa do debugowania
  int pos; // aktualna pozycja
  int startPos; // pozycja startowa (nie uderzenie)
  int zeroPos; // pozycja neutralna (dla pałeczek nie uderzenie, dla obrotu środek)
  int endPos; // pozycja końcowa (uderzenie)

  ServoInfo() = default;

  ServoInfo(String name, int pin, int startPos, int endPos) {
    int r = this->s.attach(pin, SERVO_MIN, SERVO_MAX);
    Serial.printf("Why?? %d | %d\n", pin, r);
    this->name = name;

    this->pos = 0;
    this->startPos = startPos;
    this->zeroPos = startPos;
    this->endPos = endPos;
  }

  ServoInfo(String name, int pin, int startPos, int zeroPos, int endPos) {
    this->s.attach(pin, SERVO_MIN, SERVO_MAX);
    this->name = name;

    this->pos = 0;
    this->startPos = startPos;
    this->zeroPos = zeroPos;
    this->endPos = endPos;
  }

  // ustawia pozycje servo na dany kąt i oblicza przybliżony czas
  // potrzebny na przemieszczenie się servo
  int move(int degree) {
    int delta = abs(this->pos - degree);
    int waitTime = max(10, int(delta * SERVO_SPEED));

    this->s.write(degree);
    this->pos = degree;
    return min(waitTime, 1000);
  }

  // przesuwa servo na pozycje neutralną
  void reset() { this->move(this->zeroPos); }

  // uderza ramieniem
  int down() { return this->move(this->endPos); }

  // de facto to samo co reset
  int zero() { return this->move(this->zeroPos); }

  // podnosi ramie po uderzeniu
  int up() { return this->move(this->startPos); }
};


ServoInfo *servoLeft;
ServoInfo *servoRight;
ServoInfo *servoRotate;

// ================================================


const Song oktawa = {
  .length = 7,
  .notes = new Note[7]{
    Note('C', 0.25), Note('D', 0.25), Note('E', 0.25), Note('F', 0.25), Note('G', 0.25), Note('A', 0.25), Note('H', 0.25),
  },
};

const Song wlazlKotekNaPlotek = {
  .length = 18,
  .notes = new Note[18]{
      Note('G', 0.25), Note('E', 0.25), Note('E', 0.25), Note('F', 0.25),
      Note('D', 0.25), Note('D', 0.25), Note('C', 0.125), Note('E', 0.125), Note('G', 0.5),
      Note('G', 0.25), Note('E', 0.25), Note('E', 0.25), Note('F', 0.25),
      Note('D', 0.25), Note('D', 0.25), Note('C', 0.125), Note('E', 0.125), Note('C', 0.5),
  },
};

const Song panieJanie = {
  .length = 32,
  .notes = new Note[32]{
      Note('F', 0.25), Note('G', 0.25), Note('A', 0.25), Note('F', 0.25),
      Note('F', 0.25), Note('G', 0.25), Note('A', 0.25), Note('F', 0.25),

      Note('A', 0.25), Note('H', 0.25), Note('C', 0.5), Note('A', 0.25), Note('H', 0.25), Note('C', 0.5),

      Note('C', 0.125), Note('D', 0.125), Note('C', 0.125), Note('H', 0.25), Note('A', 0.25), Note('F', 0.25),
      Note('C', 0.125), Note('D', 0.125), Note('C', 0.125), Note('H', 0.25), Note('A', 0.25), Note('F', 0.25),

      Note('G', 0.25), Note('C', 0.25), Note('F', 0.5), Note('G', 0.25), Note('C', 0.25), Note('F', 0.5),
  },
};

const Song mamChusteczkeHaftowana = {
  .length = 28,
  .notes = new Note[28]{
    Note('E', 0.25), Note('G', 0.25), Note('G', 0.25), Note('G', 0.25),
    Note('E', 0.25), Note('G', 0.25), Note('G', 0.25), Note('G', 0.25), 
    Note('A', 0.25), Note('G', 0.25), Note('F', 0.25), Note('E', 0.25),
    Note('F', 0.5), Note('D', 0.5),

    Note('D', 0.25), Note('F', 0.25), Note('F', 0.25), Note('F', 0.25),
    Note('D', 0.25), Note('F', 0.25), Note('F', 0.25), Note('F', 0.25),
    Note('G', 0.25), Note('F', 0.25), Note('E', 0.25), Note('D', 0.25),
    Note('E', 0.5), Note('C', 0.5),
  },
};

const Song kolkoGraniaste = {
  .length = 33,
  .notes = new Note[33]{
    Note('G', 0.25), Note('E', 0.125), Note('A', 0.125), Note('G', 0.25), Note('E', 0.25),
    Note('G', 0.25), Note('E', 0.125), Note('A', 0.125), Note('G', 0.25), Note('E', 0.25),
    Note('F', 0.125), Note('F', 0.125), Note('E', 0.125), Note('D', 0.125), Note('G', 0.125), Note('G', 0.125), Note('G', 0.125), Note('G', 0.125),
    Note('F', 0.125), Note('F', 0.125), Note('E', 0.125), Note('D', 0.125), Note('G', 0.125), Note('G', 0.125), Note('G', 0.125), Note('G', 0.125),
    Note('G', 0.125), Note('F', 0.125), Note('E', 0.125), Note('D', 0.125), Note('C', 0.5)
  }
};

Song singleNoteSong = {
  .length = 1,
  .notes = new Note[1]{ Note('C', 0.25) },
};

#define SONG_COUNT 5
// first value is for padding
Song songs[] = { oktawa, wlazlKotekNaPlotek, panieJanie, mamChusteczkeHaftowana, kolkoGraniaste };

// indeks aktualnie granej nuty w danej piosence
int noteIdx = 0;
Song currentSong = oktawa;




void setup() {
  Serial.begin(9600);
  // Czeka na inicializacje połączenia po UART
  while (!Serial) {}

  // przygotowanie pinów przycisków
  // pinMode(BUTTON_TOGGLE, INPUT);
  // pinMode(RESET_BUTTON, INPUT);

  // inicializacja servo
  servoLeft = new ServoInfo("left", SERVO_LEFT_PIN, 18, 1);
  servoRight = new ServoInfo("right", SERVO_RIGHT_PIN, 0, 17);
  servoRotate = new ServoInfo("rotate", SERVO_ROTATE_PIN, 0, 30, 60);



  Serial.println("Start!!!");
  opMode = STOPPED;

  // Ustawienie robota na neutralnej pozycji
  servoLeft->reset();
  servoRight->reset();
  servoRotate->reset();

  delay(3000);
}


// coś tam z nutami związane
#define BASE_TIME 1200
void handlePlaying() {
  static State state = ROTATE;
  static ServoInfo *arm = servoLeft;
  static unsigned long deltaTime = 0;
  static unsigned long nextNoteTime = 0;
  static Note currentNote = Note();

  // Jakies servo jeszcze wykonuje ruch
  if (currentTime <= deltaTime) return;


  if (state == ROTATE) {
    currentNote = currentSong.notes[noteIdx];
    NotePos pos = currentNote.getNotePosition();

    arm = servoRight;
    int degree = pos.rightDegree;
    if (abs(servoRotate->pos - pos.leftDegree) <= abs(servoRotate->pos - pos.rightDegree)) {
      arm = servoLeft;
      degree = pos.leftDegree;
    }

    deltaTime = currentTime + servoRotate->move(degree);
    Serial.printf("Moved to note: (%d) %c\n", noteIdx, currentNote.note);
    state = PLAY_NOTE;
    return;
  }

  if (state == PLAY_NOTE) {
    if (currentTime < nextNoteTime) return;

    nextNoteTime = currentTime + currentNote.time * BASE_TIME;


    deltaTime = currentTime + arm->down();
    state = RETURN_TO_REST;
    Serial.printf("Played note: (%d) %c\n", noteIdx, currentNote.note);
    return;
  }

  if (state == RETURN_TO_REST) {
    if (!continouse_play && noteIdx + 1 == currentSong.length) { // koniec piosenki
      opMode = STOPPED;
    }
    noteIdx = (noteIdx + 1) % currentSong.length;

    deltaTime = currentTime + arm->up();
    state = ROTATE;
    return;
  }
}


void handleCommunication() {
  static bool repeatMode = continouse_play;
  static Song lastSong = currentSong;

  if (!Serial.available()) return;
  String msg = Serial.readStringUntil('\n');

  if (msg[0] == 'S') {
    int number = atoi(&msg[1]);
    if (number < 0 || number >= SONG_COUNT) Serial.printf("Song index out of range <0; %d>\n", SONG_COUNT-1);
    else Serial.printf("Selected song no. %d\n", number);
    return;
  }

  if (msg[0] == '#') { // sama nuta
    char rawNote = msg[1];

    singleNoteSong.notes[0].note = getNotePos(rawNote).note;
    Serial.printf("Play note: %c\n", rawNote);

    noteIdx = 0;
    opMode = PLAYING;
    return;
  }

  if (msg == String("!stop")) {
    Serial.printf("Stop playing\n");
    opMode = STOPPED;
    return;
  }

  if (msg == String("!play")) {
    Serial.printf("Start playing\n");
    opMode = PLAYING;
    return;
  }

  if (msg == String("!repeat on")) {
    Serial.printf("Turn on repeatative play\n");
    continouse_play = true;
    return;
  }

  if (msg == String("!repeat off")) {
    Serial.printf("Turn off repeatative play\n");
    continouse_play = false;
    return;
  }

  if (msg == String("!interactive on")) {
    lastSong = currentSong;
    repeatMode = continouse_play;
    continouse_play = false;

    currentSong = singleNoteSong;
    Serial.printf("Turn on interactive mode\n");
    return;
  }

  if (msg == String("!interactive off")) {
    continouse_play = repeatMode;
    currentSong = lastSong; 

    noteIdx = 0;
    Serial.printf("Turn off interactive mode\n");
    opMode = STOPPED;
  }

  if (msg == String("!reset")) {
    Serial.printf("Reset robot to default positions and stop playing\n");
    opMode = RESET;
    return;
  }

  Serial.printf("Received from input '%s'\n", msg.c_str());
}

void printMetaInfo() {
  static int lastLeft = 0, lastRight = 0, lastRotate = 0;

  if (lastLeft == servoLeft->pos && lastRight == servoRight->pos && lastRotate == servoRotate->pos) return;
  lastLeft = servoLeft->pos;
  lastRight = servoRight->pos;
  lastRotate = servoRotate->pos;

  Serial.printf("$ %d %d %d\n", servoLeft->pos, servoRight->pos, servoRotate->pos);
}

void loop() {
  currentTime = millis();
  handleCommunication();
  // handleButtons();

  switch (opMode) {
  case STOPPED: break;
  case PLAYING:
    handlePlaying();
    break;
  case CALIBRATION: break;
  case RESET:
    servoLeft->reset();
    servoRight->reset();
    servoRotate->reset();
    noteIdx = 0;
    opMode = STOPPED;
    break;
  }

  printMetaInfo();
}