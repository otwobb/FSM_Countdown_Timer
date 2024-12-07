//EEEN20011 Lab 2
//Oliver Brown
//Student ID: 11310030
//Countdown timer implemented as an FSM
//This project scored 84% in assesment

#include "mbed.h"
#include "C12832.h" //Imports the library for the LCD screen

C12832 lcd(D11, D13, D12, D7, D10); //Creates an LCD Object from the LCD library

//Create global variables for minutes and seconds
//These must be global as they need to be accessed by several ISRs as well as main
volatile int Minutes = 1;
volatile int Seconds = 1;
volatile int Seconds_Total = 1;

//Create counter APIs
//Timeout countdown;                                 
Timeout countdown;                                  //Countdown API to trigger state change when time is elapsed
Ticker time_elapsed;                                //Ticker API to allow tracking of time elapsed
Ticker frequency;                                   //Ticker API to control the frequency of the buzzer

class LED                                           //Begin LED class definition
{

protected:                                          //Protected (Private) data member declaration
    DigitalOut outputSignal;                        //Declaration of DigitalOut object
    bool status;                                    //Variable to recall the state of the LED

public:                                             //Public declarations
    LED(PinName pin) : outputSignal(pin)
    {
        off();   //Constructor - user provides the pin name, which is assigned to the DigitalOut
    }

    void on(void)                                   //Public member function for turning the LED on
    {
        outputSignal = 0;                           //Set output to 0 (LED is active low)
        status = true;                              //Set the status variable to show the LED is on
    }

    void off(void)                                  //Public member function for turning the LED off
    {
        outputSignal = 1;                           //Set output to 1 (LED is active low)
        status = false;                             //Set the status variable to show the LED is off
    }

    void toggle(void)                               //Public member function for toggling the LED
    {
        if (status)                                 //Check if the LED is currently on
            off();                                  //Turn off if so
        else                                        //Otherwise...
            on();                                   //Turn the LED on
    }

    bool getStatus(void)                            //Public member function for returning the status of the LED
    {
        return status;                              //Returns whether the LED is currently on or off
    }
};

//Create LED objects for green                       
LED greenLED(D9);                                   //Green LED must be global as it is accessed by an ISR

class Potentiometer                                 //Begin Potentiometer class definition
{
private:                                            //Private data member declaration
    AnalogIn inputSignal;                           //Declaration of AnalogIn object
    float VDD, currentSampleNorm, currentSampleVolts; //Float variables to speficy the value of VDD and most recent samples

public:                                             // Public declarations
    Potentiometer(PinName pin, float v) : inputSignal(pin), VDD(v) {}   //Constructor - user provided pin name assigned to AnalogIn...
                                                                        //VDD is also provided to determine maximum measurable voltage
    float amplitudeVolts(void)                      //Public member function to measure the amplitude in volts
    {
        return (inputSignal.read()*VDD);            //Scales the 0.0-1.0 value by VDD to read the input in volts
    }
    
    float amplitudeNorm(void)                       //Public member function to measure the normalised amplitude
    {
        return inputSignal.read();                  //Returns the ADC value normalised to range 0.0 - 1.0
    }
    
    void sample(void)                               //Public member function to sample an analogue voltage
    {
        currentSampleNorm = inputSignal.read();       //Stores the current ADC value to the class's data member for normalised values (0.0 - 1.0)
        currentSampleVolts = currentSampleNorm * VDD; //Converts the normalised value to the equivalent voltage (0.0 - 3.3 V) and stores this information
    }
    
    float getCurrentSampleVolts(void)               //Public member function to return the most recent sample from the potentiometer (in volts)
    {
        return currentSampleVolts;                  //Return the contents of the data member currentSampleVolts
    }
    
    float getCurrentSampleNorm(void)                //Public member function to return the most recent sample from the potentiometer (normalised)
    {
        return currentSampleNorm;                   //Return the contents of the data member currentSampleNorm  
    }
};

class SamplingPotentiometer : public Potentiometer {

private:
    float samplingFrequency, samplingPeriod;
    Ticker sampler;

public:
    SamplingPotentiometer(PinName p, float v, float fs) : Potentiometer(p, v), samplingFrequency(fs) {
        //Create sampling period from frequency

        samplingPeriod = 1.0f / samplingFrequency;

        //Attach ticker to for a sample at specified frequency
        sampler.attach(this, &SamplingPotentiometer::sample, samplingPeriod);
    }
};

class Speaker                                       //Begin speaker class definition. This class is very similar to the LED class, but in order to have coherent class names a new class is created rather than inheriting.         
{

protected:                                          //Protected (Private) data member declaration
    DigitalOut outputSignal;                        //Declaration of DigitalOut object
    bool status;                                    //Variable to recall the state of the LED

public:                                             //Public declarations
    Speaker(PinName pin) : outputSignal(pin)
    {
        off();   //Constructor - user provides the pin name, which is assigned to the DigitalOut
    }

