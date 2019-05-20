#include <Adafruit_MCP23008.h>
#include <Adafruit_NeoPixel.h>

Adafruit_MCP23008 mcp1;
Adafruit_MCP23008 mcp2;
Adafruit_MCP23008 mcp3;

#define NUMBER_ROWS_AND_COLUMNS 12
#define MATRIX_SIZE 16

#define GAME_OVER_MODE 1
#define PLAYING_MODE 0

bool buttonRow[NUMBER_ROWS_AND_COLUMNS];
bool buttonCol[NUMBER_ROWS_AND_COLUMNS];

int lastRowPressed = -1;
int lastColPressed = -1;

int gameMode = PLAYING_MODE;

#define UP_DIRECTION 0
#define DOWN_DIRECTION 1
#define LEFT_DIRECTION 2
#define RIGHT_DIRECTION 3

#define addr1 0
#define addr2 1
#define addr3 2

#define RED_BUTTON 0
#define ORANGE_BUTTON 1
#define YELLOW_BUTTON 2
#define GREEN_BUTTON 3
#define BLUE_BUTTON 4
#define PURPLE_BUTTON 5
#define WHITE_BUTTON 6
#define GRAY_BUTTON 7

#define FIRE_BUTTON_PIN 9
#define MATRIX_PIN 8

uint32_t playerBoard[MATRIX_SIZE][MATRIX_SIZE];
Adafruit_NeoPixel matrix = Adafruit_NeoPixel(MATRIX_SIZE * MATRIX_SIZE, MATRIX_PIN, NEO_GRB + NEO_KHZ800);

uint32_t RED = matrix.Color(255, 0, 0);
uint32_t ORANGE = matrix.Color(255, 165, 0);
uint32_t YELLOW = matrix.Color(255, 255, 0);
uint32_t GREEN = matrix.Color(0, 255, 0);
uint32_t BLUE = matrix.Color(0, 0, 255);
uint32_t WHITE = matrix.Color(255, 255, 255);
uint32_t CLEAR = matrix.Color(0, 0, 0);

unsigned long timeOfLastBlinkToggle = 0;
bool overlaysOn = true;
unsigned long timeSinceGameOverStarted = 0;

struct cell {
  int row;
  int column;
};

cell aircraftCarrier[5];
cell battleship[4];
cell cruiser1[3];
cell cruiser2[3];
cell defender[2];

cell aircraftCarrierRepresentation[5];
cell battleshipRepresentation[4];
cell cruiser1Representation[3];
cell cruiser2Representation[3];
cell defenderRepresentation[2];

cell numberTurnsRepresentation[36];

int aircraftCarrierHits = 0;
int battleshipHits = 0;
int cruiser1Hits = 0;
int cruiser2Hits = 0;
int defenderHits = 0;

int numberTurns = 36;

bool displayedAircraftCarrierSunkMessage = false;
bool displayedBattleshipSunkMessage = false;
bool displayedCruiser1SunkMessage = false;
bool displayedCruiser2SunkMessage = false;
bool displayedDefenderSunkMessage = false;

void setup() {
  Serial.begin(9600);
  pinMode(FIRE_BUTTON_PIN, INPUT_PULLUP);   
  
  mcp1.begin(addr1);
  mcp2.begin(addr2);
  mcp3.begin(addr3);

  for(int i = 0; i < 8; i++) {
    mcp1.pinMode(i, INPUT);
    mcp1.pullUp(i, HIGH);
    mcp2.pinMode(i, INPUT);
    mcp2.pullUp(i, HIGH);
    mcp3.pinMode(i, INPUT);
    mcp3.pullUp(i, HIGH);
  }

  matrix.begin();
  matrix.setBrightness(32);
  matrix.show();

  randomSeed(analogRead(0));

  defineGameParameters();
  startGame();
}

void startGame() {
  resetRowAndColumnSelectionVariables();
  resetPlayerBoard();  
  Serial.println("Let's play Battleship!");
}

