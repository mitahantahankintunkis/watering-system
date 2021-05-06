#include <stdio.h>
#include <string.h>
#include <SoftwareSerial.h>

#define RX 3
#define TX 4

SoftwareSerial BTSerial(RX, TX);


// Hardware pins
int moisture_sensor_pin = 1;
int water_level_pin = 0;
int motor_pin = 5;

// Measured data
int moisture_level = 0;
int water_level = 0;

// Threshold values for triggering different events
int water_level_threshold = 25;
int moisture_level_threshold = 700;
int moisture_level_range = 64;

// Sensor measurement variables
unsigned long measure_interval = 200;
int measure_repeats = 5;
int current_repeats = 0;


// Different program states
enum ProgramState {
    SLEEP,
    MEASURE_SOIL_MOISTURE,
    CHECK_WATER_LEVEL,
    WATER_PLANTS,
    MOTOR_COOLDOWN,
};

// Starting on SLEEP in case the power supply/motor breaks and
// the power switches off each time the motor runs
ProgramState state = SLEEP;

bool watering = false;


// Timing variables

unsigned long watering_duration = 30 * 1000;
unsigned long motor_cooldown_duration = 75 * 1000;
unsigned long sleep_duration = 15 * 60 * 1000;
unsigned long state_begin = 0;


// Bluetooth communication variables
#define INPUT_SIZE 63
unsigned char input_buffer[INPUT_SIZE + 1];



// Utility function which checks whether str0 starts with str1
bool string_starts_with(char* str0, char* str1) {
    size_t lenstr0 = strlen(str0);
    size_t lenstr1 = strlen(str1);

    return lenstr0 < lenstr1 ? false : memcmp(str0, str1, lenstr1) == 0;
}


void parse_commands() {
    // Reading the received data. Using C strings
    size_t read_bytes = BTSerial.readBytesUntil('\n', input_buffer, INPUT_SIZE);
    input_buffer[read_bytes] = '\0';

    // TODO - convert these into a loop in case there will be more commands
    if (string_starts_with(input_buffer, "status")) {
        unsigned long dt = millis() - state_begin;
        unsigned long minutes = dt / (60 * 1000);
        unsigned long seconds = dt / (1000) - minutes * 60;

        BTSerial.println("Status:");

        BTSerial.print("    Time in current state:  ");
        BTSerial.print(minutes);
        BTSerial.print("m ");
        BTSerial.print(seconds);
        BTSerial.println("s");

        BTSerial.print("    Soil moisture sensor:  ");
        BTSerial.println(moisture_level);

        BTSerial.print("    Soil moisture range:   ");
        BTSerial.print(moisture_level_threshold);
        BTSerial.print(" +/- ");
        BTSerial.println(moisture_level_range);

        BTSerial.print("    Water level sensor:    ");
        BTSerial.println(water_level);

        BTSerial.print("    Water level threshold: ");
        BTSerial.println(water_level_threshold);

    } else if (string_starts_with(input_buffer, "help")) {
        BTSerial.println("Automatic watering system");
        BTSerial.println("    Commands:");
        BTSerial.println("        help");
        BTSerial.println("            Shows this message");
        BTSerial.println("");
        BTSerial.println("        status");
        BTSerial.println("            Shows system status");
        BTSerial.println("");
        BTSerial.println("        wake_up");
        BTSerial.println("            Wakes up from sleep mode");
        BTSerial.println("");
        BTSerial.println("        sleep");
        BTSerial.println("            Goes to sleep");
        BTSerial.println("");
        BTSerial.println("        force_water");
        BTSerial.println("            Waters the plants without checking anything");
        BTSerial.println("");
        BTSerial.println("        set_moisture_threshold (num)");
        BTSerial.println("            Sets the desired soil moisture level");
        BTSerial.println("");
        BTSerial.println("        set_moisture_range (num)");
        BTSerial.println("            Sets the acceptable range of soil moisture");
        BTSerial.println("");
        BTSerial.println("        set_water_level_threshold (num)");
        BTSerial.println("            Sets the water tank level cutoff threshold");

    } else if (string_starts_with(input_buffer, "wake_up")) {
        state_begin = millis();
        current_repeats = 0;
        state = MEASURE_SOIL_MOISTURE;

    } else if (string_starts_with(input_buffer, "force_water")) {
        state_begin = millis();
        current_repeats = 0;
        state = WATER_PLANTS;

    } else if (string_starts_with(input_buffer, "sleep")) {
        state_begin = millis();
        current_repeats = 0;
        digitalWrite(motor_pin, LOW);
        state = SLEEP;

    } else if (string_starts_with(input_buffer, "set_moisture_threshold")) {
        int a;
        size_t n = sscanf(input_buffer, "set_moisture_threshold %d", &a);

        if (n != 1 || a < 0 || a > 1023) {
            BTSerial.println("Error: expected 1 value in range [0, 1023]");
        } else {
            BTSerial.println("accepted");
            moisture_level_threshold = a;
        }

    } else if (string_starts_with(input_buffer, "set_moisture_range")) {
        int a;
        size_t n = sscanf(input_buffer, "set_moisture_range %d", &a);

        if (n != 1 || a < 0 || a > 1023) {
            BTSerial.println("Error: expected 1 value in range [0, 1023]");
        } else {
            BTSerial.println("accepted");
            moisture_level_range = a;
        }

    } else if (string_starts_with(input_buffer, "set_water_level_threshold")) {
        int a;
        size_t n = sscanf(input_buffer, "set_water_level_threshold %d", &a);

        if (n != 1 || a < 0 || a > 1023) {
            BTSerial.println("Error: expected 1 value in range [0, 1023]");
        } else {
            BTSerial.println("accepted");
            water_level_threshold = a;
        }
    } else {
        BTSerial.println("Unknown command. Type 'help' for infromation");
    }
}


