import re
with open("Step1_Verification/src/main.cpp", "r") as f:
    content = f.read()

content = re.sub(r'Serial\.printf\("PZEM.*?isnan\(v\)\);', '', content, flags=re.DOTALL)

with open("Step1_Verification/src/main.cpp", "w") as f:
    f.write(content)