    void on(void)                                   //Public member function for turning the LED on
    {
        outputSignal = 0;                           //Set output to 0 (LED is active low)
        status = true;                              //Set the status variable to show the LED is on
    }

    void off(void)                                  //Public member function for turning the LED off
    {
        outputSignal = 1;                           //Set output to 1 (LED is active low)
        status = false;                             //Set the status variable to show the LED is off
    }

    void toggle(void)                               //Public member function for toggling the LED
    {
        if (status)                                 //Check if the LED is currently on
            off();                                  //Turn off if so
        else                                        //Otherwise...
            on();                                   //Turn the LED on
    }

    bool getStatus(void)                            //Public member function for returning the status of the LED
    {
        return status;                              //Returns whether the LED is currently on or off
    }
};

Speaker buzzer(D6);                                 //Create object for Nucleo board buzzer. Must be global as is accessed by ISRs


typedef enum {initialisation, set_duration, timer_running, timer_paused, time_elapsed_done, restart_timer, clear_screen} ProgramState;
ProgramState state;

void tickerISR() {                                  //ISR to toggle the greenLED and decrease time by 1s. Attached to Ticker time_elapsed

    greenLED.toggle();


    if (Seconds == 1 && Minutes == 0) {             //Detach ticker if time is up
        time_elapsed.detach();
    }
    if (Seconds == 0 && Minutes != 0) {
        Seconds = 59;
        Minutes--;
        Seconds_Total--;
    }
    else {
        Seconds--;  
        Seconds_Total--;
    }


}

void buzzerISR() {                                  //ISR to toggle the buzzer. Attached to Ticker frequency      

    buzzer.toggle();

}

void timeoutISR() {                                 //ISR to be actioned upon time completing. Attached to Timeout countdown

    frequency.attach(&buzzerISR, 0.001);
    state = time_elapsed_done;                      //Change state to time_elapsed_done

}

void fireISR(){                                     //ISR for when the centre button is pressed

    switch (state){                                 //Case statements for when the centre button is pressed

        case (initialisation)  :                    //When centre is pressed should move to set_duration
            state = set_duration;
            break;

        case (set_duration) :                       //When centre is pressed should move to timer_running
            state = timer_running;
            time_elapsed.attach(&tickerISR, 1.0);
            countdown.attach(&timeoutISR, Seconds_Total);
            break;

        case (timer_running) :                      //When centre is pressed should move to timer_paused
            state = timer_paused;
            time_elapsed.detach();
            countdown.detach();
            break;

        case (timer_paused) :                       //When centre is pressed should move to timer_running
            state = timer_running;
            time_elapsed.attach(&tickerISR, 1.0);
            countdown.attach(&timeoutISR, Seconds_Total);
            break;

        case (time_elapsed_done) :                  //When centre is pressed should move to set_duration
            state = set_duration;
            frequency.detach();

            break;

        default :                                   //Reset to initialisation if an erorr occurs
            state = initialisation;
            break;

    }
}

void upISR() {

    switch (state) {

        case(initialisation) :                      //Should not move state
        state = state;
        break;

        case(set_duration) :                        //Should not move state
        state = state;
        break;

        case(timer_running) :                       //Should not move state
        state = state;
        break;

        case(timer_paused) :                        //When up is pressed should move to restart_timer
        state = restart_timer;
        break;

        case(time_elapsed_done) :                   //Should not move state
        state = state;
        break;

        default :                                   //Reset to initialisation if an error occurs
        state = initialisation;
        break;

    }
}

void downISR() {

    switch (state){

        case(initialisation) :                      //Should not move state
        state = state;
        break;  

        case(set_duration) :                        //Should not move state
        state = state;
        break;   
        
        case(timer_running) :                       //Should not move state
        state = state;
        break;  

        case(timer_paused) :                        //When down is pressed should move to set_duration
        state = set_duration;
        break;

        case(time_elapsed_done) :                   //Should not move state
        state = state;
        break;   

        default :                                   //Reset to initialisation if an error occurs
        state = initialisation;
        break;

    }
}


