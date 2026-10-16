import re
with open("Step1_Verification/src/main.cpp", "r") as f:
    content = f.read()

# insert right before the end of the loop
insert_logic = """
        // LCD Status States
        lcd.setCursor(0, 0);
"""
replacement = """
        Serial.printf("Live - V:%.1f I:%.2f P:%.0f Bal:%.2f\\n", v, i, p, currentBalance);
        
        // LCD Status States
        lcd.setCursor(0, 0);
"""
content = content.replace(insert_logic, replacement)

with open("Step1_Verification/src/main.cpp", "w") as f:
    f.write(content)
