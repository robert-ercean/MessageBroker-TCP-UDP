
# Protocolul de nivel aplicatie pentru incadrarea mesajelor


Pentru a evita trimiterea unor "useless" bytes in cazul in care lungimea
payload-urilor pachetelor TCP difera, cel mai bun exemplu fiind diferenta
de lungime dintre un pachet cu un topic oarecare, si tip INT si un pachet
cu topic oarecare si tip STRING al carui payload poate ajunge si la 1501
caracatere (caz in care sirul nu este null-terminated); am creat protoculul
urmator care asigura trimiterea structurata a pachetelor in functie de 
lungimea payload-ului fiecaruia. Astfel, singura "lungime de pachet" care
este garantata ca va fi trimisa la orice "send" este lungimea in bytes a
header-ului protocolului implementat numit "tcp_hdr". Acesta are doua
campuri, unul care descrie tipul "actiunii" incapsulata de header, si alt
camp care reprezinta lungimea payload-ului pachet-ului. 


| Field  |           Meaning             |
|--------|-------------------------------|
| `action` |       tipul actiunii          |
| `len`    |      lungime payload          |

Campul "action" este
folosit atat in subscriber cat si in server pentru a stii cum se interpreteaza
si parseaza pachetul, iar campul length este folosit in operatiile de
`send_tcp_packet` si `recv_tcp_packet` care se ocupa cu trimiterea/primirea
fluxului de bytes intr-o maniera corecta. Astfel, avem:


- `SEND_TCP_PACKET` -  Pentru orice tip de pachet TCP, cand facem send este
garantat ca stim lungimea totala a pachetului intrucat pachetul este construit
in instanta de program de unde este trimis. Astfel, ca la laboratorul 7, intram 
intr-o bucla in care efectuam apelul de sistem send, retinem numarul de bytes trimisi
si iesim din bucla abia cand numarul total de bytes trimisi coincide tu suma dintre
header-ul tcp si lungimea payload-ului.


- `RECV_TCP_PACKET` -  Fata de operatia de send, la receive nu stim cati bytes trebuie
sa primim datorita diferitelor tipuri de payload-uri. Ce stim insa, este ca la orice tip
de pachet este garantat ca trebuie sa primi macar lungimea header-ului tcp, de unde putem
afla si lungimea payload-ului. Astfel, intram intr-o bucla unde contorizamt return-ul apelului
recv si iesim din aceasta bucla atunci numarul total de bytes primiti este egal cu lungimea
header-ului TCP. Dupa ce am obtinut intreg-ul header, aflam lungimea payload-ului din campul 
`len` al acestuia, iar apoi intram in cea de-a doua bucla care functioneaza la fel, doar ca 
ne oprim atunci cand primim `len` bytes.

## Structura protocolului si descrierea actiunilor


| Action type  |                                        Meaning                   |
|--------|------------------------------------------------------------------------|
|CONNECT_ACTION | Subscriber turned on his client, and wants to create a connection with the server |
|SHUTDOWN_CLOSE | Send/ received by both the server and client (details further) |
|SUBSCRIBE_ACTION | Subscriber want to sub to a new topic, send only by the subscriber to the server |
|NOTIFICATION_ACTION | Packet send to the subscriber only by the server, contains information |
|                    | about a subscriber's subbed topic|
|SHUTDOWN_INTRUDER | Send only by the server to a "wannabe" subscriber that tried to connect with an already
|                  | existing ID inside the server's database |
|UNSUBSCRIBE_ACTION | Send only by the subscriber to the server |


- `SHUTDOWN_CLOSE`
    - send by the `server` and received by the `subscriber` when the servers closes down, signals the subscriber
    to close his socket on his end, and then his client
    - send by the `client` and received by the `server` when the client receives the exit command from standard input,
    signals the server to shutdown and close the socket associated with that respective subscriber, mark that subscriber as offline inside its database and remove that socket FD from the server's file descriptors database, and only after that is done the subscriber can shutdown and close his socket as well, then exits his client.

- `SHUTDOWN_INTRUDER`
    - As explained above, this happens when a subscribers tries to connect with an already existing ID inside the server's database, and that ID is associated with an online user. In case that ID is associatd with an offline user, the server WILL NOT send this signal as it perceives it as just an user reconnecting. Decided to add this signal for 
    clarity of the code.
    

# Details about the server's database and flow


- The server holds multiple structures to keep track of each subscriber's info as follows: 


| Field  |                                        Meaning                   |
|--------|------------------------------------------------------------------------|
|FD | Holds the TCP SOCKET connection of that respective user |
|ID | Holds the uniqe ID of the user |
|TOPICS | String array containing each user's subbed topics |
|ONLINE | Flag indicatin whether the user is online / offline used in connection / reconnection checks|


- #### WILDCARD OPTION HANDLING
    - Even though it increases compiling time quite significantly, i chose to use the regex library
    for easy parsing between topics
    - You can find more details about this inside the code commentaries (`LINE 140`)

 
## Other


- `DISCLAIMER` Abia dupa ce am terminat README-ul mi-am dat seama ca l-am scris in romana, iar
comentariile din cod sunt in engleza, imi pare rau pentru inconsecventa :(
- As you can observe, i used a mix of C and C++ syntax. I initially started with C and then decided
to continue with C++ once I needed easier managing / iteration options of vectors.