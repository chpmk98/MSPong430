import processing.serial.*;

int lf = 10;    // Linefeed in ASCII, https://ascii.cl/
String myString = null;
Serial myPort;  // The serial port

int paddlex;
int paddley;
int ballx;
int bally;
int hasBall;
int myScore;
int theirScore;

void setup() {
  // List all the available serial ports
  printArray(Serial.list());
  // Open the port you are using at the rate you want:
  myPort = new Serial(this, "/dev/tty.usbmodem14103", 9600*2);
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
      myScore = data[5];
      theirScore = data[6];
      background(51);
      rect(paddlex-100,paddley-10,200,20);
      text("%d : %d", 10, 30);
      if(hasBall == 1){
        circle(ballx,bally,10);
      }
    }
  }
}
