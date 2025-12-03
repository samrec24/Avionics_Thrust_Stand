#include "rpm.h"
#include "../config.h"
#include <Arduino.h>

void rpm_init() {
  // Initialize RPM measurement (e.g., set pin modes, start interrupts, etc.)
    Serial.println("RPM measurement initialized.");
}




// --- Pin and Configuration ---
const int HALL_SENSOR_PIN = 2; // Digital pin connected to the Hall effect sensor (must be 2 or 3 for interrupts on Uno)
const int PULSES_PER_ROTATION = 1; // Number of magnets/pulses per single revolution (adjust this value for your setup)
const unsigned long RPM_UPDATE_INTERVAL = 500; // Milliseconds between printing the RPM to the Serial Monitor

// --- Volatile Variables (Shared with Interrupt) ---
// volatile is crucial for variables modified in the ISR and read in the main loop
volatile unsigned long lastPulseTime = 0; // Time (in microseconds) when the last pulse occurred
volatile unsigned long pulseDuration = 0; // Duration (in microseconds) between the last two pulses
volatile bool newPulse = false;           // Flag to indicate a new pulse duration is available

// --- Variables for Main Loop Calculations ---
unsigned long currentRPM = 0;
unsigned long lastUpdateTime = 0; // Timer for controlling Serial print frequency
const unsigned long MAX_PULSE_DURATION = 500000; // Maximum duration (0.5 seconds) to wait for a pulse.
                                                 // If pulse duration is longer, assume low/zero RPM.

// --- Interrupt Service Routine (ISR) ---
// This function runs every time the Hall sensor sees a magnet (RISING edge)
void pulseInterrupt() {
  unsigned long now = micros();

  // 1. Calculate the duration since the last pulse
  // Only calculate if the duration is valid (not the first pulse, and no rollover error)
  if (lastPulseTime != 0 && now > lastPulseTime) {
    pulseDuration = now - lastPulseTime;
    newPulse = true; // Signal that a new measurement is ready
  }

  // 2. Record the current time for the *next* calculation
  lastPulseTime = now;
}

void setup() {
  Serial.begin(115200);
  Serial.println("High-Speed RPM Sensor Ready (Period Measurement)");

  // Initialize the sensor pin as an input
  pinMode(HALL_SENSOR_PIN, INPUT); 

  // Attach the interrupt:
  // Trigger the 'pulseInterrupt' function on the RISING edge of the signal on Pin 2
  attachInterrupt(digitalPinToInterrupt(HALL_SENSOR_PIN), pulseInterrupt, RISING);
}

void loop() {
  // --- A. Handle New Pulse Calculation ---
  // Check if a new, valid pulse duration is available from the ISR
  if (newPulse) {
    // CRITICAL SECTION: Disable interrupts while reading shared variables
    noInterrupts();
    unsigned long time_period = pulseDuration;
    // Reset the flag immediately after reading
    newPulse = false;
    interrupts();
    // END CRITICAL SECTION

    // Calculate RPM using the period between the last two pulses (in microseconds)
    // RPM = (60 seconds/minute * 1,000,000 microseconds/second) / (time_period * PULSES_PER_ROTATION)
    // Simplified: 60,000,000 / (time_period * PULSES_PER_ROTATION)
    
    // Check for division by zero (shouldn't happen with valid pulseDuration)
    if (time_period > 0) {
      currentRPM = 60000000UL / (time_period * PULSES_PER_ROTATION); // The UL ensures 32-bit arithmetic for the constant
    }
  }

  // --- B. Handle Zero RPM Detection and Display ---
  // This section prevents the last RPM value from sticking when the motor stops.
  if (millis() - lastUpdateTime >= RPM_UPDATE_INTERVAL) {
    // Check if the time since the last recorded pulse exceeds the max expected duration
    // If it has, and we haven't received a new pulse, assume the rotation has stopped or is very slow.
    if (micros() - lastPulseTime > MAX_PULSE_DURATION && currentRPM > 0) {
      currentRPM = 0; // Set RPM to zero
    }
    
    // Print the calculated RPM
    Serial.print("RPM: ");
    Serial.println(currentRPM);
    
    // Update the display timer
    lastUpdateTime = millis();
  }
}