void defineGameParameters() {
  for(int i = 0; i < 5; i++) {
    aircraftCarrierRepresentation[i].row = 13;
    aircraftCarrierRepresentation[i].column = i + 1;
  }
  for(int i = 0; i < 4; i++) {
    battleshipRepresentation[i].row = 13;
    battleshipRepresentation[i].column = i + 7;
  }  
  for(int i = 0; i < 3; i++) {
    cruiser1Representation[i].row = 13;
    cruiser1Representation[i].column = i + 12;
    cruiser2Representation[i].row = 15;
    cruiser2Representation[i].column = i + 5;
  }
  for(int i = 0; i < 2; i++) {
    defenderRepresentation[i].row = 15;
    defenderRepresentation[i].column = i + 10;
  }
  for(int i = 0; i <36; i++) {    
    if(i < 12) {
      numberTurnsRepresentation[i].row = i;
      numberTurnsRepresentation[i].column = 13;
    } else if(i < 24) {
      numberTurnsRepresentation[i].row = i - 12;
      numberTurnsRepresentation[i].column = 14;
    } else {
      numberTurnsRepresentation[i].row = i - 24;
      numberTurnsRepresentation[i].column = 15;
    }
  }
}

// the loop function runs over and over again forever
void loop() {
  readInputs();    
  setLastButtonsPressed();  
  if(gameMode == GAME_OVER_MODE) {
    long currentTime = millis();
    if(currentTime - timeSinceGameOverStarted > 10000) {
      startGame();
    }
  } else {
    if(canFire()) {
      if(digitalRead(FIRE_BUTTON_PIN) == LOW) {
        while(digitalRead(FIRE_BUTTON_PIN) == LOW) {
          // wait until the user releases the fire button
        }
        bool isHit = fire();
        if(!isHit) {
          updateNumberTurns();        
        }
        resetRowAndColumnSelectionVariables();      
      }    
    }    
    if(isGameOver()) {
      gameMode = GAME_OVER_MODE;
      timeSinceGameOverStarted = millis();
      if(isWin()) {
        Serial.println("Congratulations, you sunk all my ships!");
      } else {
        Serial.println("Sorry, you lose.  You didn't sink all my ships in time.");
      }    
    }
  }
  updateMatrix();
}
void updateNumberTurns() {
  cell currentNumberTurnRepresentation = numberTurnsRepresentation[36 - numberTurns];
  playerBoard[currentNumberTurnRepresentation.row][currentNumberTurnRepresentation.column] = CLEAR;
  numberTurns = numberTurns - 1;
  Serial.print("You have ");
  Serial.print(numberTurns);
  Serial.println(" misses remaining");    
  Serial.println("");
}
bool isWin() {
  return aircraftCarrierIsSunk() && battleshipIsSunk() && cruiser1IsSunk() && cruiser2IsSunk() && defenderIsSunk();
}

bool isGameOver() {
  return numberTurns == 0 || isWin();  
}

bool rowIsSelected() {
  return lastRowPressed != -1;
}
bool colIsSelected() {
  return lastColPressed != -1;
}

bool canFire() {
  return rowIsSelected() && colIsSelected() && !hasBeenPlayed(lastRowPressed, lastColPressed);
}
bool hasBeenPlayed(int row, int col) {
  return playerBoard[row][col] != CLEAR;
}

void setLastRowButtonPressed() {
  for(int i = 0; i < NUMBER_ROWS_AND_COLUMNS; i++) {
    if(buttonRow[i]) {
      lastRowPressed = i;
    }
  }
}
void setLastColButtonPressed() {
  for(int i = 0; i < NUMBER_ROWS_AND_COLUMNS; i++) {
    if(buttonCol[i]) {
      lastColPressed = i;
    }
  }
}

void setLastButtonsPressed() {
  setLastRowButtonPressed();
  setLastColButtonPressed();
}

void resetRowAndColumnSelectionVariables() {
  for(int i = 0; i < NUMBER_ROWS_AND_COLUMNS; i++) {
    buttonRow[i] = false;
    buttonCol[i] = false;
  }
  lastRowPressed = -1;
  lastColPressed = -1;
  overlaysOn = true;
  timeOfLastBlinkToggle = 0;
}

void fillNumberTurns() {
  for(int i = 0; i < 36; i++) {
    playerBoard[numberTurnsRepresentation[i].row][numberTurnsRepresentation[i].column] = BLUE;
  }
}

