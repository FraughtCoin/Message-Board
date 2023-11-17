Tudorache Cristian 322CB

Pentru a trimite mesaje peste tcp, stiind ca dimensiunea maxima a pachetelor care pot sa circule este in jur
de 1600 bytes, trimit mai intai 2 bytes care reprezinta catul si restul impartirii dimensiunii de transmis la 256.
Apoi trimit intro bucla bytes pana cand ii trimit pe toti.

Cand dau receive, mai intai primesc 2 bytes pe care ii convertesc in dimensiunea totala, iar apoi, intr-o bucla,
primesc bytes pana ajung la numarul total.

Atat serverul cat si clientul TCP utilizeaza multiplexare cu poll pentru a putea citi de pe diferiti sockets.

Inainte de a intra in bulca de client, acesta o sa trimita mai intai id-ul sau.
Clientul TCP trimite comanda pe care o primeste de la tastatura, daca aceasta indeplineste conditiile de validitate.
Cand clientul primeste un pachet de la server, acesta il interpreteaza in functie de tipul sau, iar pentru tipurile
numerice am utilizat operatii pe biti pentru a obtine rezultatul corect.

Pe partea serverului, acesta are doi vectori de clienti/subscriberi si de topicuri. Un subscriber este definit prin
un file descriptor, care reprezinta socketul pe care se comunica cu el, un id care este unic, un vector de topicuri la
care este abonat si un vector de topicuri pe care le-a ratat, cat timp el este offline.
Topicul este caracterizat de numele topicului, un camp sf care tine minte daca trebuie store&forward, continutul si lungimea sa.
Cand un abonat se conecteaza, serverul primeste id-ul sau. Daca el nu exista, se creeaza un abonat nou, daca exista si este offline
se reface online, altfel se deconecteaza.
Un topic nou este creat atunci cand un client da subscribe si este adaugat in vectorul abonatului respectiv.
Cand un client se deconecteaza, fd-ul sau o sa deina -1, pentru a sti ca el este deconectat.
Cand un client da unsubscribe, ma plimb prin lista lui de topicuri si il elimin, mutand restul cu un index mai la stanga.

Cand un client UDP trimite un pachet, ii adaug ip-ul si portul sau si il trimit mai departe, iterand prin toti abonatii si trimitandu-l
atunci cand gasesc topic cu numele specific. Daca abonatul este offline, il adaug in lista de sf pentru a le trimite cand se reconecteaza.
