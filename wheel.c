#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "family.h"
#include "reading.h"

#define BUF_SIZE    256

/* Starting with words (returned by read_words), generate and return
   a new word list with only those words of length len. Also, fill
   words_remaining with the number of words in the new word list.
*/
char **prune_word_list(char **words, int len, int *words_remaining) {
    char **word_list;
    int cache_index = 0;
	*words_remaining = 0;
    char *word = words[cache_index];
    while(word != NULL){
        if (strlen(word) == len){
            *words_remaining += 1;
        }
        cache_index += 1;
        word = words[cache_index];
    }

    word_list = malloc(sizeof(char *) * (*words_remaining + 1));
    if (word_list == NULL){
        perror("malloc error in prune_word_list");
        exit(1);
    }
    // SET THE LAST TO NULL
    word_list[*words_remaining] = NULL;

    int i = 0;
    word = words[0];
    cache_index = 0;
    while (word != NULL){
        if(strlen(word) == len){
            word_list[i] = word;
            i += 1;
        }
        cache_index += 1;
        word = words[cache_index];
    }
	word_list[*words_remaining] = NULL;
    return word_list;
}


/* Free memory acquired by prune_word_list.
*/
void deallocate_pruned_word_list(char **word_list) {
    free(word_list);
}


/* Return the word_list of all length-L words, and store that length in len.
   - ask user for the length of words to use
   - use prune_word_list to get a list of words of the appropriate length
   - if there are no words of this length in word list, then ask user to
    provide a different word length until there is at least one word of that
    length in words.

   If the user enters a non-numeric string, then ask the user to provide
   a different word length as above.
*/
char **get_word_list_of_length(char **words, int *len) {
    char buffer[BUF_SIZE];
    char *remainder;
    printf("Length of words to use? ");
    if (fgets(buffer, BUF_SIZE, stdin) == NULL){
        perror("fgets failed in get_word_list_of_length");
        exit(1);
    }
    *len = strtol(buffer, &remainder, 10);
    if (*remainder != '\n'){ // The input was not all numeric
        return get_word_list_of_length(words, len);
    }
	/* signature reminder: char** prune_word_list(char**, int, int *)  */
	int family_member_count;
    char **word_list = prune_word_list(words, *len, &family_member_count);   
	if(family_member_count < 1){
        deallocate_pruned_word_list(word_list);
        printf("There are no words of that length.\n");
        return get_word_list_of_length(words, len);
    }
    return word_list;
}


/* Continue to ask the user for their next guess, until it
   does not appear in letters_guessed. Add the new guess to letters_guessed,
   and return the guess.
*/
char get_next_guess(char *letters_guessed) {
    char guess;
    int i;

    do {
        printf("Next guess? ");
        fflush(stdout);

        if (scanf(" %c", &guess) != 1) {
            perror("scanf");
            exit(1);
        }

        if (guess >= 'a' && guess <= 'z') {
            i = guess - 'a';
            if (letters_guessed[i])
                printf("You already chose that letter.\n");
            else {
                letters_guessed[i] = guess;
                break;
            }
        } else
            printf("Please insert a lowercase letter.\n");

    } while (1);

    return guess;
}


/*Play one game of desperate_hangman */
void play_round(char **words) {
    Family *famlist = NULL, *biggest_fam;
    char input_buffer[BUF_SIZE];
    char **word_list = NULL;
    int len, i, found;
    int guesses = 0;
    int game_over = 0; /*1 = game is over*/
    char guess;
    char *current_word; /*Representation of current word; each blank is a - */
    char *sig; /*Signature of a family*/
    char letters_guessed[26] = {'\0'}; /*Guesses so far*/
    
    /*Get a valid word_list from length (one that has at least one word)*/
    word_list = get_word_list_of_length(words, &len);

    while (guesses < 1 || guesses > 26) {
        printf("How many guesses would you like?\n");
        printf("Choose a number between 1 and 26: ");
        fflush(stdout);

        if (fgets(input_buffer, BUF_SIZE, stdin) == NULL) {
            perror("fgets");
            exit(1);
        }

        guesses = strtol(input_buffer, NULL, 10);
        if (guesses < 1 || guesses > 26) {
            printf("You entered an invalid number!\n\n");
    	}
    }

    /*Word starts off as all unknowns*/
    current_word = malloc(len + 1);
    if (current_word == NULL) {
        perror("malloc");
        exit(1);
    }
    memset(current_word, '-', len);
    current_word[len] = '\0';

    while (!game_over) {
        printf("Guesses remaining: %d\n", guesses);
        printf("Word: %s\n", current_word);
        guess = get_next_guess(letters_guessed);
        deallocate_families(famlist);
        famlist = generate_families(word_list, guess);
        biggest_fam = find_biggest_family(famlist);
        sig = get_family_signature(biggest_fam);
        /*Search signature for letters in current_word*/
        found = 0;
        i = 0;
        while (*sig) {
            if (*sig == guess) {
                found = 1;
                current_word[i] = guess;
            }
            sig++;
            i++;
        }
        if (found) {
            printf("Good guess!\n");
            if (!strchr(current_word, '-')) {
                printf("You win! The word was %s.\n", current_word);
                game_over = 1;
            }
        }
        else {
            printf("There is no %c in the word.\n", guess);
            guesses--;
            game_over = guesses <= 0;
        }
        deallocate_pruned_word_list(word_list);
        word_list = get_new_word_list(biggest_fam);
    }

    if (guesses == 0) {
        printf("You lose! The word was %s.\n",
                get_random_word_from_family(biggest_fam));
    }
    
    deallocate_pruned_word_list(word_list);
    free(current_word);
    deallocate_families(famlist);
}


/* Read words, initialize families, and play as long as
   the user answers 'y'. */
int main(void) {
    char again;
    char **words;
    
    words = read_words("dictionary.txt");
    init_family(1024);

    do {
        play_round(words);
        printf("Play another round (y/n)? ");
        if (scanf(" %c", &again) != 1) {
            perror("scanf");
            break;
        }
        // In case of valid input, "consume" the newline character.
        getchar();

    } while (again == 'y');
  
    deallocate_words(words);
    return 0;
}