void resetPlayerBoard() {
  for(int i = 0; i < MATRIX_SIZE; i++) {
    for(int j = 0; j < MATRIX_SIZE; j++) {
      playerBoard[i][j] = CLEAR;
    }
  }
  numberTurns = 36;
  fillNumberTurns();
  gameMode = PLAYING_MODE;
  
  aircraftCarrierHits = 0;
  battleshipHits = 0;
  cruiser1Hits = 0;
  cruiser2Hits = 0;
  defenderHits = 0;
  
  displayedAircraftCarrierSunkMessage = false;
  displayedBattleshipSunkMessage = false;
  displayedCruiser1SunkMessage = false;
  displayedCruiser2SunkMessage = false;
  displayedDefenderSunkMessage = false;

  resetShips();
  
  placeBoardMarkers();
  placeAircraftCarrier();
  placeBattleship();
  placeCruiser1();
  placeCruiser2();
  placeDefender();
  placeShipRepresentations();      

  updateMatrix();
}
void placeBoardMarkers() {
  for(int i = 0; i < MATRIX_SIZE; i++) {
    playerBoard[12][i] = ORANGE;    
  }
  for(int i = 0; i < NUMBER_ROWS_AND_COLUMNS; i++) {
    playerBoard[i][12] = ORANGE;
  }
}
void placeShipRepresentations() {  
  for(int i = 0; i < 5; i++) {
    playerBoard[aircraftCarrierRepresentation[i].row][aircraftCarrierRepresentation[i].column] = GREEN;    
  }
  for(int i = 0; i < 4; i++) {
    playerBoard[battleshipRepresentation[i].row][battleshipRepresentation[i].column] = GREEN;
  }
  for(int i = 0; i < 3; i++) {
    playerBoard[cruiser1Representation[i].row][cruiser1Representation[i].column] = GREEN;
    playerBoard[cruiser2Representation[i].row][cruiser2Representation[i].column] = GREEN;
  }
  for(int i = 0; i < 2; i++) {
    playerBoard[defenderRepresentation[i].row][defenderRepresentation[i].column] = GREEN;
  }   
}

void resetShips() {
  for(int i = 0; i < 5; i++) {
    aircraftCarrier[i].row = -1;
    aircraftCarrier[i].column = -1;    
  }
  for(int i = 0; i < 4; i++) {
    battleship[i].row = -1;
    battleship[i].column = -1;
  }
  for(int i = 0; i < 3; i++) {
    cruiser1[i].row = -1;
    cruiser1[i].column = -1;
    cruiser2[i].row = -1;
    cruiser2[i].column = -1;
  }
  for(int i = 0; i < 2; i++) {
    defender[i].row = -1;
    defender[i].column = -1;
  }
}

int getRandomDirection() {
  int randomNumber = random(0, 4);
  switch(randomNumber) {
    case 0:
      return UP_DIRECTION;
    case 1:
      return DOWN_DIRECTION;
    case 2:
      return LEFT_DIRECTION;
    case 3:
      return RIGHT_DIRECTION;
    default:
      return UP_DIRECTION;
  }
}

cell getRandomCell() {
  cell randomCell;
  randomCell.row = random(0, 12);
  randomCell.column = random(0, 12);
  return randomCell;
}

void generateShipPlacement(cell startCellOfShip, int directionOfShip, int shipSize, cell shipPlacement[]) {
  for(int i = 0; i < shipSize; i++) {
    switch(directionOfShip) {
      case UP_DIRECTION:        
        shipPlacement[i].row = startCellOfShip.row - i;
        shipPlacement[i].column = startCellOfShip.column;
      break;
      case DOWN_DIRECTION:        
        shipPlacement[i].row = startCellOfShip.row + i;
        shipPlacement[i].column = startCellOfShip.column;
        break;
      case LEFT_DIRECTION:
        shipPlacement[i].row = startCellOfShip.row;
        shipPlacement[i].column = startCellOfShip.column - i;
        break;
      case RIGHT_DIRECTION:
        shipPlacement[i].row = startCellOfShip.row;
        shipPlacement[i].column = startCellOfShip.column + i;
        break;
    }
  }
}

