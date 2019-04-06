#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <time.h>
#include <signal.h>

#include "socket.h"
#include "gameplay.h"


#ifndef PORT
    #define PORT 30001
#endif
#define MAX_QUEUE 5


void add_player(struct client **top, int fd, struct in_addr addr);
void remove_player(struct client **top, int fd);
/* Send the message in outbuf to all clients */
void broadcast(struct game_state *game, char *outbuf);
void announce_turn(struct game_state *game);
void announce_winner(struct game_state *game, struct client *winner);
/* Move the has_next_turn pointer to the next active client */
void advance_turn(struct game_state *game);
void disconnect_handler(struct game_state *game, int fd, char *name);

/* Broadcase the message outbuf to all the players inside the game
pointed to by the struct game_state pointer game. 
Precondition: outbuf is terminated by a network newline */
void broadcast(struct game_state *game, char *outbuf) {
    struct client *p;
    for (p = game->head; p != NULL; p = p->next) {
        int r = write(p->fd, outbuf, sizeof(char) * strlen(outbuf));
        // this player has left the game, so remove him/her
        // and broadcase this info to all other players
        if (r == -1) {
            char *name = malloc(strlen(p->name) + 1);
            strcpy(name, p->name);
            disconnect_handler(game, p->fd, name);
            free(name);
        }
    }
}

/* Announce the turn to all the players
   -If it is the player's turn write "Your guess?"
   -For the other players in the game, write
    "It's <someone>'s turn"*/
void announce_turn(struct game_state *game) {
    char *msg_nxt_player = "Your guess?\r\n";
    char msg_other_player[MAX_BUF];
    if (game->has_next_turn == NULL) {
        sprintf(msg_other_player, "There is currently no player\r\n");
    } else {
        sprintf(msg_other_player, "It's %s's turn\r\n", (game->has_next_turn)->name);
    }
    int nxt_player_fd = (game->has_next_turn)->fd;
    struct client *p;
    int r;
    for (p = game->head; p != NULL; p = p->next) {
        if (p->fd == nxt_player_fd) {
            r = write(p->fd, msg_nxt_player, sizeof(char) * strlen(msg_nxt_player));
        } else {
            r = write(p->fd, msg_other_player, sizeof(char) * strlen(msg_other_player));
        }
        if (r == -1) {
            char *name = malloc(strlen(p->name) + 1);
            strcpy(name, p->name);
            disconnect_handler(game, p->fd, name);
            free(name);
        }
    }
}

/* Anounnce the winner of the game to all of the players*/
void announce_winner(struct game_state *game, struct client *winner) {
    struct client *p;
    char msg_other[MAX_BUF];
    sprintf(msg_other, "Game over! %s won!\r\n", winner->name);
    char *msg_winner = "Game over! Congrats! You win!\r\n";
    for (p = game->head; p != NULL; p = p->next) {
        int r;
        if (p->fd == winner->fd) {
            r = write(p->fd, msg_winner, sizeof(char) * strlen(msg_winner));
        } else {
            r = write(p->fd, msg_other, sizeof(char) * strlen(msg_other));
        }
        if (r == -1) {
            char *name = malloc(strlen(p->name) + 1);
            strcpy(name, p->name);
            disconnect_handler(game, p->fd, name);
            free(name);
        }
    }
}

/*
 * Handle a disconnected player with file descriptor fd, if some 
 * read/write call failed. Also broadcast a goodbye information 
 * to all the players
 */
void disconnect_handler(struct game_state *game, int fd, char *name) {
    char msg[MAX_MSG];
    printf("%s has left\n", name);
    sprintf(msg, "Goodbye %s\r\n", name);
    broadcast(game, msg);
    if (game->has_next_turn != NULL && game->has_next_turn->fd == fd) {
        advance_turn(game);
        // game->has_next_turn = game->has_next_turn->next;
    }
    remove_player(&(game->head), fd);
    // if (game->has_next_turn != NULL) {
    //     announce_turn(game);
    // }
    announce_turn(game);
    // we shall not announce this if the game has no player
}

