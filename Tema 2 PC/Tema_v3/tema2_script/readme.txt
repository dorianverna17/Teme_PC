Verna Dorian-Alexandru 324CC
Tema 2 Protocoale de Comunicatii

Timp implementare: 5 zile ~ 3 ore pe zi adunate (poate chiar 4)
Dificultate: Mediu -> Greu (Ma asteptam sa fie mai usoara decat
prima dar nu prea a fost :)))

Detalii de implementare:
Implementarea mea consta in 3 fisiere: server.c, subscriber.c si
helpers.h, am scris codul in C dar am regretat putin dupa pentru
ca mi-am dat seama ca ar trebui sa implementez propriile mele
hashtable-uri pentru a fi eficienta. (probabil ca ar fi trebuit
sa fac in C++).

Serverul si clientul TCP pornesc normal, similar oarecum cu
felul in care i-am facut sa porneasca in Laboratorul de Multiplexare.
(Creez socketii, realizez bind etc.). Ambele programe au fiecare cate
un while mare unde implementez functionalitatile celor doua programe

In helpers.h am realizat 3 tabele de hash-uri care ma ajuta in
materie de eficienta a programului. Toate acestea sunt folosite in
cadrul serverului si sunt:

-> o tabela pentru socketi in care key-ul ar fi socket-ul, iar ca si
valoare sunt clientii (id-urile lor). Am atasat definitiei tabelei si
o serie de functii pentru adaugarea unui client pe un socket, gasirea
unuia, stergerea unuia.

-> o tabela pentru clienti in care key-ul este id-ul, aici se regasesc
informatiile despre clienti precum socketul pe care este in prezent
conectat, portul, un flag connected care imi spune daca clientul este
conectat sau nu (0 sau 1) si o lista pe care o folosesc pentru a trimite
mesajele care se acumuleaza atunci cand sf este 1, iar clientul este
deconectat. La fel ca si tabela precedenta, si acesteia i-am impelementat
o serie de functii pentru a usura lucrul cu aceasta, cautarea si adaugarea
in ea.

-> o tabela pentru topicuri in care key-ul este un topic, iar fiecare
intrare din tabela contine o lista de structuri pentru userii care au
dat subscribe la acel topic, o structura de acest fel fiind formata din
id-ul clientului care a dat subscribe si sf-ul acestuia. Functiile
importante aici sunt cele de adaugare a topicului, de subscribe, unsubscribe,
gasire a topicului si a verificarii daca exista un topic.

