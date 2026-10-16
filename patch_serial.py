import re
with open("Step1_Verification/src/main.cpp", "r") as f:
    content = f.read()

insert_logic = """
        Serial.printf("PZEM Read -> V:%.2f I:%.3f P:%.2f E:%.3f (isnan=%d)\\n", v, i, p, e, isnan(v));
        
        if (isnan(v)) {
"""
content = re.sub(r'        if \(isnan\(v\)\) \{', insert_logic, content)

with open("Step1_Verification/src/main.cpp", "w") as f:
    f.write(content)
