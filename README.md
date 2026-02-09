Sistem de Control al Temperaturii
Proiectul implementează un regulator de temperatură bazat pe microcontrolerul Arduino Uno. Sistemul monitorizează temperatura ambientală și acționează un element de încălzire pentru a menține un punct de referință (Set Point) stabilit de utilizator.

Componente Hardware
Microcontroler: Arduino Uno.

Senzor: LM35 (interfațat analogic la pinul A0).


Element de execuție: Bec cu incandescență controlat prin tranzistor de putere TIP31C (Pin D9).


Interfață: Display LCD 16x2 I2C și 4 butoane tactile .


Caracteristici Software
Arhitectură: Automat de stări finite (FSM) cu faze de repaus, încălzire și răcire .


Timing: Utilizare Timer1 și întreruperi pentru cronometrare precisă la 1Hz, evitând utilizarea funcției delay() în logica principală.




Procesare semnal: Medierea a 32 de eșantioane pentru citirea stabilă a temperaturii de la senzor.


Siguranță: Dezactivarea automată a elementului de încălzire la finalizarea ciclului sau în modul meniu.


Link-uri Proiect
Prezentare video: https://youtu.be/JKKef3z4x80

Documentație tehnică detaliată: DocumentatieSistemControlTemperatura.pdf
