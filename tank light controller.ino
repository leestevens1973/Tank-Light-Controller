

/* Tank Light Controller
 
 Webserver to monitor temperature and control and fade led lights
 By Lee stevens
 
 config variables at top of sketch for changing setting.
 use pin 3 for storm light pins 5,6,9,10 for others,
 
 much of the code is taken from the redwall sensor web server, many thanks to Jon Leach for that.
 
 v 1.0.0
 initial programing of web server led controls
 HTML tables done with changing status colour and buttons, intensity level and on/off and submit buttons , number of ligth tables drawn is depending on setting of lightCount 
 
 NTP time server
 
 v1.0.1
 added fade function & intensity array - Lights fading in and out rather than switching on and off
 save setting of each lightID so web page shows current light state
 added temperature to web page using one wire and dallas temperature library
 
 v1.0.2
 added storm function and variable for stom duration
 took out web config page due to space limitations - show current settings and system info
 Temperature backgropund changes to red if overtemp, blue if below temp and green if normal
 added Global Buttons
 all controls allow lights to fade in and out now rather than turn off
 Lights now fade in slowly at given alarm time altering fadeAlarmx changes alarmfadespeed based 
 on the value of fade and multiplied by fadeAlarmx for a slower fade in and out at alarm trigger times. 
 ie fade(20)*fadeAlarmx(5) =  delay(fade*fadeAlarmx); = delay(100);
 
 Tidied some code and comments added 
 
 
 Todo but not enough space !!!!!
 possibly add a function to allow lights to randomly fade in and out like cloud effect
 RGB mood lamp style effect
 simple form for basic config storm length and on and off times 
 storm as an alarm ?
 Save to EEprom
 RTC support?
 could make an option for multiple probes and average temp
 Tidy up HTML & sketch
 
 */

#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <Time.h>
#include <TimeAlarms.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// device variables edit to suit

int onTimeHr=18; // hour on
int onTimeMin=12; // min on
int offTimeHr=22; // hour off
int offTimeMin=01; // min off
int lightCount=3; // how many lights - max 5 although pin 9 will be stormlight!!!
int fade=20; // alter fade speed
int fadeAlarmx =5; // multiply fade + to slow alarm fade
int stormDuration=5; // increase for longer stormtime
int alarmOvertemp=22; // alarm over temp setting
int alarmBelowtemp=18; // alarm below temp setting

// IP Settings
byte mac[] = { 
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192,168,0,177);
byte gateway[] = { 
  192,168,0,1 };

// Initialize the Ethernet server library with port number
EthernetServer server(80);

// program variables
int Storm=0; // to set storm status
int StormPin = 6; // which pin storm light is connected- plugging into one of the unused pins light wud only flash at storm time
int lightID=1; 
int lightPin=lightID+2; // use this to convert pin to correct array
// arrays for each light bank 
int Status[5]={
  1,1,1,1,1};
int Intensity[5]={
  1,1,1};
int lastIntensity[5]={
  0,0,0,0,0};
int lightPinPWM[5]={
  3,5,6,9,10};  // have to use these pin for PWM to work
// to show alarm settings on page
int onTime=onTimeHr*100+onTimeMin;
int offTime=offTimeHr*100+offTimeMin;

//string for fetching data from address
String inString = String(); 

// NTP server stuff
unsigned int localPort = 8888;      // local port to listen for UDP packets
IPAddress timeServer(216,171,112,36); // address of NTP server
const int NTP_PACKET_SIZE= 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets 
// A UDP instance to let us send and receive packets over UDP 
EthernetUDP Udp;

time_t t;

// set up one wire on pin 2
#define ONE_WIRE_BUS 2
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);