bool isValidShipPlacement(cell startCellOfShip, int directionOfShip, int shipSize) {  
  // check first if the placement can happen on the grid based on the size of the ship
  switch(directionOfShip) {
    case UP_DIRECTION:
      if(startCellOfShip.row - shipSize < 0) {
        return false;
      }
      break;
    case DOWN_DIRECTION:
      if(startCellOfShip.row + shipSize > 11) {
        return false;
      }
      break;
    case LEFT_DIRECTION:
      if(startCellOfShip.column - shipSize < 0) {
        return false;
      }
      break;
    case RIGHT_DIRECTION:
      if(startCellOfShip.column + shipSize > 11) {
        return false;
      }
      break;
    default:
      return false;
  }
  cell candidateShipPlacement[shipSize];
  generateShipPlacement(startCellOfShip, directionOfShip, shipSize, candidateShipPlacement);

  // next check if the ship interferes with any of the other ships
  if(aircraftCarrier[0].row != -1) {    
    if(placementsOverlap(aircraftCarrier, 5, candidateShipPlacement, shipSize)) {
      return false;
    }
  }
  if(battleship[0].row != -1) {
    if(placementsOverlap(battleship, 4, candidateShipPlacement, shipSize)) {
      return false;
    }
  }
  if(cruiser1[0].row != -1) {
    if(placementsOverlap(cruiser1, 3, candidateShipPlacement, shipSize)) {
      return false;
    }
  }
  if(cruiser2[0].row != -1) {
    if(placementsOverlap(cruiser2, 3, candidateShipPlacement, shipSize)) {
      return false;
    }
  }

  // if all these tests pass, then the ship placement is valid
  return true;
}

bool placementsOverlap(cell ship1[], int sizeOfShip1, cell ship2[], int sizeOfShip2) {
  for(int i = 0; i < sizeOfShip1; i++) {
    for(int j = 0; j < sizeOfShip2; j++) {
      if(ship1[i].row == ship2[j].row && ship1[i].column == ship2[j].column) {
        return true;
      }
    }
  }
  return false;
}

void placeShip(int shipSize, cell ship[]) {
  cell startCellOfShip;
  int directionOfShip;
  do {
    startCellOfShip = getRandomCell();
    directionOfShip = getRandomDirection();
  } while(!isValidShipPlacement(startCellOfShip, directionOfShip, shipSize));
  
  generateShipPlacement(startCellOfShip, directionOfShip, shipSize, ship);
}


void placeAircraftCarrier() {      
  placeShip(5, aircraftCarrier);    
}
void placeBattleship() {  
  placeShip(4, battleship);  
}
void placeCruiser1() {  
  placeShip(3, cruiser1);  
}
void placeCruiser2() {  
  placeShip(3, cruiser2);
}
void placeDefender() {
  placeShip(2, defender);
}

void readInputs() {
  buttonRow[0] = mcp2.digitalRead(GREEN_BUTTON) == LOW? true: false;
  buttonRow[1] = mcp2.digitalRead(YELLOW_BUTTON) == LOW? true: false;
  buttonRow[2] = mcp2.digitalRead(ORANGE_BUTTON) == LOW? true: false;
  buttonRow[3] = mcp2.digitalRead(RED_BUTTON) == LOW? true: false;
  buttonRow[4] = mcp3.digitalRead(GRAY_BUTTON) == LOW? true: false;
  buttonRow[5] = mcp3.digitalRead(WHITE_BUTTON) == LOW? true: false;
  buttonRow[6] = mcp3.digitalRead(PURPLE_BUTTON) == LOW? true: false;
  buttonRow[7] = mcp3.digitalRead(BLUE_BUTTON) == LOW? true: false;
  buttonRow[8] = mcp3.digitalRead(GREEN_BUTTON) == LOW? true: false;
  buttonRow[9] = mcp3.digitalRead(YELLOW_BUTTON) == LOW? true: false;
  buttonRow[10] = mcp3.digitalRead(ORANGE_BUTTON) == LOW? true: false;
  buttonRow[11] = mcp3.digitalRead(RED_BUTTON) == LOW? true: false;

  buttonCol[0] = mcp2.digitalRead(BLUE_BUTTON) == LOW? true: false;
  buttonCol[1] = mcp2.digitalRead(PURPLE_BUTTON) == LOW? true: false;
  buttonCol[2] = mcp2.digitalRead(WHITE_BUTTON) == LOW? true: false;
  buttonCol[3] = mcp2.digitalRead(GRAY_BUTTON) == LOW? true: false;
  buttonCol[4] = mcp1.digitalRead(RED_BUTTON) == LOW? true: false;
  buttonCol[5] = mcp1.digitalRead(ORANGE_BUTTON) == LOW? true: false;
  buttonCol[6] = mcp1.digitalRead(YELLOW_BUTTON) == LOW? true: false;
  buttonCol[7] = mcp1.digitalRead(GREEN_BUTTON) == LOW? true: false;
  buttonCol[8] = mcp1.digitalRead(BLUE_BUTTON) == LOW? true: false;
  buttonCol[9] = mcp1.digitalRead(PURPLE_BUTTON) == LOW? true: false;
  buttonCol[10] = mcp1.digitalRead(WHITE_BUTTON) == LOW? true: false;
  buttonCol[11] = mcp1.digitalRead(GRAY_BUTTON) == LOW? true: false;
}

