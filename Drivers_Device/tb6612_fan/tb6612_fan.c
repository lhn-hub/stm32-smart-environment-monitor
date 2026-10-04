/* M0 hardware contract only; no motor driver is implemented or enabled yet.
 * A channel must never drive. Never initialize TIM3_CH1/PWMA.
 * CS=0 also means STBY=0: TB6612 is in standby during SPI transfers.
 * CS=1: SCK/AIN1 idle low; W25Q releases MISO/PWMA, whose nominal
 * TB6612 internal 200k pull-down holds it low. With PWMA low, neither
 * value of PB0/AIN2 is intended to produce differential motor drive.
 * This steady-state rationale is NOT measured proof of switching safety.
 * bus_arbiter give: PA4/STBY=0, PA7/BIN1=0, PA5/SCK idle=0, FAN CCR=0.
 * M4/M5 must record full-speed SPI stress and AO1/AO2 differential voltage
 * or safely observed A-channel shaft motion. The B fan alone cannot verify A.
 */
