# desperate_hangman
A terminal text based implementation of hangman, with a twist!
## Version one: Single Player (Local)

### DESPERATE?
Under this implementation, the word is not fixed! As the game progress, the computer opponent will delibrately choose wordlist that doesn't contain any character that you have already tried! 

### How to play
Clone this repository with ```$ git clone```. Then go the new directory and ```cd``` into the ```Version-Local``` and type ```
$ make``` this will invoke the Makefile to compile the game using ```gcc```. You can then play the game by executing ```
$ ./wheel``` and follow the command line prompts to play the game!

## Version two: Multiplayer (Online)

### How to play
Clone this repository with ```$ git clone```Then go the new directory and ```cd``` into the ```Version-Multiplayer``` and type```$ make```this will invoke the Makefile to compile the game using ```gcc```. You can then start the server using```$ ./wordsrv```. Now fireup another terminal window and start netcat by calling ```nc -C localhost <port>``` where ```-C``` forces the use of network newline which is essential to the backend logic so make sure you put this flag. The port was set to ```30001``` by default but of course you can change it as you wish, just be sure you connect to the right port when using netcat.

# Have fun!