bool fire() {
  Serial.println("FIRE!!!");  
  bool isHit = checkIfIsHit(lastRowPressed, lastColPressed);  
  if(isHit) {
    Serial.println("...HIT!");
    playerBoard[lastRowPressed][lastColPressed] = RED;
    updateShipHitCounts(lastRowPressed, lastColPressed);
  } else {
    Serial.println("...miss");
    playerBoard[lastRowPressed][lastColPressed] = WHITE;
  }
  updateShipRepresentations();  
  return isHit;
}

bool aircraftCarrierIsSunk() {
  return aircraftCarrierHits >= 5;
}
bool battleshipIsSunk() {
  return battleshipHits >= 4;
}
bool cruiser1IsSunk() {
  return cruiser1Hits >= 3;
}
bool cruiser2IsSunk() {
  return cruiser2Hits >= 3;
}
bool defenderIsSunk() {
  return defenderHits >= 2;
}

void updateShipRepresentations() {
  if(aircraftCarrierIsSunk()) {
    for(int i = 0; i < 5; i++) {
      playerBoard[aircraftCarrierRepresentation[i].row][aircraftCarrierRepresentation[i].column] = RED;
    }
    if(!displayedAircraftCarrierSunkMessage) {
      Serial.println("You sunk my aircraft carrier!");
      displayedAircraftCarrierSunkMessage = true;
    }
  }
  if(battleshipIsSunk()) {
    for(int i = 0; i < 4; i++) {
      playerBoard[battleshipRepresentation[i].row][battleshipRepresentation[i].column] = RED;
    }
    if(!displayedBattleshipSunkMessage) {
      Serial.println("You sunk my battleship!");
      displayedBattleshipSunkMessage = true;
    }
  }
  if(cruiser1IsSunk()) {
    for(int i = 0; i < 3; i++) {
      playerBoard[cruiser1Representation[i].row][cruiser1Representation[i].column] = RED;
    }
    if(!displayedCruiser1SunkMessage) {
      Serial.println("You sunk one of my cruisers!");  
      displayedCruiser1SunkMessage = true;
    }
    
  }
  if(cruiser2IsSunk()) {
    for(int i = 0; i < 3; i++) {
      playerBoard[cruiser2Representation[i].row][cruiser2Representation[i].column] = RED;
    }
    if(!displayedCruiser2SunkMessage) {
      Serial.println("You sunk one of my cruisers!");  
      displayedCruiser2SunkMessage = true;
    }
  }
  if(defenderIsSunk()) {
    for(int i = 0; i < 2; i++) {
      playerBoard[defenderRepresentation[i].row][defenderRepresentation[i].column] = RED;
    }
    if(!displayedDefenderSunkMessage) {
      Serial.println("You sunk my defender!");
      displayedDefenderSunkMessage = true;
    }
  }  
}

void updateShipHitCounts(int row, int col) {
  if(isAircraftCarrierHit(row, col)) {
    aircraftCarrierHits++;
  }
  if(isBattleshipHit(row, col)) {
    battleshipHits++;
  }
  if(isCruiser1Hit(row, col)) {
    cruiser1Hits++;
  }
  if(isCruiser2Hit(row, col)) {
    cruiser2Hits++;
  }
  if(isDefenderHit(row, col)) {
    defenderHits++;
  }
}