void setup() {
    pinMode(moisture_sensor_pin, INPUT);
    pinMode(water_level_pin, INPUT);
    pinMode(motor_pin, OUTPUT);

    // Failsafe
    digitalWrite(motor_pin, LOW);

    BTSerial.begin(38400);
    BTSerial.println("Hello! Going to sleep for a bit.");
}


// Does nothing for a while
ProgramState sleep() {
    unsigned long dt = millis() - state_begin;

    // Failsafe
    digitalWrite(motor_pin, LOW);
    watering = false;

    if (dt >= sleep_duration) {
        BTSerial.println("Measuring soil moisture");

        return MEASURE_SOIL_MOISTURE;
    }

    return SLEEP;
}



ProgramState measure_soil_moisture() {
    unsigned long dt = millis() - state_begin;

    if (current_repeats == 0) {
        moisture_level = 0;
    }

    if (dt >= measure_interval) {
        moisture_level += analogRead(moisture_sensor_pin);
        ++current_repeats;
    }

    if (current_repeats >= measure_repeats) {
        moisture_level /= measure_repeats;
        current_repeats = 0;

        BTSerial.print("Soil moisture: ");
        BTSerial.println(moisture_level);

        if (watering) {
            // Waters the soil to moisture_level_threshold + moisture_level_range
            if (moisture_level < moisture_level_threshold + moisture_level_range) {
                BTSerial.println("Checking water level");

                return CHECK_WATER_LEVEL;
            }

        } else {
            // Waits for the soil to dry to moisture_level_threshold - moisture_level_range
            if (moisture_level < moisture_level_threshold - moisture_level_range) {
                BTSerial.println("Checking water level");

                return CHECK_WATER_LEVEL;
            }
        }

        BTSerial.println("Going to sleep");
        return SLEEP;
    }

    return MEASURE_SOIL_MOISTURE;
}


ProgramState check_water_level() {
    unsigned long dt = millis() - state_begin;

    if (current_repeats == 0) {
        water_level = 0;
    }

    if (dt >= measure_interval) {
        water_level += analogRead(water_level_pin);
        ++current_repeats;
    }

    if (current_repeats >= measure_repeats) {
        water_level /= measure_repeats;
        current_repeats = 0;

        BTSerial.print("Water level: ");
        BTSerial.println(water_level);

        if (water_level > water_level_threshold) {
            BTSerial.println("Watering plants");

            return WATER_PLANTS;
        }

        BTSerial.println("Not enough water, going to sleep");
        return SLEEP;
    }

    return CHECK_WATER_LEVEL;
}


ProgramState water_plants() {
    unsigned long dt = millis() - state_begin;
    watering = true;

    digitalWrite(motor_pin, HIGH);

    if (dt >= watering_duration) {
        digitalWrite(motor_pin, LOW);
        BTSerial.println("Cooling the motor down");

        return MOTOR_COOLDOWN;
    }

    return WATER_PLANTS;
}


ProgramState motor_cooldown() {
    unsigned long dt = millis() - state_begin;

    if (dt >= motor_cooldown_duration) {
        BTSerial.println("Measuring soil moisture");

        return MEASURE_SOIL_MOISTURE;
    }

    return MOTOR_COOLDOWN;
}


void loop() {
    if (BTSerial.available()) {
        parse_commands();
    }

    // State machine
    ProgramState new_state = state;

    switch (state) {
        case SLEEP:
            new_state = sleep();
            break;

        case MEASURE_SOIL_MOISTURE:
            new_state = measure_soil_moisture();
            break;

        case CHECK_WATER_LEVEL:
            new_state = check_water_level();
            break;

        case WATER_PLANTS:
            new_state = water_plants();
            break;

        case MOTOR_COOLDOWN:
            new_state = motor_cooldown();
            break;

        default:
            break;
    }

    // Checking if the state switched
    if (state != new_state) {
        // Failsafe
        digitalWrite(motor_pin, LOW);

        state_begin = millis();
        state = new_state;
    }
}