void setup() {
  // Open serial communications and wait for port to open:
  //  Serial.begin(9600);

  // start the Ethernet connection and the server:
  Ethernet.begin(mac, ip , gateway);
  //  server.begin();
  //  Serial.print("server is at ");
  //  Serial.println(Ethernet.localIP());
  Udp.begin(localPort);
  setSyncProvider(getNtpTime);
  setSyncInterval(3600);

  // initialise pins  
  int x=0;
  while (lightID<=lightCount)  
  {
    pinMode(lightPinPWM[x],OUTPUT); 
    lightID++;
    x++;  
  }
  pinMode(9,OUTPUT);


  Alarm.alarmRepeat(onTimeHr,onTimeMin,0, MorningAlarm);  // 8:30am every day
  Alarm.alarmRepeat(offTimeHr,offTimeMin,0,EveningAlarm);  // 5:45pm every day 

}

void loop() {
  Alarm.delay(1000);
  time_t t = now();
  int currentTime = ((hour(t) + adjustBSTEurope()) * 100) + minute(t);

  // Loop for each lightID before web
  while (lightID<=lightCount)
  {
    // set light pin 
    lightPin=lightPinPWM[lightID-1];
    // check intenesity and if 0 turn off and vice versa
    if (Status[lightID-1]==0)
    {
      Intensity[lightID-1]=0;
    }
    if (Intensity[lightID-1]==0); 
    {
      (Status[lightID-1]==0);
    }

    // check status and set pin to fade up or down
    if (Status[lightID-1]==1)
    {       
      // Fade up 
      if (Intensity[lightID-1]>=lastIntensity[lightID-1])
      {
        fadein();
      }
      // else fade down to new value
      if (Intensity[lightID-1] < lastIntensity[lightID-1])
      {
        fadeout(); 
      }     
      if (Intensity[lightID-1]==0)
      {
        Status[lightID-1]=0;
      } 
      // after fade so to set intensity 
      lastIntensity[lightID-1]=Intensity[lightID-1]; 
    } 
    if (Status[lightID-1]==0) // switched off
    {  
      lastIntensity[lightID-1]=Intensity[lightID-1];
      Intensity[lightID-1]=0;
      fadeout();      
    }
    lastIntensity[lightID-1]=Intensity[lightID-1];
    lightPin++;
    lightID++;

  }
  EthernetClient client = server.available();
  if (client) {
    //   inString="";
    //    Serial.println("new client");
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {

        char c = client.read();

        //read data from header
        if (inString.length() < 65) {
          inString += c;          
        }       

        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {

          // web based code here e.g readstring from header and process data

          // set up variables for parsing header
          String settingString;
          String lightParsestring;
          int stringpos;
          int indexPos;
          int endPos;
          // reset parse strings
          settingString="";
          stringpos=0;
          lightParsestring="";

          // light settings
          indexPos= (inString.indexOf("?")+1);
          endPos= (inString.indexOf("=submit"));       
          lightParsestring= (inString.substring(indexPos,endPos));

          // Light ID
          stringpos= (lightParsestring.indexOf("config")+6);      
          settingString=(lightParsestring.substring(stringpos));
          lightID= settingString.toInt();

          // intenstity setting
          stringpos=(lightParsestring.indexOf("intensity")+10);
          settingString=(lightParsestring.substring(stringpos));
          Intensity[lightID-1]=settingString.toInt();

          // turn on or off if button pressed 
          if (lightParsestring.indexOf("Turn+Of")>1)
          {
            // Status[lightID-1]=0;
            lightPin=lightPinPWM[lightID-1];
            for (int i=Intensity[lightID-1]*42;i>0-1;i--)
            {
              analogWrite(lightPin,i);
              delay(fade);
              Status[lightID-1]=0;
            }
          }
          if (lightParsestring.indexOf("Turn+On")>1)
          {            
            Status[lightID-1]=1;
          }
          if (lightParsestring.indexOf("=Storm")>0)
          {
            lightID=1;
            while (lightID<=lightCount)
            {
              lightPin=lightPinPWM[lightID-1];
              for (int i=Intensity[lightID-1]*42;i>0-1;i--){
                analogWrite(lightPin,i);
                delay(fade);
                Status[lightID-1]=0;
              }
              lightID++;
            }
            delay(200);
            Storm=1; // set storm to start after http
            if (Storm==1) 
            {  
              for (int a=0;a<=stormDuration;a++)
              { 
                lightning();      
              }
              delay(50);
              lightID=1;
              while (lightID<=lightCount)
              {
                lightPin=lightPinPWM[lightID-1];
                for (int i=0;i<=255;i++){
                  analogWrite(lightPin,i);
                  delay(30);
                  Status[lightID-1]=1;
                  Intensity[lightID-1]=6;
                  lastIntensity[lightID-1]=Intensity[lightID-1];
                }
                lightID++;
              }
            }
            Storm=0;
          }
          if (lightParsestring.indexOf("=Allon")>0)
          {
            lightID=1;
            while (lightID<=lightCount)
            {
              lightPin=lightPinPWM[lightID-1];
              for (int i=0;i<=255;i++)
              {
                analogWrite(lightPin,i);
                delay(30);
                Status[lightID-1]=1;
                Intensity[lightID-1]=6;
                lastIntensity[lightID-1]=Intensity[lightID-1];
              }
              lightID++;
            }
          }
          if (lightParsestring.indexOf("=Alloff")>0)
          {
            //allOff();
            lightID=1;
            while (lightID<=lightCount)
            {
              lightPin=lightPinPWM[lightID-1];
              for (int i=Intensity[lightID-1]*42;i>0-1;i--){
                analogWrite(lightPin,i);
                delay(fade);
                Status[lightID-1]=0;
              }
              lightID++;
            }

          }

          // send a standard http response header
          client.println(F("HTTP/1.1 200 OK"));
          client.println(F("Content-Type: text/html"));
          client.println(F("Connection: close"));  // the connection will be closed after completion of the response
          //client.println(F("Refresh: 5000"));  // refresh the page automatically every 5 sec
          client.println();
          client.println(F("<!DOCTYPE HTML>"));
          client.println(F("<html>"));
          client.println(F("<head>"));
          client.println(F("<style>"));
          client.println(F(".helpHed"));
          client.println(F("{ border-bottom: 2px solid #6699CC; border-left: 1px solid #6699CC; "));
          client.println(F("background-color: #BEC8D1; text-align: left;"));
          client.println(F("text-indent: 5px; font-family: Verdana; font-weight: bold;"));
          client.println(F("font-size: 11px; color: #404040;"));
          client.println(F("}"));
          client.println(F("table.soft"));
          client.println(F("{ text-align: center; "));
          client.println(F("font-size: small; color: #404040; width: 200px;"));
          client.println(F("background-color: #fafafa; border: 1px #B9BB94 solid; "));
          client.println(F(" border-collapse: collapse; border-spacing: 0px; }"));
          client.println(F("}"));
          client.println(F("</style>"));
          client.println(F("</head>"));
          // end of header

          // tables start here

          //draw main table
          client.println(F("<table  cellspacing='5'>"));
          client.println(F("<tr>"));
          client.println(F("<td>"));

          // draw a table for each light
          int lightID=1;
          while (lightID<=lightCount)
          {
            //start table
            client.println(F("<form name='light settings"));
            client.println(lightID);
            client.println(F("' action='post'>"));
            client.println(F("<table class='soft' cellpadding='3' cellspacing='1' border='1'>"));
            client.println(F("<tr>"));
            client.print(F("<td colspan='2' class='helpHed'>  # "));
            client.println(lightID);
            client.println(F("</td>")); 
            client.println(F("</tr>"));
            // new row
            client.println(F("<tr>"));
            //new cell
            client.println(F("<td>"));
            client.println("Status");
            client.println(F("</td>"));
            //next cell
            // check status and set background colour
            if (Status[lightID-1]==1)
            {
              client.println(F("<td style='background-color:#D6FFD6'>"));
              client.print("On");
            } 
            if (Status[lightID-1]==0)
            {
              client.println(F("<td style='background-color:#FFDEDE'>"));
              client.println("Off");           
            }
            client.println(F("</td>"));
            //end row
            client.println(F("</tr>"));
            // new row
            client.println(F("<tr>"));
            //new cell
            client.println(F("<td>"));
            client.println("Intensity");
            client.println(F("</td>"));
            //next cell
            client.println(F("<td>"));
            // drop down numerical list
            int i =1;
            client.println(F("<select name='intensity'>"));
            while (i<=6)
            {            
              client.print(F("  <option "));
              if (i==Intensity[lightID-1])                      
              {
                client.print(F(" selected "));          
              }
              client.print("value='");
              client.print(i);
              client.println(F("'>"));
              client.print(i);            
              client.println(F("</option>"));
              i++;
            }          
            client.println(F("</td>"));
            //end row
            client.println(F("</tr>"));
            client.println(F("<tr >"));
            client.println(F("<td  colspan='2' >"));
            client.print(F("<input type='submit' name='config"));
            client.print(lightID);
            // check status and set button
            if (Status[lightID-1]==1){
              client.println(F("' value='Turn Off'>")); 
            }
            if (Status[lightID-1]==0){
              client.println(F("' value='Turn On'>")); 
            }
            client.print(F("<input type='submit' name='config"));
            client.print(lightID);
            client.println(F("' value='submit'>"));
            client.println(F("</td>"));
            client.println(F("</tr>"));
            // end table
            client.println(F("</table>"));
            client.println(F("</td>")); 
            client.println(F("<td >"));
            client.println(F("</form>"));
            lightID++;
          }
          //end main table
          client.println(F(" </td> "));
          client.println(F("</tr>"));
          client.println(F("</table>"));

          // current temp,settings and controls
          //draw main table
          client.println(F("<table cellspacing='5'>"));
          client.println(F("<tr valign='top'>"));
          client.println(F("<td >"));
          client.println(F("<table class='soft' cellpadding='5' cellspacing='5' border='1' >"));
          client.println(F("<tr>"));
          client.println(F("<td class ='helpHed' colspan='2'>"));
          client.print("Temperature");           
          client.println(F("</td>")); 
          client.println(F("</tr>"));
          // new row
          client.println(F("<tr>"));
          //new cell
          client.print(F("<td style='background-color:#"));
          // change colour of cell if over or under temp -red over - blue under - green ok
          if (sensors.getTempCByIndex(0)>alarmOvertemp)
          {
            client.print(F("FFDEDE"));                               
          }
          if (sensors.getTempCByIndex(0)<alarmBelowtemp)         
          {
            client.print(F("D6EBFF")); 
          }
          else 
          {
            client.print(F("D6FFD6"));
          }

          client.print(F("' colspan='2'>"));
          client.println("<font size='7'>");
          // print temperature
          sensors.requestTemperatures(); 
          client.print(sensors.getTempCByIndex(0));
          client.println("</font>");
          // new row
          client.println(F("<tr>"));           
          //new cell
          client.println(F("<td >"));
          client.print(F("<font size='5'>Celsius"));
          client.println("</font>");
          client.println(F("</td>"));
          client.println(F("</td>"));           
          //end row
          client.println(F("</tr>"));
          client.println(F("</table>"));
          client.println(F("</td>")); 
          client.println(F("<td >"));

          // device info table to repeat table copy this code to next comment

          //start table
          client.println(F("<table class='soft' cellpadding='5' cellspacing='5' border='1'>"));
          client.println(F("<tr>"));
          client.println(F("<td class ='helpHed' colspan='4'>"));
          client.print("Current Settings");           
          client.println(F("</td>")); 
          client.println(F("</tr>"));
          // new row
          client.println(F("<tr>"));
          // new row
          client.println(F("<tr>"));
          //new cell
          client.println(F("<td>"));
          client.println("On Time");
          client.println(F("</td>"));
          //new cell
          client.println(F("<td>"));
          if(onTime<=999)
          {
            client.print("0");
          }
          client.println(onTime);
          client.println(F("</td>"));
          //end row
          client.println(F("</tr>"));
          // new row
          client.println(F("<tr>"));
          //new cell
          client.println(F("<td>"));
          client.println("Off Time");
          client.println(F("</td>"));
          //new cell
          client.println(F("<td>"));
          if(offTime<=999)
          {
            client.println("0");
          }
          client.println(offTime);
          client.println(F("</td>"));
          //end row
          client.println(F("</tr>"));
          //new cell
          client.println(F("<td>"));
          client.println("Alarm +");
          client.println(F("</td>"));
          //new cell
          client.println(F("<td>"));
          client.println(alarmOvertemp);
          client.println(F("</td>"));
          //end row
          client.println(F("</tr>"));
          // new row
          client.println(F("<tr>"));
          //new cell
          client.println(F("<td>"));
          client.println("Alarm -");
          client.println(F("</td>"));
          //new cell
          client.println(F("<td>"));
          client.println(alarmBelowtemp);
          client.println(F("</td>"));
          //end row
          client.println(F("</tr>"));
          // end table
          client.println(F("</table>"));
          client.println(F("</td>")); 
          client.println(F("<td >"));

          // end of device info table to repeat table copy this code to above comment

          // controls         
          client.println(F("<form name='control buttons"));
          client.println(F("' action='post'>"));
          client.println(F("<table class='soft' cellpadding='5' cellspacing='5' border='1' >"));
          client.println(F("<tr>"));
          client.println(F("<td class ='helpHed' colspan='2'>")); 
          client.print(F("Global Controls"));        
          client.println(F("</td>")); 
          client.println(F("</tr>"));
          // new row
          client.println(F("<tr>"));
          //new cell
          client.println(F("<td>"));
          client.print(F("<input type='submit' name='on' value='Allon'>"));
          client.println(F("</td>"));
          //end row
          client.println(F("</tr>"));
          // new row
          client.println(F("<tr>"));
          //new cell
          client.println(F("<td>"));
          client.print(F("<input type='submit' name='off' value='Alloff'>"));
          client.println(F("</td>"));
          //end row
          client.println(F("</tr>"));
          // new row
          client.println(F("<tr>"));
          //new cell
          client.println(F("<td>"));
          client.print(F("<input type='submit' name='Storm'' value='Storm'>"));
          client.println(F("</td>"));
          //end row
          client.println(F("</tr>"));
          // end table
          client.println(F("</table>"));
          client.println(F("</td>"));
          client.println(F("</form>"));
          //end main table
          client.println(F(" </td> "));
          client.println(F("</tr>"));
          client.println(F("</table>"));

          // settings form

          //if (inString.indexOf("settings") > 1)
          //{
          //          //draw main table
          //          client.println(F("<table valign='top' cellspacing='5'>"));
          //          client.println(F("<tr>"));
          //          client.println(F("<td>"));
          //          // settings tables
          //          // print settings form           
          //          client.println(F("<form name='light settings"));
          //          client.println(F("' action='post'>"));
          //          //start table
          //          client.println(F("<table class='soft' cellpadding='5' cellspacing='5' border='1'>"));
          //          client.println(F("<tr>"));
          //          client.println(F("<td class ='helpHed' colspan='2'>"));
          //          client.print("Settings");           
          //          client.println(F("</td>")); 
          //          client.println(F("</tr>"));
          //          // new row
          //          client.println(F("<tr>"));
          //          //new cell
          //          client.println(F("<td>"));
          //          client.println("IP Address");
          //          client.println(F("</td>"));
          //          //next cell
          //          client.println(F("<td>"));
          //          client.print(F("<input class='sofT' size=10 value = '"));
          //          client.println(Ethernet.localIP());
          //          client.println(F("'name='ip'>"));          
          //          client.println(F("</td>"));
          //          //end row
          //          client.println(F("</tr>"));
          //          // new row
          //          client.println(F("<tr>"));
          //          //new cell
          //          client.println(F("<td>"));
          //          client.println("Gateway");
          //          client.println(F("</td>"));
          //          //next cell
          //          client.println(F("<td>"));
          //          client.print(F("<input class='sofT' size=10 value = '"));
          //          client.println(Ethernet.gatewayIP());
          //          client.println(F("'name='gw'>"));
          //          client.println(F("</td>"));
          //          //end row
          //          client.println(F("</tr>")); 
          //          //end row
          //          client.println(F("</tr>"));
          //          // new row
          //          client.println(F("<tr>"));
          //          //new cell
          //          client.println(F("<td>"));
          //          client.println("onTime");
          //          client.println(F("</td>"));
          //          //next cell
          //          client.println(F("<td>"));
          //          client.print(F("<input class='sofT' size=1 value = '"));
          //          client.println(onTime);
          //          client.println(F("'name='onTime'>"));          
          //          client.println(F("</td>"));
          //          //end row
          //          client.println(F("</tr>"));
          //          // new row
          //          client.println(F("<tr>"));
          //          //new cell
          //          client.println(F("<td>"));
          //          client.println("offTime");
          //          client.println(F("</td>"));
          //          //next cell
          //          client.println(F("<td>"));
          //          client.print(F("<input class='sofT' size=1 value = '"));
          //          client.println(offTime);
          //          client.println(F("'name='offTime'>"));
          //          client.println(F("</td>"));
          //          //end row
          //          client.println(F("</tr>"));
          //          // new row
          //          client.println(F("<tr>"));
          //          //new cell
          //          client.println(F("<td>"));
          //          client.println("Lights");
          //          client.println(F("</td>"));
          //          //next cell
          //          client.println(F("<td>"));
          //          client.print(F("<input class='sofT' size=1 value = '"));
          //          client.println("6");
          //          client.println(F("'name='Count'>"));          
          //          client.println(F("</td>")); 
          //          //end row
          //          client.println(F("</tr>"));
          //          client.println(F("<tr>"));
          //          // end table
          //          client.println(F("</table>"));
          //          client.println(F("</td>")); 
          //          client.println(F("<td >"));
          //          //end main table
          //          client.println(F(" </td> "));
          //          client.println(F("</tr>"));
          //          client.println(F("</table>"));
          //      }
          // format for table
          //            client.println(F("<table class='soft' cellpadding='5' cellspacing='5' border='1'>"));
          //            client.println(F("<tr>"));
          //            client.println(F("<td class ='helpHed' colspan='2'>"));
          //            client.print("<br>");           
          //            client.println(F("</td>")); 
          //            client.println(F("</tr>"));
          //            // new row
          //            client.println(F("<tr>"));
          //            //new cell
          //            client.println(F("<td ></br>"));
          //            client.println("<font size='7'>Test</font></br>");
          //            client.println(F("</td>"));            
          //            //end row
          //            client.println(F("</tr>"));
          //            //end main table
          //            client.println(F(" </td> "));
          //            client.println(F("</tr>"));
          //            client.println(F("</table>"));

          client.println("</html>");
          break;

          inString="";
        }
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
        } 
        else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }
    // give the web browser time to receive the data
    delay(1);
    // close the connection:
    client.stop();
    // start storm after html so page prints  

    inString="";

  }
}

