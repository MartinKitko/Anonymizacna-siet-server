# Anonymizacna-siet-server
Rozpracovanie anonymizačnej siete s použitím socketov a vlákien - serverová časť

1. klient sa pripojí priamo na server a pošle mu počet uzlov, cez ktoré sa chce pripojiť
2. server mu pošle adresu prvej nódy, na ktorú sa má pripojiť
3. server vyberie daný počet náhodných nód zo zoznamu a spojí ich sockety (každá nóda má 2 sockety)
4. klient sa odpojí od servera, pripojí sa na prvú nódu a odošle jej URL požadovanej stránky na stiahnutie
5. táto správa prejde cez všetky nódy až na server, kde sa stiahne obsah danej stránky
6. ten sa následne pošle klientovi cez nódy v opačnom poradí

Taktiež existuje možnosť pripojenia klienta ako uzol siete, vtedy je zaradený do cesty a je cez neho smerovaná prevádzka.
Server je potrebné spustiť s argumentom programu: port
Ukončiť jeho činnosť je možné zadaním ":end"
