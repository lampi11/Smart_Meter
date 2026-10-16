import re
with open("Step1_Verification/src/main.cpp", "r") as f:
    content = f.read()

lcd_logic = """        if (isnan(v)) {
            // Hardware failure state (or no AC power to PZEM)
            v = 0.0; i = 0.0; p = 0.0;
        }"""

content = re.sub(r'        if \(isnan\(v\)\) \{.*?return;\n        \}', lcd_logic, content, flags=re.DOTALL)

with open("Step1_Verification/src/main.cpp", "w") as f:
    f.write(content)