unsigned long getNtpTime()
{
  sendNTPpacket(timeServer); // send an NTP packet to a time server
  delay(1000);
  if ( Udp.parsePacket() ) { 
    Udp.read(packetBuffer,NTP_PACKET_SIZE);  // read packet into buffer

    //the timestamp starts at byte 40, convert four bytes into a long integer
    unsigned long hi = word(packetBuffer[40], packetBuffer[41]);
    unsigned long low = word(packetBuffer[42], packetBuffer[43]);
    // this is NTP time (seconds since Jan 1 1900
    unsigned long secsSince1900 = hi << 16 | low;  
    // Unix time starts on Jan 1 1970
    const unsigned long seventyYears = 2208988800UL;     
    unsigned long epoch = secsSince1900 - seventyYears;  // subtract 70 years
    return epoch;
  }
  return 0; // return 0 if unable to get the time 
}

// send an NTP request to the time server at the given address 

unsigned long sendNTPpacket(IPAddress address)

{
  memset(packetBuffer, 0, NTP_PACKET_SIZE);  // set all bytes in the buffer to 0

  // Initialize values needed to form NTP request
  packetBuffer[0] = B11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum
  packetBuffer[2] = 6;     // Max Interval between messages in seconds
  packetBuffer[3] = 0xEC;  // Clock Precision
  // bytes 4 - 11 are for Root Delay and Dispersion and were set to 0 by memset
  packetBuffer[12]  = 49;  // four-byte reference ID identifying
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // send the packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer,NTP_PACKET_SIZE);
  Udp.endPacket();
}
int adjustBSTEurope()
{
  time_t t = now();
  // last sunday of march
  int beginBSTDate=  (31 - (5* year() /4 + 4) % 7);
  int beginBSTMonth=3;
  //last sunday of october
  int endBSTDate= (31 - (5 * year() /4 + 1) % 7);
  int endBSTMonth=10;
  // BST is valid as:
  if (((month(t) > beginBSTMonth) && (month(t) < endBSTMonth))
    || ((month(t) == beginBSTMonth) && (day(t) >= beginBSTDate)) 
    || ((month(t) == endBSTMonth) && (day(t) <= endBSTDate)))
    return 1;  // BST = utc +1 hour
  else return 0; // GMT = utc
}


