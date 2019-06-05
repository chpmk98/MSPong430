import processing.serial.*;

int lf = 10;    // Linefeed in ASCII, https://ascii.cl/
String myString = null;
Serial myPort;  // The serial port

int paddlex;
int paddley;
int ballx;
int bally;
int hasBall;

void setup() {
  // List all the available serial ports
  printArray(Serial.list());
  // Open the port you are using at the rate you want:
  myPort = new Serial(this, "COM12", 9600*2);
  myPort.clear();
  // Throw out the first reading, in case we started reading 
  // in the middle of a string from the sender.
  myString = myPort.readStringUntil(lf);
  myString = null;
  size(500,500);
  textSize(50);
}

void draw() {
  while (myPort.available() > 0) {
    myString = myPort.readStringUntil(lf);
    if (myString != null) {
      println(myString);
      int [] data = int(split(myString,' '));
      if(data.length < 5){
        continue;
      }
      paddlex = data[0];
      paddley = data[1];
      ballx = data[2];
      bally = data[3];
      hasBall = data[4];
      background(51);
      rect(paddlex-25,paddley-10,50,20);
      circle(ballx,bally,10);
      
    }
  }
}