bool isAircraftCarrierHit(int row, int col) {
  for(int i = 0; i < 5; i++) {
    if(aircraftCarrier[i].row == row && aircraftCarrier[i].column == col) {      
      return true;
    }
  }
  return false;
}
bool isBattleshipHit(int row, int col) {
  for(int i = 0; i < 4; i++) {
    if(battleship[i].row == row && battleship[i].column == col) {
      return true;
    }
  }
  return false;
}
bool isCruiser1Hit(int row, int col) {
  for(int i = 0; i < 3; i++) {
    if(cruiser1[i].row == row && cruiser1[i].column == col) {
      return true;
    }
  }
  return false;
}
bool isCruiser2Hit(int row, int col) {
  for(int i = 0; i < 3; i++) {
    if(cruiser2[i].row == row && cruiser2[i].column == col) {
      return true;
    }
  }
  return false;
}
bool isDefenderHit(int row, int col) {
  for(int i = 0; i < 2; i++) {
    if(defender[i].row == row && defender[i].column == col) {
      return true;
    }
  }
  return false;
}

bool checkIfIsHit(int row, int col) {  
  return isAircraftCarrierHit(row, col) || isBattleshipHit(row, col) || isCruiser1Hit(row, col) || isCruiser2Hit(row, col) || isDefenderHit(row, col);  
}

bool displayOverlays() {
  if(timeOfLastBlinkToggle == 0) {
    timeOfLastBlinkToggle = millis();    
  }
  long currentTime = millis();
  if(currentTime - timeOfLastBlinkToggle > 200) {
    if(overlaysOn) {
      overlaysOn = false;
    } else {
      overlaysOn = true;
    }
    timeOfLastBlinkToggle = millis();
  }
  return overlaysOn;
}

void updateMatrix() {
  matrix.clear();
  for(int i = 0; i < MATRIX_SIZE; i++) {
    for(int j = 0; j < MATRIX_SIZE; j++) {
      matrix.setPixelColor(convertToMatrixPoint(i,j), playerBoard[i][j]);
    }
  }
  if(displayOverlays()) {
    overlayCrossHairs();
    if(gameMode == GAME_OVER_MODE) {
      overlayShips();
    }
  }
  
  matrix.show();
}
void overlayCrossHairs() {
  if(rowIsSelected()) {
    for(int i = 0; i < NUMBER_ROWS_AND_COLUMNS; i++) {
      matrix.setPixelColor(convertToMatrixPoint(lastRowPressed, i), YELLOW);
    }
  }
  if(colIsSelected()) {
    for(int i = 0; i < NUMBER_ROWS_AND_COLUMNS; i++) {
      matrix.setPixelColor(convertToMatrixPoint(i, lastColPressed), YELLOW);
    }
  }
  if(rowIsSelected() && colIsSelected()) {
    matrix.setPixelColor(convertToMatrixPoint(lastRowPressed, lastColPressed), RED);
  }
}
void overlayShips() {
  for(int i = 0; i < 5; i++) {
    matrix.setPixelColor(convertToMatrixPoint(aircraftCarrier[i].row, aircraftCarrier[i].column), BLUE);
  }
  for(int i = 0; i < 4; i++) {
    matrix.setPixelColor(convertToMatrixPoint(battleship[i].row, battleship[i].column), BLUE);
  }
  for(int i = 0; i < 3; i++) {
    matrix.setPixelColor(convertToMatrixPoint(cruiser1[i].row, cruiser1[i].column), BLUE);
    matrix.setPixelColor(convertToMatrixPoint(cruiser2[i].row, cruiser2[i].column), BLUE);
  }
  for(int i = 0; i < 2; i++) {
    matrix.setPixelColor(convertToMatrixPoint(defender[i].row, defender[i].column), BLUE);
  }
}

int convertToMatrixPoint(int i, int j) {
  if(i % 2 == 0) {
    return ((15 - i) * 16) + j;
  } else {
    return ((15 - i) * 16) + (15 - j);    
  }
}