void fadeout()
{
  for (int i=lastIntensity[lightID-1]*42;i>=Intensity[lightID-1]*42;i--){
    analogWrite(lightPin,i);
    delay(fade); 
  }
}
void fadein(){
  for (int i=lastIntensity[lightID-1]*42;i<=Intensity[lightID-1]*42;i++){
    analogWrite(lightPin,i);
    delay(fade);
  }
}

void lightning()
{
#define BETWEEN 5000
#define DURATION 40
#define OFFDURATION 1000
#define TIMES 10
#define LONGWAIT 4000
  //  int StormPin = 3;
  // int StormPin2 = 5; use if more than one bank is to flash wud have to add code below
  unsigned long lastTime = 0;
  int waitTime = 0; 
  if (millis() - waitTime > lastTime)  // time for a new flash
  {
    randomSeed (analogRead (0));// randomize
    // adjust timing params
    lastTime += waitTime;
    waitTime = random(BETWEEN); 
    randomSeed (analogRead (0));// randomize
    delay(10 + random(LONGWAIT));

    for (int i=0; i< random(TIMES); i++)
    {
      digitalWrite(StormPin, HIGH);
      delay(20);
      digitalWrite(StormPin, LOW);
      delay(90);
      randomSeed (analogRead (0));// randomize
      digitalWrite(StormPin, HIGH);
      delay(20 + random(DURATION));
      digitalWrite(StormPin, LOW);
      delay(20+ random(OFFDURATION) );      
    }
  }
}

// functions to be called when an alarm triggers:
void MorningAlarm()
{   
  lightID=1;
  while (lightID<=lightCount)
  {
    lightPin=lightPinPWM[lightID-1];
    for (int i=0;i<=255;i++){
      analogWrite(lightPin,i);
      delay(fade*fadeAlarmx);
      Status[lightID-1]=1;

    }
    lightID++;
  }

}

void EveningAlarm()
{    
  lightID=1;
  while (lightID<=lightCount)
  {
    lightPin=lightPinPWM[lightID-1];
    for (int i=Intensity[lightID-1]*42;i>0-1;i--){
      analogWrite(lightPin,i);
      delay(fade*fadeAlarmx);
      Status[lightID-1]=0;
    }
    lightID++;
  }  
}