int main()
{
    //Declare red and blue LEDs
    LED redLED(D5);                                 //Red and blue can be declared in main as they are only accessed from it                          
    LED blueLED(D8);  

    //Declare InterruptIn objects for the left, right and centre ISRs
    InterruptIn centre(D4);                         
    InterruptIn up(A2);
    InterruptIn down(A3);

    //Clear LCD
    lcd.cls();

    //Declare Objects from SamplingPotentiometer class for the right and left potentiometers
    SamplingPotentiometer left_potentiometer(A0, 3.3, 100);
    SamplingPotentiometer right_potentiometer(A1, 3.3, 100);

    //Assign inputs to their relative ISRs
    centre.rise(&fireISR);                          //When centre is pressed fireISR will be called
    up.rise(&upISR);                                //When up is pressed upISR will be called
    down.rise(&downISR);                            //When down is pressed downISR will be called

    state = initialisation;                         //Begin in state initialisation

    //Declare variables for main
    //To keep track of time before countdown begins:
    int Initial_Minutes = Minutes;                  
    int Initial_Seconds = Seconds;
    int Initial_Seconds_Total = Seconds_Total;

    //Ratios for filling the bars:
    int bar_fill1;  
    int Initial_bar_fill1 = bar_fill1;
    int bar_fill2;  
    int Initial_bar_fill2 = bar_fill2;
    int bar_fill3;  
    int Initial_bar_fill3 = bar_fill3;

    while(true) {

        switch (state) {

            case (initialisation) :


                lcd.rect(0, 0, 127, 31, 1);
                lcd.rect(2, 2, 125, 29, 1);
                lcd.locate(7, 5);
                lcd.printf("INITIALISATION COMPLETE");
                lcd.locate(35, 18);
                lcd.printf("F: CONTINUE");

                redLED.off();
                greenLED.off();
                blueLED.off();
            break;

            case (set_duration) :
                
                Minutes = 10 * left_potentiometer.amplitudeNorm();
                Seconds = 60 * right_potentiometer.amplitudeNorm();
                Seconds_Total = 60 * Minutes + Seconds;

                bar_fill1 = 18 * float (Seconds) / float (Initial_Seconds);
                bar_fill2 = 18 * float (Minutes) / float (Initial_Minutes);

                if(bar_fill1 != Initial_bar_fill1 || bar_fill2 != Initial_bar_fill2){
                    wait(0.1);
                    lcd.cls();
                }

                Initial_Minutes = Minutes;
                Initial_Seconds = Seconds;
                Initial_Seconds_Total = Seconds_Total;
                Initial_bar_fill1 = bar_fill1;
                Initial_bar_fill2 = bar_fill2;

                if (Minutes > 9) {                          //Limit minutes to 9 to avoid flickering between 9 and 10
                    Minutes = 9;
                }
                if (Seconds > 59) {                         //Limit seconds to 59 to avoid flickering between 59 and 60
                    Seconds = 59;
                }

                lcd.rect(0, 0, 127, 31, 1);
                lcd.rect(2, 2, 125, 29, 1);
                lcd.locate(7, 5);
                lcd.printf("SET TIME DURATION: %d:%02d", Minutes, Seconds);
                lcd.locate(7, 18);
                lcd.printf("Minutes");
                lcd.locate(60, 18);
                lcd.printf(" Seconds");
                lcd.rect(42, 19, 61, 25, 1);
                lcd.rect(101, 19, 121, 25, 1);
                lcd.fillrect(43, 20, (44 + 2 * (Minutes - 1)), 24, 1);
                lcd.fillrect(101, 20, (102+ 2 * (Seconds - 1) / 6), 24, 1);



                redLED.on();
                greenLED.off();
                blueLED.off();
            break;

            case (timer_running) :   

                redLED.off();
                blueLED.off(); 
                bar_fill3 = 113 * float (Seconds_Total) / float (Initial_Seconds_Total);
                
                if(bar_fill3 != Initial_bar_fill3){
                    lcd.cls();
                }

                Initial_bar_fill3 = bar_fill3;

                lcd.rect(0, 0, 127, 31, 1);
                lcd.rect(2, 2, 125, 29, 1);
                lcd.locate(11, 5);
                lcd.printf("TIME REMAINING: %d:%02d   ", Minutes, Seconds);
                lcd.fillrect(7, 19, 121, 25, 1);
                lcd.fillrect(8, 20, 120 - bar_fill3, 24, 0);
                
            break;

            case(timer_paused) :

                lcd.rect(0, 0, 127, 31, 1);
                lcd.rect(2, 2, 125, 29, 1);
                lcd.locate(11, 5);
                lcd.printf("   TIMER PAUSED    ");
                lcd.locate(6, 18);
                lcd.printf(" F:CONT. U:RESET D:QUIT ");

                redLED.on();
                greenLED.on();
                blueLED.off();
            break;

            case(time_elapsed_done) : 
                
                lcd.rect(0, 0, 127, 31, 1);
                lcd.rect(2, 2, 125, 29, 1);
                lcd.locate(11, 5);
                lcd.printf("     TIME ELAPSED     ");
                lcd.rect(7, 19, 121, 25, 1);
                lcd.fillrect(8, 20, 120, 24, 0);

                redLED.off();
                greenLED.off();
                blueLED.off();
            break;

            case(restart_timer) :

                Minutes = Initial_Minutes;
                Seconds = Initial_Seconds;
                Seconds_Total = Initial_Seconds_Total;

                time_elapsed.attach(&tickerISR, 1.0);
                countdown.attach(&timeoutISR, Seconds_Total);

                state = timer_running;
            break;

            default : 

                state = initialisation;
            break;

        }
    }
}