/* Move the game state forward. Note that this function handles the case
 * where some player left the server, in which case we should handle to 
 * skip that player. We shall make the has_next_turn point to a player
 * that is still online and about to play. In the case of a player left
 * the game, we shall delete the player from the current linked-list. If
 * we reached the end of the linked list, we shall start again from the
 * of the linked_list*/
void advance_turn(struct game_state *game) {
    struct client *cur = game->has_next_turn;
    if (cur->next != NULL) {
        game->has_next_turn = cur->next;
    } else {
        game->has_next_turn = game->head;
    }
}

/* The set of socket descriptors for select to monitor.
 * This is a global variable because we need to remove socket descriptors
 * from allset when a write to a socket fails.
 */
fd_set allset;


/* Add a client to the head of the linked list
 */
void add_player(struct client **top, int fd, struct in_addr addr) {
    struct client *p = malloc(sizeof(struct client));

    if (!p) {
        perror("malloc");
        exit(1);
    }

    // TODO: printf("Adding client %s\n", inet_ntoa(addr));

    p->fd = fd;
    p->ipaddr = addr;
    p->name[0] = '\0';
    p->in_ptr = p->inbuf;
    memset(p->inbuf, '\0', MAX_BUF);
    p->next = *top;
    *top = p;
}

/* Removes client from the linked list and closes its socket.
 * Also removes socket descriptor from allset 
 */
void remove_player(struct client **top, int fd) {
    struct client **p;

    for (p = top; *p && (*p)->fd != fd; p = &(*p)->next);
    // Now, p points to (1) top, or (2) a pointer to another client
    // This avoids a special case for removing the head of the list
    if (*p) {
        struct client *t = (*p)->next;
        // TODO: printf("Removing client %d %s\n", fd, inet_ntoa((*p)->ipaddr));
        FD_CLR((*p)->fd, &allset);
        close((*p)->fd);
        free(*p);
        *p = t;
    } else {
        fprintf(stderr, "Trying to remove fd %d, but I don't know about it\n",
                fd);
    }
}

/* Remove the node with fd field fd in the linked list pointed to by
 * the head, which is a pointer to the struct client. We want to change
 * the original struct, and this is why we are passing the pointer in.
 * This function returns the new head of the linked list, to handle the
 * case that we are trying to delete the first element of the list
 */
void remove_from_newplayers(struct client **head, int fd) {
    printf("[%d] removed from the new player list\n", fd);
    struct client *temp = *head, *prev;
    if (temp != NULL && temp->fd == fd) {
        *head = temp->next;
        return;
    }
    while (temp != NULL && temp->fd != fd) {
        prev = temp;
        temp = temp->next;
    }
    if (temp == NULL) {
        return;
    }
    prev->next = temp->next;
}

/*
 * Search the first n characters of buf for a network newline (\r\n).
 * Return the index of the '\n' plus one of the first network newline,
 * or -1 if no network newline is found. This will be equiv to the length
 * of the entire string, including the \r\n part.
 * Definitely do not use strchr or other string functions to search here. (Why not?)
 * I think the reason is that things are not null-terminated
 */
int find_network_newline(const char *buf, int n) {
    // At lease enough space that's needed for the first \r\n
    if (n < 2) {
        return -1;
    }
    for (int i = 0; i < n - 1; i++) {
        // If we can find an macthing \r\n segment.
        if (buf[i] == '\r' && buf[i + 1] == '\n') {
            return i + 2;
        }
    }
    return -1;
}

void *Malloc(size_t size) {
    void *pt = malloc(size);
    if (pt == NULL) {
        perror("malloc");
        exit(1);
    }
    return pt;
}

/* Read from client cur_client, into the inbuf of the client
 * This function resets the in_ptr field after reading, and uses the inbuf
 * field of the cur_client as the buffer
 * RETURN VALUES:
 *  >= 0 -----  the length read
 *  -1 -------  the \r\n is not yet found
 *  -2 -------  the client has disconnected
 */