Toate tabelele folosite de mine folosesc separate chaining.
La primul si la al doilea hashtable (cel pt clienti si cel pt topicuri) am
folosit o functie de hash pentru stringuri si anume una care se foloseste de
algoritmul djb2 (de aici m-am inspirat: http://www.cse.yorku.ca/~oz/hash.html),
algoritmul practic are aceeasi functionalitate ca cel din link, dar a trebuit
sa ii aduc modificari pentru a avea o functionalitate buna, in sensul ca nu
garanta neaparat obtinerea unui hash bun (uneori obtineam unul negativ -> de
aici si un Broken Pipe pe checker la care mi-a luat destul de mult sa-mi dau
seama ce il cauza). Anyway, pe forum mi s-a spus ca e ok daca folosesc acest
algoritm si mentionez asta in readme, precum si sursa.

Serverul accepta conexiuni cu clientii TCP si UDP, la fel cum este mentionat
si in enunt, printez mesajele New client <ID_CLIENT> connected from IP:PORT. si
Client <ID_CLIENT> disconnected., iar serverul si subscriber-ul imi accepta
comenzile care trebuie luate de la tastatura fara sa afiseze lucruri in plus.
Reconectarile merg, iar daca trebuie sa afisez ca un client este deja conectat
atunci trebuie sa verific daca acesta este in hash table.

Comenzile de abonare si dezabonare au efect asupra tabelei topics si se folosesc
de functiile de subscribe_client si unsubscribe_client.

Optiunea store and forward este implementata folosind tabela topics, tabela topics
retine pe langa id-ul clientului care a dat subscribe si sf-ul acestuia. Astfel,
atunci cand un udp trimite un mesaj pe acel topic. Il preiau in server, iar mai
apoi ma duc la intrarea topicului in tabela topics. In mod normal, iau fiecare id
din lista topicurilor, iar daca clientul cu acel id are connected == 1, atunci ii
trimit mesajul, daca nu atunci nu il trimit (verificare in table - tabela de clienti).
La store-and-forward am mai adaugat verificarea ca in cazul in care nu e conectat
clientul, deci connected == 0, dar sf-ul este 1 pentru acel topic, atunci
adaug in lista de mesaje de trimis clientului din tabela table mesajul respectiv.
Atunci cand clientul se reconecteaza, voi trimite toate mesajele din acea lista
si voi reinitializa lista.

Din punct de vedere al functionalitatii programului cu mai multi clienti, acesta
este bun, dar am observat ca se intampla sa dea rateuri cateodata. Am studiat si
problemele celorlalti de pe forum si am reusit sa reduc aceste cazuri in care nu
da bine (primeam un broken pipe pentru quick flow). Ce am facut a fost sa controlez
flow-ul octetilor transmisi in server si primiti in server pentru un mesaj tcp_msg.
Practic trimit intr-un while mesajele si le si preiau pana cand au fost trimisi un
numar de octeti egal cu sizeof(tcp_msg). Din 25-30 de rulari, acel test cu quick
flow nu imi trecea pentru aproximativ 3 cazuri.

Protocol la nivel aplicatie:
Am doua structuri pentru protocolul pe care doream sa il realizam, una pentru
un mesaj tcp pe care il parsez odata ce este trimis un mesaj de conectare a unui
client tcp la server, initial, acesta este transmis ca un buffer de forma char*,
dar este mai apoi parsat in server pentru o functionalitate buna.

Cea de-a doua structura este tcp_message (ambele fiind definite in helpers.h).
Ea reprezinta un mesaj tcp -> mesajul putand fi de 4 tipuri: subscribe, unsubscribe,
mesaj cu topic from server si mesaj de deconectare. Valoare ce trebuie sa o printeze
clientul tcp este reprezentata de campul value din aceasta structura, dar are si camp
pentru adresa udp-ului care a trimis mesajul, portul pe care este conectat, id-ul
clientului catre care a fost trimis mesajul, topicul si sf-ul. Cam atat despre
semnificatia tipurilor folosite in cadrul structurii pentru protocolul la nivel
aplicatie.

Ca reguli, in primul rand mesajul este primit intr-un while, asa cum am spus mai sus,
primind octetii pe rand pana atunci cand este primit un mesaj intreg de tip tcp_msg.
(am un while in care pentru trimitere apelez de mai multe ori send pana cand am trimis
un numar de octeti egal cu sizeof(tcp_msg), iar pentru recv procedez la fel -> pana
primesc sizeof(tcp_msg)).
Daca primesc un mesaj udp, atunci ma folosesc de functiile parse_udp_message si
udp_to_string pentru a forma valoare cu care voi initializa campul value din
structura tcp_msg. Serverul poate trimite doar doua tipuri de mesaje de tip tcp la
clientii tcp si anume de tip 3 si 2, adica ori ii poate da un mesaj de disconnect
unui client sau ii poate da un mesaj primit de la clientul udp. Dupa ce am obtinut
valoarea pentru membrul value, atunci pot sa continui cu trimiterea mesajelor.
obtinerea membrului value si trimiterea mesajelor tcp se realizeaza in cadrul functiei
send_tcp_messages. De asemenea, si functia send_remaining_messages are rolul de
a trimite mesaje tcp in acelasi mod, doar ca acestea sunt formate deja. Un subscriber
poate trimite mesaje tcp_msg de tip 0 sau 1, adica subscribe si unsubscribe, acestea
fiind tratate si ele la randul lor in server.

Sper ca explicatiile pentru implementarea protocolului sunt ok, codul contine si
comentarii ajutatoare.

In cadrul programului verific si inputul primit de la tastatura, dezactivez Neagle
si nu afisez mesaje in plus. Conversiile de byte order le fac in functia
parse_udp_message. Inchiderea serverului duce si la inchiderea automata a clientilor
(se trimite cate un tcp_message de deconectare la fiecare client).

Per total, eu consider ca am avut destul de multe lucruri de invatat din tema asta.
Mi s-a parut destul de muncitoresc sa implementez toate hash table-urile acelea in C.

Ca si recomandare, as fi sugerat sa fie explicat mai mult ce se vrea de la noi in
legatura cu protocolul la nivel aplicatie, dar la un moment dat am inteles odata cu
intrebarile care au mai fost adresate pe forum

All in all, a fost o tema buna. Felicitari pentru ea si keep up the good work!

Verna Dorian-Alexandru
324CC