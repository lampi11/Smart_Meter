import re
with open("Step1_Verification/src/main.cpp", "r") as f:
    content = f.read()

content = content.replace("LiquidCrystal_I2C lcd(0x27, 16, 2);", "LiquidCrystal_I2C* lcd = nullptr;")
content = content.replace("lcd.", "lcd->")
setup_replace = """    // Scan I2C for LCD
    Wire.beginTransmission(0x27);
    if (Wire.endTransmission() == 0) {
        lcd = new LiquidCrystal_I2C(0x27, 16, 2);
    } else {
        Wire.beginTransmission(0x3F);
        if (Wire.endTransmission() == 0) {
            lcd = new LiquidCrystal_I2C(0x3F, 16, 2);
        } else {
            Serial.println("LCD NOT FOUND on 0x27 or 0x3F!");
            lcd = new LiquidCrystal_I2C(0x27, 16, 2); // fallback
        }
    }
    lcd->init();
    lcd->backlight();"""

content = re.sub(r'    // Scan I2C for LCD.*?lcd->backlight\(\);', setup_replace, content, flags=re.DOTALL)

with open("Step1_Verification/src/main.cpp", "w") as f:
    f.write(content)