int read_from_client(struct client *cur_client, int max_len) {
    int r;
    int length;
    r = read(cur_client->fd, cur_client->in_ptr, max_len);
    if (r == -1) {
        perror("read");
        exit(1);
    } else if (r == 0) {
        return -2;
    }
    printf("[%d] Read %d bytes\n", cur_client->fd, r);
    cur_client->in_ptr = &(cur_client->in_ptr[r]);
    // The line has finished!
    if ((length = find_network_newline(cur_client->inbuf, MAX_BUF)) > -1) {
        cur_client->inbuf[length - 2] = '\0';
        cur_client->in_ptr = &(cur_client->inbuf[0]);
        // Since we have null terminated the string, we can use the print
        printf("[%d] Found new line %s\n", cur_client->fd, cur_client->inbuf);
        return length - 2;
    }
    return -1;
}

int main(int argc, char **argv) {
    /* Handler for SIGPIPE, which causes the code to stop
     * we will use this to determine whether a user has dc'ed or not
     */
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGPIPE, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    int clientfd, maxfd, nready;
    struct client *p;
    struct sockaddr_in q;
    fd_set rset;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <dictionary filename>\n", argv[0]);
        exit(1);
    }

    // Create and initialize the game state
    struct game_state game;

    srandom((unsigned int) time(NULL));
    // Set up the file pointer outside of init_game because we want to 
    // just rewind the file when we need to pick a new word
    game.dict.fp = NULL;
    // This is the number of words that is in our dictionary
    game.dict.size = get_file_length(argv[1]);
    // We pass the game state struct and the dictionary file to init the game
    init_game(&game, argv[1]);

    // head and has_next_turn also don't change when a subsequent game is
    // started so we initialize them here.
    game.head = NULL;
    game.has_next_turn = NULL;

    /* A list of client who have not yet entered their name.  This list is
     * kept separate from the list of active players in the game, because
     * until the new playrs have entered a name, they should not have a turn
     * or receive broadcast messages.  In other words, they can't play until
     * they have a name.
     */
    struct client *new_players = NULL;

    struct sockaddr_in *server = init_server_addr(PORT);
    int listenfd = set_up_server_socket(server, MAX_QUEUE);

    // initialize allset and add listenfd to the
    // set of file descriptors passed into select
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);
    // maxfd identifies how far into the set to search
    maxfd = listenfd;

    while (1) {
        // make a copy of the set before we pass it into select
        rset = allset;
        nready = select(maxfd + 1, &rset, NULL, NULL, NULL);
        if (nready == -1) {
            perror("select");
            continue;
        }

        /* FD_ISSET() tests to see if a file descriptor is
        part of the set; this is useful after select() returns. */
        if (FD_ISSET(listenfd, &rset)) {
            printf("A new client is connecting\n");
            clientfd = accept_connection(listenfd);
            /* FD_SET() and FD_CLR() respectively add and remove  a  given  file
            descriptor from a set. */
            FD_SET(clientfd, &allset);
            if (clientfd > maxfd) {
                maxfd = clientfd;
            }
            // TODO: printf("Connection from %s\n", inet_ntoa(q.sin_addr));
            // we have a new connection, so add the player into the waiting area
            // Until the player enters an legitimate name, we cannot let the player
            // participate in the game
            add_player(&new_players, clientfd, q.sin_addr);
            char *greeting = WELCOME_MSG;
            /* If we cannot write to the client's file descriptor, we rmove the
            client from the player list */
            if (write(clientfd, greeting, strlen(greeting)) == -1) {
                // This handles the case that the player disconnects before entering name
                fprintf(stderr, "Write to client %s failed\n", inet_ntoa(q.sin_addr));
                remove_player(&new_players, p->fd); // Fixed as per in the email
            };
        }

        /* Check which other socket descriptors have something ready to read.
         * The reason we iterate over the rset descriptors at the top level and
         * search through the two lists of clients each time is that it is
         * possible that a client will be removed in the middle of one of the
         * operations. This is also why we call break after handling the input.
         * If a client has been removed the loop variables may not longer be 
         * valid.
         */
        int cur_fd;
        for (cur_fd = 0; cur_fd <= maxfd; cur_fd++) {
            if (FD_ISSET(cur_fd, &rset)) {
                // Check if this socket descriptor is an active player

                for (p = game.head; p != NULL; p = p->next) {
                    if (cur_fd == p->fd) {
                        int inlen = read_from_client(p, MAX_BUF);
                        // we have a disconnection
                        int has_disconnect = 0;
                        if (inlen == -2) {
                            char *name = malloc(strlen(p->name) + 1);
                            strcpy(name, p->name);
                            disconnect_handler(&game, cur_fd, name);
                            free(name);
                            has_disconnect = 1;
                        }
                        // input confirmed and the player is still online
                        if (inlen != -1 && has_disconnect == 0) {
                            int position = (int) p->inbuf[0] - 97;
                            // CASE ONE: player mistypes during other players' turn
                            if (game.has_next_turn->fd != cur_fd) {
                                char *msg = "It is not yet your turn!\r\n";
                                printf("[%d] player input not during their turn\n", cur_fd);
                                memset(p->inbuf, '\0', MAX_BUF);
                                if (write(cur_fd, msg, strlen(msg)) == -1) {
                                    char *name = malloc(strlen(p->name) + 1);
                                    strcpy(name, p->name);
                                    disconnect_handler(&game, p->fd, name);
                                    free(name);
                                }
                            } else {
                                // CASE TWO: palyer's turn
                                //  - SUBCASE ONE: The player's guess was empty/multiple char
                                if (strlen(p->inbuf) != 1 || position < 0 || position > 26) {
                                    char *msg = "Your guess is not valid, please try again:\r\n";
                                    if (write(cur_fd, msg, strlen(msg)) == -1) {
                                        char *name = malloc(strlen(p->name) + 1);
                                        strcpy(name, p->name);
                                        disconnect_handler(&game, p->fd, name);
                                        free(name);
                                    }
                                    printf("[%d] player input empty/too long\n", cur_fd);
                                    memset(p->inbuf, '\0', MAX_BUF);
                                    //  - SUBCASE TWO: valid, proceed the game
                                } else {
                                    // if the letter was already guessed
                                    if (game.letters_guessed[position] == 1) {
                                        char *msg = "That was already guessed, try again:\r\n";
                                        if (write(cur_fd, msg, strlen(msg)) == -1) {
                                            char *name = malloc(strlen(p->name) + 1);
                                            strcpy(name, p->name);
                                            disconnect_handler(&game, p->fd, name);
                                            free(name);
                                        }
                                        printf("[%d] player guessed existing letter\n", cur_fd);
                                        memset(p->inbuf, '\0', MAX_BUF);
                                        // if the letter was not in the word
                                    } else if (strstr(game.word, p->inbuf) == NULL) {
                                        char *msg = "Your guess was not in the word\r\n";
                                        if (write(cur_fd, msg, strlen(msg)) == -1) {
                                            char *name = malloc(strlen(p->name) + 1);
                                            strcpy(name, p->name);
                                            disconnect_handler(&game, p->fd, name);
                                            free(name);
                                        }
                                        printf("[%d] player guessed wrong\n", cur_fd);
                                        game.letters_guessed[position] = 1;
                                        advance_turn(&game);
                                        game.guesses_left -= 1;
                                        int game_fin = 0;
                                        if (game.guesses_left == 0) {
                                            char msg[MAX_MSG];
                                            sprintf(msg, "%s used up all the guesses, you lost!\r\n", p->name);
                                            broadcast(&game, msg);
                                            broadcast(&game, "Let's start a new game!\r\n");
                                            init_game(&game, argv[1]);
                                            char new_status[MAX_MSG];
                                            status_message(new_status, &game);
                                            broadcast(&game, new_status);
                                            announce_turn(&game);
                                            game_fin = 1;
                                            printf("The game trials has been exausted\n");
                                        }
                                        if (game_fin == 0) {
                                            char status[MAX_MSG];
                                            status_message(status, &game);
                                            broadcast(&game, status);
                                            announce_turn(&game);
                                        }
                                        // the letter is in the word
                                    } else {
                                        game.letters_guessed[position] = 1;
                                        for (int i = 0; i < strlen(game.guess); i++) {
                                            if (game.guess[i] == '-' && game.word[i] == p->inbuf[0]) {
                                                game.guess[i] = p->inbuf[0];
                                            }
                                        }
                                        // the game has end
                                        if (strcmp(game.guess, game.word) == 0) {
                                            announce_winner(&game, p);
                                            printf("%s has won, starting a new game\n", p->name);
                                            broadcast(&game, "Let's start a new game!\r\n");
                                            init_game(&game, argv[1]);
                                            char new_status[MAX_MSG];
                                            status_message(new_status, &game);
                                            broadcast(&game, new_status);
                                            announce_turn(&game);
                                            // the game proceeds, the player guesses again
                                        } else {
                                            broadcast(&game, "Good guess!\r\n");
                                            char msg[MAX_MSG];
                                            status_message(msg, &game);
                                            broadcast(&game, msg);
                                            announce_turn(&game);
                                            printf("[%d] guessed correctly, guessing again\n", cur_fd);
                                        }
                                    }
                                }
                            }
                        }
                        break;
                    }
                }

                // Check if any new players are entering their names
                for (p = new_players; p != NULL; p = p->next) {
                    // cur_fd is the file descriptor that we are interested in
                    if (cur_fd == p->fd) {
                        int name_len = read_from_client(p, MAX_NAME);
                        if (name_len == 0) {
                            char *msg = "The user name that you entered was empty, please try again: \r\n";
                            printf("[%d] entered an empty name\n", cur_fd);
                            if (write(cur_fd, msg, strlen(msg)) == -1) {
                                // fprintf(stderr, "Write to client %s failed\n", inet_ntoa(q.sin_addr));
                                remove_player(&new_players, p->fd);
                            }
                            memset(p->inbuf, '\0', MAX_BUF);
                            p->in_ptr = &(p->inbuf[0]);
                        } else if (name_len > 0) {
                            struct client *cache = game.head;
                            int duplicate_name = 0;
                            for (; cache != NULL; cache = cache->next) {
                                if (strcmp(cache->name, p->inbuf) == 0 && cache->fd != -1) {
                                    duplicate_name = 1;
                                    char *msg = "The user name that you entered was taken, please try again: \r\n";
                                    printf("[%d] entered an existing name\n", cur_fd);
                                    if (write(cur_fd, msg, strlen(msg)) == -1) {
                                        // fprintf(stderr, "Write to client %s failed\n", inet_ntoa(q.sin_addr));
                                        remove_player(&new_players, p->fd);
                                    }
                                    memset(p->inbuf, '\0', MAX_BUF);
                                    // the name alraedy exists
                                }
                            }
                            // so the name is valid if it reaches here
                            if (duplicate_name == 0) {
                                // set fields as appropriate
                                strncpy(p->name, p->inbuf, MAX_NAME);
                                // p->name should be null terminated, but we should be carefull with the size
                                p->name[MAX_NAME - 1] = '\0';
                                printf("[%d] %s has joined the game\n", p->fd, p->name);
                                // update the new_players
                                remove_from_newplayers(&new_players, cur_fd);
                                p->next = game.head;
                                game.head = p;
                                // so if this is a fresh new game
                                if (game.has_next_turn == NULL) {
                                    game.has_next_turn = p;
                                }
                                memset(p->inbuf, '\0', MAX_BUF);
                                // The new player has joined, report the status of the game to the new player
                                char *msg = Malloc(MAX_MSG);
                                sprintf(msg, "%s has just joined the game, hello there!\r\n", p->name);
                                broadcast(&game, msg);
                                memset(msg, '\0', MAX_MSG);
                                msg = status_message(msg, &game);
                                if (write(cur_fd, msg, strlen(msg)) == -1) {
                                    char *name = malloc(strlen(p->name) + 1);
                                    strcpy(name, p->name);
                                    disconnect_handler(&game, p->fd, name);
                                    free(name);
                                }
                                free(msg);
                                announce_turn(&game);
                                break;
                            }
                            memset(p->inbuf, '\0', MAX_BUF);
                        } else if (name_len == -2) {
                            remove_from_newplayers(&new_players, cur_fd);
                        }
                    }
                }
            }
        }
    }
    return 0;
}
