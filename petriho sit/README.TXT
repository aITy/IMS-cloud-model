V loadbalancing jsem smazal odchod pozadavku ze systemu pro server A,B,C - nemelo to pro ne smysl.
Pokud by byl server A na maximum, nesel bych dal k ostatnim serverum.
U toho priznaku je navic spatne to, ze ho vemu a nevracim ... a navic tam neni nijak modelovano to, kdy se ten priznak nuluje
tim myslim detekce, ze system je zas nezatizen
V kodu jsem to udelal jako zarizeni a vracim ho vzdycky, jen neni udelana nijak ta detekce znovustaveni "rovnovahy" clusteru

PS: zitra to pak nejak dame dohromady a prekreslime + predelame kod